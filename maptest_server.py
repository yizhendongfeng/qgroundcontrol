"""Local stub for the DJI Cloud API map-element feature: WebSocket push + REST on one port.

Why one port: `CloudServerSettings.websocketUrl` determines BOTH the ws endpoint and the
REST base (DjiCloudMapClient derives the http base from it by swapping the scheme and
clearing the path), so pointing that one setting at ws://127.0.0.1:8765/api/v1/ws makes
the ws and every REST call land here.

WebSocket side pushes the four map biz_codes on a timer so the downlink path can be
watched live.  HTTP side serves element-groups CRUD and prints every body it receives so
the uplink payloads can be checked by eye.

No auth: `x-auth-token` is accepted and reported as set/unset only -- the value is a real
platform token and must never be echoed.
"""
import base64
import hashlib
import json
import os
import re
import socket
import struct
import sys
import threading
import time
import uuid

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

# Windows 控制台默认 GBK，日志里的中文会抛 UnicodeEncodeError 把响应打断（客户端看到空回复）。
# 强制 UTF-8 + replace：编码不了也不崩。
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

# 后端约定：Pilot 默认往 type==2 的 APP 共享组里加元素
SHARED_GROUP_ID = "grp-shared-0001"
WORKSPACE_FALLBACK = "ws-local"

# 深圳南山一带，跟飞行视图默认位置对得上
LAT, LON = 22.532, 113.930

_lock = threading.Lock()
_elements = {}      # id -> element json (含 resource)
_ws_conns = []      # 已握手的连接，方便 REST 收完就回推


def log(*parts):
    print("[%s] %s" % (time.strftime("%H:%M:%S"), " ".join(str(p) for p in parts)), flush=True)


# ---------------------------------------------------------------------------
# 元素构造
# ---------------------------------------------------------------------------

def _content(geom_type, coordinates, color):
    return {
        "type": "Feature",
        "properties": {"color": color, "clampToGround": True},
        "geometry": {"type": geom_type, "coordinates": coordinates},
    }


def line_element(eid, name, color="#2D8CF0"):
    """LineString：coordinates 是 [[lon,lat], ...]，两点以上"""
    return {
        "id": eid,
        "name": name,
        "create_time": int(time.time() * 1000) - 60000,
        "update_time": int(time.time() * 1000) - 60000,
        "resource": {
            "type": 1,
            "user_name": "pilot",
            "content": _content("LineString", [
                [LON - 0.006, LAT - 0.004],
                [LON - 0.001, LAT + 0.001],
                [LON + 0.004, LAT - 0.001],
                [LON + 0.008, LAT + 0.003],
            ], color),
        },
    }


def area_element(eid, name, color="#19BE6B"):
    """Polygon：coordinates 外面比 LineString 多一层（只取第 0 环）"""
    ring = [
        [LON - 0.007, LAT + 0.006],
        [LON + 0.001, LAT + 0.006],
        [LON + 0.001, LAT + 0.012],
        [LON - 0.007, LAT + 0.012],
    ]
    return {
        "id": eid,
        "name": name,
        "create_time": int(time.time() * 1000) - 60000,
        "update_time": int(time.time() * 1000) - 60000,
        "resource": {
            "type": 2,
            "user_name": "pilot",
            "content": _content("Polygon", [ring], color),
        },
    }


def seed():
    with _lock:
        for e in (line_element("11111111-1111-4111-8111-111111111111", "平台线段A"),
                  area_element("22222222-2222-4222-8222-222222222222", "平台区域B")):
            _elements[e["id"]] = e


# ---------------------------------------------------------------------------
# 帧收发
# ---------------------------------------------------------------------------

def send_text(conn, payload):
    data = payload.encode("utf-8")
    header = bytearray([0x81])
    if len(data) < 126:
        header.append(len(data))
    elif len(data) < 65536:
        header.append(126)
        header += struct.pack(">H", len(data))
    else:
        header.append(127)
        header += struct.pack(">Q", len(data))
    with _lock:
        conn.sendall(bytes(header) + data)


def envelope(biz_code, data):
    return json.dumps({
        "biz_code": biz_code,
        "version": "1.0",
        "timestamp": int(time.time() * 1000),
        "data": data,
    })


def push(biz_code, data):
    """往所有活着的 ws 连接推一条"""
    payload = envelope(biz_code, data)
    for conn in list(_ws_conns):
        try:
            send_text(conn, payload)
            log("PUSH", biz_code, json.dumps(data, ensure_ascii=False)[:160])
        except OSError:
            with _lock:
                if conn in _ws_conns:
                    _ws_conns.remove(conn)


def read_frames(conn):
    """只为了保证连接活着：回 pong，文本帧打出来。"""
    try:
        while True:
            head = conn.recv(2)
            if len(head) < 2:
                return
            opcode = head[0] & 0x0F
            masked = head[1] & 0x80
            length = head[1] & 0x7F
            if length == 126:
                length = struct.unpack(">H", conn.recv(2))[0]
            elif length == 127:
                length = struct.unpack(">Q", conn.recv(8))[0]
            mask = conn.recv(4) if masked else b"\x00\x00\x00\x00"
            payload = b""
            while len(payload) < length:
                chunk = conn.recv(length - len(payload))
                if not chunk:
                    return
                payload += chunk
            if masked:
                payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
            if opcode == 0x8:
                return
            if opcode == 0x9:
                with _lock:
                    conn.sendall(b"\x8a" + bytes([len(payload)]) + payload)
            elif opcode == 0x1:
                log("RECV", payload.decode("utf-8", "replace")[:200])
    except OSError:
        return


# ---------------------------------------------------------------------------
# 定时推送：把增/改/删/刷新四条路都走一遍
# ---------------------------------------------------------------------------

def scenario():
    # MAPTEST_SCENARIO=create,update,delete,refresh 可只跑其中几步；不设则全跑，设成 none 就一步都不跑。
    # 用来把客户端崩溃点隔离到某一条推送路径上。
    want = os.environ.get("MAPTEST_SCENARIO", "create,update,delete,refresh")
    steps = [s.strip() for s in want.split(",") if s.strip()]

    if "weird" in steps:
        scenario_weird()
        return

    time.sleep(3)
    if "create" in steps:
        new_id = str(uuid.uuid4())
        with _lock:
            _elements[new_id] = line_element(new_id, "平台新推线段", "#FFBB00")
            created = _elements[new_id]
        push("map_element_create", dict(created, group_id=SHARED_GROUP_ID))

    time.sleep(6)
    updated = None
    if "update" in steps:
        with _lock:
            e = _elements.get("11111111-1111-4111-8111-111111111111")
            if e:
                e["name"] = "平台线段A改名"
                e["update_time"] = int(time.time() * 1000)
                e["resource"]["content"]["properties"]["color"] = "#E23C39"
                e["resource"]["content"]["geometry"]["coordinates"] = [
                    [LON - 0.010, LAT + 0.003],
                    [LON - 0.002, LAT + 0.008],
                    [LON + 0.006, LAT + 0.002],
                ]
                updated = json.loads(json.dumps(e))
    if updated:
        push("map_element_update", dict(updated, group_id=SHARED_GROUP_ID))

    time.sleep(6)
    if "delete" in steps:
        with _lock:
            _elements.pop("22222222-2222-4222-8222-222222222222", None)
        push("map_element_delete", {"group_id": SHARED_GROUP_ID,
                                    "id": "22222222-2222-4222-8222-222222222222"})

    time.sleep(5)
    if "refresh" in steps:
        # 后端批量改过（网页端拖拽）→ 只给组 id，客户端必须重新 GET
        push("map_group_refresh", {"ids": [SHARED_GROUP_ID]})


def _raw_element(eid, name, resource):
    """只带协议字段的元素条目（resource 原样给，方便塞畸形值）"""
    now = int(time.time() * 1000)
    return {"id": eid, "name": name, "create_time": now, "update_time": now, "resource": resource}


def scenario_weird():
    """把协议允许、真机上见过、但普通桩没覆盖的畸形 payload 全推一遍。

    每一条都对应一个"平台画的图形本地显示不一样"或一句 QList::operator[] 断言的嫌疑。
    跑法：MAPTEST_SCENARIO=weird
    """
    time.sleep(3)
    pushes = []

    # 1) 闭合的 Polygon 环（GeoJSON 语义，首尾同点）：本地应去掉重复点，不该多一个拖拽手柄
    ring = [[LON - 0.007, LAT + 0.006], [LON + 0.001, LAT + 0.006],
            [LON + 0.001, LAT + 0.012], [LON - 0.007, LAT + 0.012],
            [LON - 0.007, LAT + 0.006]]
    pushes.append(("map_element_create", _raw_element(str(uuid.uuid4()), "闭合环区域",
        {"type": 2, "user_name": "pilot", "content": _content("Polygon", [ring], "#FFBB00")})))

    # 2) Polygon 少一层嵌套：[[lon,lat],...] 而不是 [[[lon,lat],...]]
    flat = [[LON + 0.010, LAT + 0.006], [LON + 0.018, LAT + 0.006], [LON + 0.018, LAT + 0.012]]
    pushes.append(("map_element_create", _raw_element(str(uuid.uuid4()), "扁平嵌套区域",
        {"type": 2, "user_name": "pilot", "content": _content("Polygon", flat, "#19BE6B")})))

    # 3) content 被二次编码成 JSON 字符串（后端把 content 当 String 存/转发）
    pushes.append(("map_element_create", _raw_element(str(uuid.uuid4()), "字符串content线段",
        {"type": 1, "user_name": "pilot",
         "content": json.dumps(_content("LineString",
             [[LON - 0.006, LAT - 0.010], [LON + 0.004, LAT - 0.006]], "#B620E0"))})))

    # 4) resource.type 缺失，只能靠 content.geometry.type 判
    pushes.append(("map_element_create", _raw_element(str(uuid.uuid4()), "缺type线段",
        {"user_name": "pilot", "content": _content("LineString",
            [[LON - 0.012, LAT - 0.002], [LON - 0.004, LAT + 0.004]], "#E23C39")})))

    # 5) 1 个点的 LineString / 2 个点的 Polygon：退化几何必须被丢掉，不能喂给 visuals
    pushes.append(("map_element_create", _raw_element(str(uuid.uuid4()), "单点线段",
        {"type": 1, "user_name": "pilot",
         "content": _content("LineString", [[LON, LAT]], "#212121")})))
    pushes.append(("map_element_create", _raw_element(str(uuid.uuid4()), "两点区域",
        {"type": 2, "user_name": "pilot",
         "content": _content("Polygon", [[[LON, LAT], [LON + 0.001, LAT + 0.001]]], "#212121")})))

    # 6) Point(0)：本次范围外，应该只记一行日志、地图上什么都不画
    pushes.append(("map_element_create", _raw_element(str(uuid.uuid4()), "平台端点",
        {"type": 0, "user_name": "pilot", "content": _content("Point", [LON, LAT], "#2D8CF0")})))

    # 7) 一口气来好几条 —— "我添加几个元素就崩溃了"就是这个场景
    churn = []
    for i in range(5):
        eid = str(uuid.uuid4())
        churn.append(eid)
        pushes.append(("map_element_create", _raw_element(eid, "批量线段%d" % (i + 1),
            {"type": 1, "user_name": "pilot", "content": _content("LineString",
                [[LON - 0.020 + i * 0.004, LAT - 0.020],
                 [LON - 0.020 + i * 0.004, LAT - 0.010]], "#2D8CF0")})))

    for biz, data in pushes:
        push(biz, dict(data, group_id=SHARED_GROUP_ID))
        time.sleep(0.6)

    # 8) 删中间几条再补几条：删+增交错最容易踩到"visuals 还拿着已经删掉的几何"
    for eid in churn[1:4]:
        push("map_element_delete", {"group_id": SHARED_GROUP_ID, "id": eid})
        time.sleep(0.3)
    for i in range(3):
        push("map_element_create", dict(_raw_element(str(uuid.uuid4()), "补推线段%d" % (i + 1),
            {"type": 1, "user_name": "pilot", "content": _content("LineString",
                [[LON + 0.020, LAT - 0.020 + i * 0.004],
                 [LON + 0.030, LAT - 0.020 + i * 0.004]], "#19BE6B")}),
            group_id=SHARED_GROUP_ID))
        time.sleep(0.3)

    time.sleep(3)
    push("map_group_refresh", {"ids": [SHARED_GROUP_ID]})


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------

def _json_response(conn, obj, status="200 OK"):
    body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
    conn.sendall((
        "HTTP/1.1 %s\r\n"
        "Content-Type: application/json;charset=UTF-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n" % (status, len(body))
    ).encode() + body)


def handle_rest(conn, method, path, headers, body):
    try:
        parsed = json.loads(body) if body else {}
    except ValueError:
        parsed = {"<unparsable>": body[:400].decode("utf-8", "replace")}

    log("HTTP", method, path,
        "token:", "<set>" if headers.get("x-auth-token") else "<missing>")

    groups_m = re.match(r"^/map/api/v1/workspaces/([^/]+)/element-groups$", path)
    create_m = re.match(r"^/map/api/v1/workspaces/([^/]+)/element-groups/([^/]+)/elements$", path)
    elem_m = re.match(r"^/map/api/v1/workspaces/([^/]+)/elements/([^/]+)$", path)

    if groups_m and method == "GET":
        with _lock:
            elements = list(_elements.values())
        log("  -> GET 返回 %d 个元素" % len(elements))
        _json_response(conn, {
            "code": 0, "message": "success",
            "data": [{
                "id": SHARED_GROUP_ID, "name": "共享图层", "type": 2,
                "is_lock": False, "elements": elements,
            }],
        })
        return

    if create_m and method == "POST":
        # CreateMapElementRequest：id 由客户端生成（@NotNull uuid），没有 resource 包装，
        # 是 {id, name, resource:{type, content:{...}}} 这种扁平结构
        eid = parsed.get("id")
        log("  -> POST body:", json.dumps(parsed, ensure_ascii=False))
        resource = parsed.get("resource") or {}
        geom = (resource.get("content") or {}).get("geometry") or {}
        log("  -> 校验 id=%s 非空=%s / resource.type=%s / geometry.type=%s / color=%s / 顶点=%s"
            % (eid, bool(eid), resource.get("type"), geom.get("type"),
               ((resource.get("content") or {}).get("properties") or {}).get("color"),
               len(geom.get("coordinates") or [])))
        if not eid or not resource:
            _json_response(conn, {"code": 400, "message": "id/resource 缺失"})
            return
        item = {"id": eid, "name": parsed.get("name", ""),
                "create_time": int(time.time() * 1000),
                "update_time": int(time.time() * 1000),
                "resource": resource}
        with _lock:
            _elements[eid] = item
        _json_response(conn, {"code": 0, "message": "success", "data": {"id": eid}})
        # 平台收到后会把 create 回推给所有客户端（含发起方），用来验证不重复
        threading.Timer(1.0, push, args=("map_element_create",
                                         dict(item, group_id=SHARED_GROUP_ID))).start()
        return

    if elem_m and method == "PUT":
        eid = elem_m.group(2)
        log("  -> PUT body:", json.dumps(parsed, ensure_ascii=False))
        with _lock:
            item = _elements.get(eid)
            if item:
                item["name"] = parsed.get("name", item["name"])
                if parsed.get("content"):
                    item["resource"]["content"] = parsed["content"]
                item["update_time"] = int(time.time() * 1000)
                echo = json.loads(json.dumps(item))
        if not item:
            _json_response(conn, {"code": 404, "message": "not found"})
            return
        _json_response(conn, {"code": 0, "message": "success"})
        threading.Timer(1.0, push, args=("map_element_update",
                                         dict(echo, group_id=SHARED_GROUP_ID))).start()
        return

    if elem_m and method == "DELETE":
        eid = elem_m.group(2)
        with _lock:
            _elements.pop(eid, None)
        log("  -> DELETE", eid)
        _json_response(conn, {"code": 0, "message": "success"})
        threading.Timer(1.0, push, args=("map_element_delete",
                                         {"group_id": SHARED_GROUP_ID, "id": eid})).start()
        return

    log("  -> 未实现的接口，返回 404")
    _json_response(conn, {"code": 404, "message": "no such route"}, "404 Not Found")


def handle_conn(conn):
    request = b""
    while b"\r\n\r\n" not in request:
        chunk = conn.recv(4096)
        if not chunk:
            conn.close()
            return
        request += chunk

    head, _, rest = request.partition(b"\r\n\r\n")
    lines = head.decode("latin-1").split("\r\n")
    method, path, _ = (lines[0].split(" ") + ["", "", ""])[:3]
    headers = {}
    for line in lines[1:]:
        if ":" in line:
            name, value = line.split(":", 1)
            headers[name.strip().lower()] = value.strip()

    if "websocket" not in headers.get("upgrade", "").lower():
        length = int(headers.get("content-length") or 0)
        body = rest
        while len(body) < length:
            chunk = conn.recv(length - len(body))
            if not chunk:
                break
            body += chunk
        try:
            handle_rest(conn, method, path, headers, body)
        except Exception as exc:                    # 桩服务，任何解析错误都只记一行
            log("  -> 处理出错:", exc)
        conn.close()
        return

    key = headers.get("sec-websocket-key")
    if not key:
        conn.close()
        return
    accept = base64.b64encode(hashlib.sha1((key + GUID).encode()).digest()).decode()
    conn.sendall((
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n"
    ).encode())
    # 客户端把 access_token 拼在 ws 的 query 里（?x-auth-token=...），是真实凭据，
    # 日志里只留路径，绝不落到文件里。
    log("WS 握手完成", path.split("?")[0],
        "token:", "<in-query>" if "x-auth-token=" in path
                 else ("<header>" if headers.get("x-auth-token") else "<missing>"))
    first = False
    with _lock:
        first = not _ws_conns
        _ws_conns.append(conn)
    if first:
        threading.Thread(target=scenario, daemon=True).start()
    read_frames(conn)
    with _lock:
        if conn in _ws_conns:
            _ws_conns.remove(conn)
    log("WS 断开")


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    seed()
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", port))
    server.listen(8)
    log("listening on 127.0.0.1:%d  ws=ws://127.0.0.1:%d/api/v1/ws  http=http://127.0.0.1:%d"
        % (port, port, port))
    while True:
        conn, _ = server.accept()
        threading.Thread(target=handle_conn, args=(conn,), daemon=True).start()


main()
