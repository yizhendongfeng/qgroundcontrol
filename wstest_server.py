"""Minimal RFC6455 WebSocket server used only to smoke-test the DJI cloud ws client.

Pushes the same biz_code envelopes the Cloud-API-Demo backend sends, so the four
drawer tabs (devices / health / progress / messages) all get real data.
"""
import base64
import hashlib
import json
import socket
import struct
import sys
import threading
import time

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


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
    conn.sendall(bytes(header) + data)


def envelope(biz_code, data):
    return json.dumps({
        "biz_code": biz_code,
        "version": "1.0",
        "timestamp": int(time.time() * 1000),
        "data": data,
    })


def feed(conn):
    seq = 0
    while True:
        seq += 1
        send_text(conn, envelope("device_online", {
            "sn": "drone001", "device_callsign": "Matrice4T", "device_model": "M4T",
            "online_status": True, "gateway_sn": "dgcs001", "bound_status": True,
        }))
        send_text(conn, envelope("device_online", {
            "sn": "drone002", "device_callsign": "Mavic3E", "device_model": "M3E",
            "online_status": True, "gateway_sn": "dgcs001",
        }))
        # 每帧一条 OSD，密集推送以验证 250ms 合流
        for _ in range(8):
            send_text(conn, envelope("device_osd", {
                "sn": "drone001",
                "host": {
                    "battery": {"capacity_percent": 90 - (seq % 5)},
                    "height": 100.0 + seq,
                    "latitude": 39.90872 + seq * 1e-5,
                    "longitude": 116.39748 + seq * 1e-5,
                    "mode_code": 1,
                },
            }))
            time.sleep(0.05)
        send_text(conn, envelope("gateway_osd", {
            "sn": "dgcs001", "host": {"capacity_percent": 66, "mode_code": 0},
        }))
        send_text(conn, envelope("device_hms", {
            "sn": "drone001",
            "host": [{
                "hms_id": "hms-1", "tid": "t1", "bid": "b1", "sn": "drone001",
                "level": 2, "module": 0,
                "key": "drone_battery_temp_high",
                "message_zh": "电池温度过高，请立即降落",
                "message_en": "Battery temperature is too high",
                "create_time": "2026-09-22T10:00:00Z",
            }],
        }))
        send_text(conn, envelope("flighttask_progress", {
            "bid": "task-1", "sn": "drone001", "timestamp": int(time.time() * 1000),
            "data": {
                "ext": {"wayline_id": "wayline-abc", "flight_id": "flight-1"},
                "progress": {"percent": (seq * 7) % 100, "current_step": seq},
                "status": "in_progress",
            },
        }))
        send_text(conn, envelope("map_element_create", {"sn": "drone001", "id": "elem-1", "name": "poi"}))
        if seq % 4 == 0:
            send_text(conn, envelope("device_offline", {"sn": "drone002", "online_status": False}))
        time.sleep(1.0)


def handle(conn, path):
    request = b""
    while b"\r\n\r\n" not in request:
        chunk = conn.recv(4096)
        if not chunk:
            return
        request += chunk
    print("REQ LINE: " + request.split(b"\r\n")[0].decode("latin-1"), flush=True)
    headers = {}
    for line in request.decode("latin-1").split("\r\n")[1:]:
        if ":" in line:
            name, value = line.split(":", 1)
            headers[name.strip().lower()] = value.strip()
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
    print("HANDSHAKE OK, subprotocol=" + repr(headers.get("sec-websocket-protocol")), flush=True)
    try:
        feed(conn)
    except (OSError, BrokenPipeError):
        print("client gone", flush=True)


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", port))
    server.listen(4)
    print("listening on 127.0.0.1:%d" % port, flush=True)
    while True:
        conn, _ = server.accept()
        threading.Thread(target=handle, args=(conn, "?"), daemon=True).start()


main()
