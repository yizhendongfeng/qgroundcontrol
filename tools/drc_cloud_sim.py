# -*- coding: utf-8 -*-
"""DGCS 云端 DRC 模拟器 —— 带界面的远程控制测试台。

它扮演的是**云端**这一侧：连到 EMQX，用大疆上云 API 的报文驱动 DGCS 里的
DjiDrcClient，从而在没有真实后台的情况下端到端地测远程控制（DRC）。

典型使用顺序（界面上按顺序点就行）：

    1. [自动发现] 或手填 SN     —— 从 thing/product/+/osd 里认出本机 DGCS 的 gcsSn
    2. [连接]
    3. [进入 DRC 会话]          —— 下发 drc_mode_enter，DGCS 会另开一条 MQTT 连接
    4. [抢飞行控制权]           —— 默认设置下 DGCS 会弹窗，**要去 DGCS 窗口点「是」**
    5. 拖动摇杆                  —— 以 10Hz 下发 drone_control
    6. [停桨] / [释放控制权] / [退出 DRC]

报文格式直接对着 src/DjiBridge/DjiDrcClient.cc 与 DjiCloudClient.cc 写，
不是照着文档猜的。

已知边界：
  · DGCS 的设置里 drcRequireLocalConsent 默认是开的，所以第 4 步必须有人在
    DGCS 那边点确认；关掉它才会自动同意。
  · 停桨除了设置里的 drcEmergencyStopEnabled 总开关，DGCS 也会弹窗二次确认。
  · 同一个 EMQX 上如果还连着真后台，两边都会往 thing/product/<sn>/services 发，
    测的时候注意别让真后台同时在抢控制权。
"""

import json
import queue
import sys
import time
import uuid

import tkinter as tk
from tkinter import ttk, messagebox

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("缺少 paho-mqtt，请先执行：pip install paho-mqtt")
    sys.exit(1)


# --------------------------------------------------------------------------
# 协议常量（取自 src/DjiBridge/DjiDrcControlMapper.cc:29-45）
# --------------------------------------------------------------------------
X_LIMIT = 17.0   # m/s，正 = 左移
Y_LIMIT = 17.0   # m/s，正 = 前进
H_MIN, H_MAX = -4.0, 5.0   # m/s，协议规定上升最多 5、下降最多 4
W_LIMIT = 90.0   # °/s，正 = 顺时针

# DjiDrcControlMapper.h:33-49
RESULT_TEXT = {
    0:      "成功",
    327002: "无控制权 / 被拒",       # kObtainControlFailed
    327005: "云台瞄准失败",
    327006: "拍照失败",
    327007: "开始录像失败",
    327008: "停止录像失败",
    327009: "切换相机模式失败",
    327010: "变焦失败",
    327014: "云台到限位",
    327015: "镜头类型不对",
    514300: "DRC 通用异常",
    514301: "DRC 心跳超时",
    514302: "DRC 证书异常",
    514303: "DRC 链路丢失",
    514304: "DRC 链路被拒",
}

UI_FONT = ("Microsoft YaHei UI", 9)


def now_ms():
    return int(time.time() * 1000)


def uid():
    return str(uuid.uuid4())


# --------------------------------------------------------------------------
# MQTT / 协议层
# --------------------------------------------------------------------------
class DrcCloudSim:
    """云端侧。收发包都在这里，界面只读它的状态。"""

    def __init__(self, out_queue):
        self.q = out_queue            # 往界面丢事件：("log"|"osd"|"up"|"reply"|"stat", payload)
        self.client = None
        self.connected = False
        self.sn = "dgcs001"
        self.host = "192.168.144.110"
        self.port = 1883
        self.username = ""
        self.password = ""

        # 杆量（DRC 语义：中立是 0/0/0/0 —— 注意不是 MAVLink 那个 0.5 的油门零点，
        # 映射成 [0,1] 的推力是 DjiDrcControlMapper 干的事，这里只管协议量）
        self.x = 0.0
        self.y = 0.0
        self.h = 0.0
        self.w = 0.0
        self.seq = 0

        self.tx_drone_control = 0
        self.rx_up = 0
        self.rej = 0
        self.last_result = None
        self.discovered_sns = set()

        # DRC 会话是否已经建立（drc_mode_enter 被 DGCS 受理）。
        # 一旦为真就**必须**持续发下行，见 App._tick 的说明。
        self.session_active = False
        self.last_up_ms = 0        # 最近一次收到 drc/up 的时刻，用来判会话是否还活着

    # ---------- 连接 ----------
    def connect(self, host, port, username, password, sn):
        self.disconnect()
        self.host, self.port = host, port
        self.username, self.password = username, password
        self.sn = sn
        self.discovered_sns.clear()
        self.session_active = False
        self.last_up_ms = 0

        c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                        client_id="dgcs-drc-cloudsim-%d" % (int(time.time()) % 100000),
                        protocol=mqtt.MQTTv5)
        if username:
            c.username_pw_set(username, password)
        c.on_connect = self._on_connect
        c.on_disconnect = self._on_disconnect
        c.on_message = self._on_message
        self.client = c
        try:
            c.connect(host, port, 10)
        except Exception as e:
            self.client = None
            raise RuntimeError("连不上 %s:%s —— %s" % (host, port, e))
        c.loop_start()

    def disconnect(self):
        if self.client is None:
            return
        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception:
            pass
        self.client = None
        self.connected = False

    # ---------- paho 回调 ----------
    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        ok = (reason_code == 0 or str(reason_code) == "Success")
        self.connected = bool(ok)
        if not ok:
            self._log("连接被拒：%s" % reason_code)
            return
        self._sub_many(self._topics())
        self._log("已连接 %s:%s，已订阅 %s 的主题" % (self.host, self.port, self.sn))
        self.q.put(("stat", None))

    def _on_disconnect(self, *args):
        self.connected = False
        self._log("连接断开")
        self.q.put(("stat", None))

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8", "replace"))
        except Exception:
            return
        topic = msg.topic

        # 自动发现：thing/product/<sn>/osd
        if topic.startswith("thing/product/") and topic.endswith("/osd"):
            seg = topic.split("/")
            if len(seg) >= 3:
                self.discovered_sns.add(seg[2])
            self.q.put(("osd", payload))

        elif topic.endswith("/drc/up"):
            self.rx_up += 1
            self.last_up_ms = now_ms()
            method = payload.get("method", "")
            data = payload.get("data") or {}
            if method == "drone_control":
                self.last_result = data.get("result")
                if self.last_result not in (0, None):
                    self.rej += 1
            self.q.put(("up", payload))

        elif topic.endswith("/services_reply"):
            self.q.put(("reply", payload))

        elif topic.endswith("/events"):
            self.q.put(("event", payload))

        elif topic.endswith("/status"):
            self.q.put(("topo", payload))

    # ---------- 订阅 / 发布 ----------
    def _topics(self):
        t = [
            "thing/product/%s/services_reply" % self.sn,
            "thing/product/%s/drc/up" % self.sn,
            "thing/product/%s/events" % self.sn,
            "thing/product/%s/osd" % self.sn,
            "sys/product/%s/status" % self.sn,
        ]
        return t

    def _sub_many(self, topics, qos=0):
        if self.client is None:
            return
        for t in topics:
            self.client.subscribe(t, qos)

    def start_discovery(self):
        """订阅通配 osd，用来认出本机 DGCS 的 SN。"""
        if self.client is None:
            return
        self.discovered_sns.clear()
        self.client.subscribe("thing/product/+/osd", 0)

    def stop_discovery(self):
        if self.client is None:
            return
        self.client.unsubscribe("thing/product/+/osd")

    def _publish(self, topic, obj):
        if self.client is None or not self.connected:
            return False
        self.client.publish(topic, json.dumps(obj, ensure_ascii=False, separators=(",", ":")), qos=0)
        return True

    @staticmethod
    def _envelope(method, data):
        return {"bid": uid(), "tid": uid(), "timestamp": now_ms(),
                "method": method, "data": data}

    def send_service(self, method, data=None):
        """services 下行（云 → 设备），应答走 services_reply。"""
        ok = self._publish("thing/product/%s/services" % self.sn,
                           self._envelope(method, data or {}))
        if ok:
            self._log("→ services  %s  %s" % (method, json.dumps(data or {}, ensure_ascii=False)))
        else:
            self._log("× 未连接，发不出 %s" % method)
        return ok

    def send_drc_down(self, method, data=None):
        """drc/down 下行，只有 drone_control / drone_emergency_stop / heart_beat
        三个 method 是 DGCS 认的（另外还认 drc_initial_state_subscribe）。"""
        ok = self._publish("thing/product/%s/drc/down" % self.sn,
                           self._envelope(method, data or {}))
        return ok

    def send_drone_control(self, freq=10, delay_time=1000):
        self.seq += 1
        data = {
            "seq": self.seq,
            "x": round(self.x, 3),
            "y": round(self.y, 3),
            "h": round(self.h, 3),
            "w": round(self.w, 3),
            # freq 决定 DGCS 的 deadman 阈值：max(500, max(3000/freq, delay_time)) ms。
            # 给 10Hz + 1000ms 时延，deadman 就是 1s —— 别调太小，否则界面一卡顿
            # DGCS 就把杆量归零了（那是它的正常保护，但会让测量看起来像"指令没生效"）。
            "freq": freq,
            "delay_time": delay_time,
        }
        if self.send_drc_down("drone_control", data):
            self.tx_drone_control += 1

    # ---------- 日志 ----------
    def _log(self, text):
        self.q.put(("log", text))


# --------------------------------------------------------------------------
# 界面
# --------------------------------------------------------------------------
class StickPad(tk.Canvas):
    """一块 2D 摇杆。对外回调 (nx, ny)，都是 [-1,1]，右/上为正；松手回调 (0,0)。

    连线只 create 一次、之后用 coords 更新 —— 早先的写法是每次拖动都 create_line，
    于是每动一下就往画布上留一条永不消失的蓝线，拖一会儿满屏都是。
    """

    def __init__(self, parent, size, top, bottom, left, right, on_change):
        super().__init__(parent, width=size, height=size, bg="#fafafa",
                         highlightthickness=1, highlightbackground="#bbb")
        self._size = size
        self._c = size / 2.0
        self._r = self._c - 30
        self._on_change = on_change

        c, r = self._c, self._r
        self.create_oval(c - r, c - r, c + r, c + r, outline="#ccc", dash=(3, 3))
        self.create_line(c - r, c, c + r, c, fill="#e0e0e0")
        self.create_line(c, c - r, c, c + r, fill="#e0e0e0")
        self.create_text(c, r - 2, text=top, fill="#888", font=UI_FONT)
        self.create_text(c, size - r + 2, text=bottom, fill="#888", font=UI_FONT)
        self.create_text(r + 4, c - 13, text=left, fill="#888", font=UI_FONT)
        self.create_text(size - r - 4, c - 13, text=right, fill="#888", font=UI_FONT)

        self._line = self.create_line(c, c, c, c, fill="#2d7dd2", width=2)
        self._knob = self.create_oval(c - 14, c - 14, c + 14, c + 14,
                                      fill="#2d7dd2", outline="#1b5391", width=2)

        self.bind("<Button-1>", self._drag)
        self.bind("<B1-Motion>", self._drag)
        self.bind("<ButtonRelease-1>", self._release)

    def _drag(self, ev):
        dx, dy = ev.x - self._c, ev.y - self._c
        d = (dx * dx + dy * dy) ** 0.5
        if d > self._r:                      # 拖出圆就夹到圆周上
            dx, dy = dx * self._r / d, dy * self._r / d
        self._place(dx, dy)
        self._on_change(dx / self._r, -dy / self._r)

    def _release(self, _ev):
        self.recenter()
        self._on_change(0.0, 0.0)

    def _place(self, dx, dy):
        c = self._c
        self.coords(self._knob, c + dx - 14, c + dy - 14, c + dx + 14, c + dy + 14)
        self.coords(self._line, c, c, c + dx, c + dy)

    def recenter(self):
        self._place(0.0, 0.0)


class App:
    def __init__(self, root):
        self.root = root
        self.q = queue.Queue()
        self.sim = DrcCloudSim(self.q)

        self.streaming = False
        self._stream_auto_started = False
        self._last_hb_ms = 0
        self._tick_job = None
        self._discover_job = None
        self._osd_latest = {}
        self._log_lines = 0

        root.title("DGCS 云端 DRC 模拟器 —— 远程控制测试台")
        root.geometry("1080x720")
        root.minsize(940, 620)

        self._build_style()
        self._build_ui()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(60, self._drain)
        self._schedule_tick()

    # ---------------- 外观 ----------------
    def _build_style(self):
        st = ttk.Style()
        try:
            st.theme_use("vista")
        except Exception:
            pass
        st.configure(".", font=UI_FONT)
        st.configure("Head.TLabel", font=("Microsoft YaHei UI", 11, "bold"))
        st.configure("Hint.TLabel", foreground="#666")
        st.configure("Go.TButton", font=("Microsoft YaHei UI", 10, "bold"))
        st.configure("Stop.TButton", font=("Microsoft YaHei UI", 10, "bold"),
                     foreground="#b00000")

    # ---------------- 布局 ----------------
    def _build_ui(self):
        outer = ttk.Frame(self.root, padding=8)
        outer.pack(fill="both", expand=True)

        self._build_conn_bar(outer)

        body = ttk.Frame(outer)
        body.pack(fill="both", expand=True, pady=(8, 0))

        left = ttk.Frame(body)
        left.pack(side="left", fill="y")
        right = ttk.Frame(body)
        right.pack(side="left", fill="both", expand=True, padx=(10, 0))

        self._build_actions(left)
        self._build_sticks(left)
        self._build_right(right)

    def _build_conn_bar(self, parent):
        bar = ttk.LabelFrame(parent, text=" 连接 ", padding=8)
        bar.pack(fill="x")

        ttk.Label(bar, text="EMQX").grid(row=0, column=0, sticky="w")
        self.v_host = tk.StringVar(value="192.168.144.110")
        ttk.Entry(bar, textvariable=self.v_host, width=16).grid(row=0, column=1, padx=(4, 10))

        ttk.Label(bar, text="端口").grid(row=0, column=2, sticky="w")
        self.v_port = tk.StringVar(value="1883")
        ttk.Entry(bar, textvariable=self.v_port, width=6).grid(row=0, column=3, padx=(4, 10))

        ttk.Label(bar, text="用户").grid(row=0, column=4, sticky="w")
        self.v_user = tk.StringVar(value="")
        ttk.Entry(bar, textvariable=self.v_user, width=10).grid(row=0, column=5, padx=(4, 10))

        ttk.Label(bar, text="密码").grid(row=0, column=6, sticky="w")
        self.v_pass = tk.StringVar(value="")
        ttk.Entry(bar, textvariable=self.v_pass, width=10, show="*").grid(row=0, column=7, padx=(4, 10))

        ttk.Label(bar, text="本机 SN").grid(row=0, column=8, sticky="w")
        self.v_sn = tk.StringVar(value="dgcs001")
        ttk.Entry(bar, textvariable=self.v_sn, width=14).grid(row=0, column=9, padx=(4, 6))

        self.btn_discover = ttk.Button(bar, text="自动发现", width=10, command=self.on_discover)
        self.btn_discover.grid(row=0, column=10, padx=(0, 6))

        self.btn_conn = ttk.Button(bar, text="连接", width=8, command=self.on_connect)
        self.btn_conn.grid(row=0, column=11)

        self.v_conn_state = tk.StringVar(value="● 未连接")
        self.lbl_conn = ttk.Label(bar, textvariable=self.v_conn_state, foreground="#b00000")
        self.lbl_conn.grid(row=0, column=12, padx=(12, 0))

        ttk.Label(bar, textvariable=tk.StringVar(
            value="顺序：连接 → 进入 DRC 会话 → 抢飞行控制权 →(去 DGCS 点「是」)→ 拖摇杆"),
            style="Hint.TLabel").grid(row=1, column=0, columnspan=13, sticky="w", pady=(6, 0))

    def _build_actions(self, parent):
        box = ttk.LabelFrame(parent, text=" 会话 / 控制权 ", padding=8)
        box.pack(fill="x")

        rows = [
            ("进入 DRC 会话", self.on_mode_enter, "drc_mode_enter"),
            ("抢飞行控制权", self.on_grab_flight, "flight_authority_grab"),
            ("请求接管(带用户)", self.on_auth_request, "cloud_control_auth_request"),
            ("抢负载控制权", self.on_grab_payload, "payload_authority_grab"),
            ("释放控制权", self.on_release, "cloud_control_release"),
            ("退出 DRC 会话", self.on_mode_exit, "drc_mode_exit"),
        ]
        for i, (text, cb, _m) in enumerate(rows):
            ttk.Button(box, text=text, width=18, command=cb).grid(
                row=i, column=0, sticky="ew", pady=1)

        ttk.Separator(box).grid(row=len(rows), column=0, sticky="ew", pady=6)

        ttk.Button(box, text="订阅远程状态", width=18, command=self.on_subscribe_state).grid(
            row=len(rows) + 1, column=0, sticky="ew", pady=1)
        ttk.Button(box, text="发一次心跳", width=18, command=self.on_heartbeat).grid(
            row=len(rows) + 2, column=0, sticky="ew", pady=1)
        ttk.Button(box, text="停桨(危险)", width=18, style="Stop.TButton",
                   command=self.on_emergency_stop).grid(
            row=len(rows) + 3, column=0, sticky="ew", pady=(6, 1))

    def _build_sticks(self, parent):
        box = ttk.LabelFrame(parent, text=" 摇杆 ", padding=8)
        box.pack(fill="x", pady=(8, 0))

        pads = ttk.Frame(box)
        pads.pack()

        # 左杆：航向 + 油门（和真遥控器一样的排布）
        lf = ttk.Frame(pads)
        lf.pack(side="left")
        ttk.Label(lf, text="航向 / 油门", style="Hint.TLabel").pack()
        self.pad_l = StickPad(lf, 168, "升 h+5", "降 h−4", "w− 逆", "w+ 顺",
                              self.on_left_pad)
        self.pad_l.pack()

        # 右杆：前后 + 左右
        rf = ttk.Frame(pads)
        rf.pack(side="left", padx=(14, 0))
        ttk.Label(rf, text="前后 / 左右", style="Hint.TLabel").pack()
        self.pad_r = StickPad(rf, 168, "前进 y+17", "后退 y−17", "x+ 左", "x− 右",
                              self.on_right_pad)
        self.pad_r.pack()

        # 杆量流（= DRC 下行保活，会话活着就不能停）
        stream = ttk.Frame(box)
        stream.pack(fill="x", pady=(10, 0))
        self.v_stream = tk.BooleanVar(value=False)
        ttk.Checkbutton(stream, text="杆量流（保活）", variable=self.v_stream,
                        command=self.on_stream_toggle).pack(side="left")
        ttk.Label(stream, text="频率").pack(side="left", padx=(8, 2))
        self.v_freq = tk.StringVar(value="10")
        ttk.Entry(stream, textvariable=self.v_freq, width=4).pack(side="left")
        ttk.Label(stream, text="Hz").pack(side="left", padx=(2, 8))
        ttk.Button(stream, text="回中", width=6, command=self.on_center).pack(side="left")
        ttk.Button(stream, text="发一帧", width=7, command=self.on_send_once).pack(
            side="left", padx=(6, 0))

        self.v_stick_read = tk.StringVar(value="")
        ttk.Label(box, textvariable=self.v_stick_read).pack(anchor="w", pady=(6, 0))
        self._refresh_stick_read()

        ttk.Label(box, style="Hint.TLabel", wraplength=360, justify="left",
                  text="x 正=左移、y 正=前进、h 正=上升（升最多 5 / 降最多 4）、w 正=顺时针。"
                       "界面已按「往哪拖就往哪飞」映射（右拖 = 右移）。"
                       "若 DGCS 里开了 drcInvertX/Y/W，方向会反过来。").pack(anchor="w")

    def _build_right(self, parent):
        nb = ttk.Notebook(parent)
        nb.pack(fill="both", expand=True)

        # --- 遥测 ---
        tel = ttk.Frame(nb, padding=8)
        nb.add(tel, text="遥测 / 应答")

        self.v_result = tk.StringVar(value="最近一次 drone_control 应答：—")
        ttk.Label(tel, textvariable=self.v_result, style="Head.TLabel").pack(anchor="w")

        self.v_stats = tk.StringVar(value="")
        ttk.Label(tel, textvariable=self.v_stats).pack(anchor="w", pady=(4, 8))

        self.txt_osd = tk.Text(tel, height=12, wrap="word", font=("Consolas", 9),
                               bg="#fbfbfb", relief="solid", borderwidth=1)
        self.txt_osd.pack(fill="both", expand=True)
        self.txt_osd.configure(state="disabled")

        # --- 日志 ---
        logf = ttk.Frame(nb, padding=8)
        nb.add(logf, text="报文日志")
        self.txt_log = tk.Text(logf, wrap="word", font=("Consolas", 9),
                               bg="#1e1e1e", fg="#d4d4d4", insertbackground="#d4d4d4")
        self.txt_log.pack(fill="both", expand=True)
        sb = ttk.Scrollbar(self.txt_log, command=self.txt_log.yview)
        self.txt_log.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")

    # ---------------- 摇杆绘制 ----------------
    # ---------------- 交互 ----------------
    def _auto_start_stream(self):
        """拖动即开始发 —— 测试台就是要点一下就能看见效果。

        注意必须走 on_stream_toggle 把 self.streaming 也置上：只 set 复选框
        的话，界面显示"已开"而 _tick 里仍是 False，就成了勾着不发。
        """
        if not self.streaming:
            self.v_stream.set(True)
            self.on_stream_toggle()
            self._stream_auto_started = True
            self.append_log("  （拖动触发，无需手动勾）")

    def on_left_pad(self, nx, ny):
        """左杆：nx∈[-1,1] 右为正 → w（顺时针正）；ny 上为正 → h（上升正）。"""
        self.sim.w = W_LIMIT * nx
        # h 的量程是不对称的：协议规定上升最多 5、下降最多 4
        self.sim.h = ny * (H_MAX if ny >= 0 else -H_MIN)
        self._refresh_stick_read()
        self._auto_start_stream()

    def on_right_pad(self, nx, ny):
        """右杆：nx 右为正、ny 上为正。协议里 x 正 = 左移，所以 x 取负 ——
        这样"往右拖 = 飞机往右飞"，跟手。"""
        self.sim.x = -X_LIMIT * nx
        self.sim.y = Y_LIMIT * ny
        self._refresh_stick_read()
        self._auto_start_stream()

    def on_center(self):
        self.sim.x = self.sim.y = self.sim.h = self.sim.w = 0.0
        self.pad_l.recenter()
        self.pad_r.recenter()
        self._refresh_stick_read()

    def _refresh_stick_read(self):
        self.v_stick_read.set("x=%6.2f  y=%6.2f   h=%5.2f  w=%6.2f   seq=%d"
                              % (self.sim.x, self.sim.y, self.sim.h, self.sim.w, self.sim.seq + 1))

    # ---------------- 按钮 ----------------
    def on_connect(self):
        if self.sim.client is not None:
            self.sim.disconnect()
            self._set_conn_label(False)
            self.btn_conn.configure(text="连接")
            self.append_log("· 已断开")
            return
        try:
            port = int(self.v_port.get())
        except ValueError:
            messagebox.showerror("参数错误", "端口要是数字")
            return
        sn = self.v_sn.get().strip()
        if not sn:
            messagebox.showerror("参数错误", "SN 不能为空")
            return
        try:
            self.sim.connect(self.v_host.get().strip(), port,
                             self.v_user.get(), self.v_pass.get(), sn)
        except Exception as e:
            messagebox.showerror("连接失败", str(e))
            return
        self.btn_conn.configure(text="断开")

    def on_discover(self):
        if self.sim.client is None:
            messagebox.showinfo("还没连接", "先点「连接」，再自动发现。")
            return
        if self._discover_job:
            return
        self.sim.start_discovery()
        self.append_log("· 开始自动发现：订阅 thing/product/+/osd，等 5 秒…")
        self.btn_discover.configure(state="disabled")

        def finish():
            self._discover_job = None
            self.btn_discover.configure(state="normal")
            self.sim.stop_discovery()
            found = sorted(self.sim.discovered_sns)
            if not found:
                self.append_log("· 没发现任何设备在推 osd —— DGCS 登录了吗？（登录门控修完之后，"
                                "未登录是不连云、不发 osd 的）")
                return
            self.append_log("· 发现设备：%s" % ", ".join(found))
            if len(found) == 1:
                self.v_sn.set(found[0])
                self.append_log("· 已把 SN 填成 %s（改了 SN 要重新连接才生效）" % found[0])
            else:
                self.append_log("· 有多个设备，请自己从上面挑一个填进 SN，然后重新连接")

        self._discover_job = self.root.after(5000, finish)

    def on_mode_enter(self):
        # DRC 会话：告诉 DGCS 去哪条 broker 上收 drc/down，以及上行频率。
        data = {
            "mqtt_broker": {
                "address": "%s:%d" % (self.sim.host, self.sim.port),
                "client_id": "dgcs-drc-cloudsim",
                "username": self.sim.username,
                "password": self.sim.password,
                "enable_tls": False,
            },
            "osd_frequency": 10,
            "hsi_frequency": 1,
        }
        self.sim.send_service("drc_mode_enter", data)
        self.append_log("· 已下发 drc_mode_enter。DGCS 会另开一条 MQTT 连接，"
                        "它的 clientId 必须和主连接不同（DGCS 自己会兜一个）。")
        # 会话一开始就必须有下行流量，见 _tick 的说明
        if self.sim.connected and not self.streaming:
            self.v_stream.set(True)
            self.on_stream_toggle()

    def on_grab_flight(self):
        self.sim.send_service("flight_authority_grab",
                              {"user_id": "cloudsim-user", "user_callsign": "云端模拟器"})
        self.append_log("· 已请求飞行控制权。如果 DGCS 弹了接管确认窗，"
                        "**去 DGCS 窗口点「是」** —— 没点之前 drone_control 会被全部拒成 327002，"
                        "而且那条拒收在 DGCS 日志里是静默的。")

    def on_auth_request(self):
        self.sim.send_service("cloud_control_auth_request",
                              {"user_id": "cloudsim-user", "user_callsign": "云端模拟器"})
        self.append_log("· 已下发 cloud_control_auth_request（网页端登录云平台时走的那条）")

    def on_grab_payload(self):
        self.sim.send_service("payload_authority_grab",
                              {"user_id": "cloudsim-user", "user_callsign": "云端模拟器"})

    def on_release(self):
        self.sim.send_service("cloud_control_release")
        self.append_log("· 已释放控制权（DGCS 会停保活流、解锁本地摇杆、发 joystick_invalid_notify）")

    def on_mode_exit(self):
        self.sim.send_service("drc_mode_exit")
        self.sim.session_active = False
        self.append_log("· 已退出 DRC 会话（保活流随之停止）")
        if self.sim.connected and self.streaming:
            self.v_stream.set(False)
            self.on_stream_toggle()

    def on_subscribe_state(self):
        self.sim.send_drc_down("drc_initial_state_subscribe")
        self.append_log("· 已订阅远程状态（DGCS 会回 drc_drone_state_push / drc_camera_osd_info_push）")

    def on_heartbeat(self):
        self.sim.send_drc_down("heart_beat", {"seq": self.sim.seq or 0, "timestamp": now_ms()})

    def on_emergency_stop(self):
        if not messagebox.askyesno(
                "确认停桨",
                "停桨会下发 MAV_CMD_DO_FLIGHTTERMINATION，飞机直接停桨坠落，不可逆。\n\n"
                "（DGCS 那边还要求设置里开了 drcEmergencyStopEnabled，并会再弹窗要操作员确认。）\n\n"
                "真的发吗？"):
            return
        self.sim.send_drc_down("drone_emergency_stop", {})
        self.append_log("· 已发送 drone_emergency_stop")

    def on_stream_toggle(self):
        self.streaming = bool(self.v_stream.get())
        self.append_log("· 杆量流 %s（%d Hz）"
                        % ("开始" if self.streaming else "停止", self._freq()))
        if not self.streaming:
            self._stream_auto_started = False
            if self.sim.session_active:
                self.append_log("  ⚠ 会话还开着却没有下行流量了：DGCS 的 heartbeatTick 约 10s 收不到"
                                "任何下行就会按「心跳超时」把整个 DRC 会话拆掉，摇杆随即失效。"
                                "要真停就点「退出 DRC 会话」。")

    def on_send_once(self):
        if not self.sim.connected:
            self.append_log("× 未连接")
            return
        self.sim.send_drone_control(self._freq(), 1000)
        self._refresh_stick_read()

    # ---------------- 周期任务 ----------------
    def _freq(self):
        try:
            return max(2, min(10, int(self.v_freq.get())))
        except ValueError:
            return 10

    def _schedule_tick(self):
        self._tick_job = self.root.after(max(20, 1000 // self._freq()), self._tick)

    def _tick(self):
        if not self.sim.connected:
            self._schedule_tick()
            return

        # 只要 DRC 会话还活着就必须持续发 —— **不是**"拖了才发"。
        # DGCS 的 heartbeatTick 每 5s 检查"上次心跳之后有没有收到任何下行"，
        # 连续 2 次没有（≈10s 静默）就 teardownDrc("心跳超时") 把会话整个拆掉；
        # checkDownLink 还会在 deadman(约 1s) 后把杆量归零。会话一没，控制权就没了，
        # 拖摇杆自然"没有效果" —— 这正是之前那个现象。
        if self.sim.session_active or self.streaming:
            self.sim.send_drone_control(self._freq(), 1000)
            self._refresh_stick_read()
            # 协议里云端也发心跳下行（设备收到会回一条上行心跳）
            now = now_ms()
            if now - self._last_hb_ms >= 2000:
                self._last_hb_ms = now
                self.sim.send_drc_down("heart_beat", {"seq": 0, "timestamp": now})

        # 会话还活着却没有上行心跳了 —— DGCS 那边多半已经拆了
        if self.sim.session_active and self.sim.last_up_ms:
            if now_ms() - self.sim.last_up_ms > 15000:
                self.sim.session_active = False
                self.append_log("× 超过 15s 没收到 drc/up —— DGCS 那边多半已经把 DRC 会话拆了，"
                                "重新点「进入 DRC 会话」和「抢飞行控制权」")
        self._schedule_tick()

    # ---------------- 队列 → 界面 ----------------
    def _drain(self):
        while True:
            try:
                kind, payload = self.q.get_nowait()
            except queue.Empty:
                break
            # 单条报文处理失败不能把整条刷新链带走 —— 否则 _drain 不再重排，
            # 界面从此静默冻住，而看起来只是"没有新消息"。
            try:
                if kind == "log":
                    self.append_log(payload)
                elif kind == "osd":
                    self._on_osd(payload)
                elif kind == "up":
                    self._on_up(payload)
                elif kind == "reply":
                    self._on_reply(payload)
                elif kind == "event":
                    self._on_event(payload)
                elif kind == "topo":
                    self.append_log("← status      %s" % self._brief(payload))
                elif kind == "stat":
                    self._set_conn_label(self.sim.connected)
            except Exception as e:
                self.append_log("× 处理 %s 消息出错：%r" % (kind, e))
        self._update_stats()
        self.root.after(60, self._drain)

    def _on_event(self, payload):
        """drc_status_notify 是 DGCS 报会话状态的地方 —— 会话被拆会带一个错误码，
        这个码必须显眼，否则表现就是"摇杆突然没效果"，看不出原因。"""
        method = payload.get("method", "?")
        data = payload.get("data") or {}
        self.append_log("← events      %s" % self._brief(payload))
        if method == "drc_status_notify":
            state, r = data.get("drc_state"), data.get("result")
            if state == 0 and r:
                self.sim.session_active = False
                self.append_log("    × 会话已断：result=%s (%s)"
                                % (r, RESULT_TEXT.get(r, "未知码")))
                if r == 514301:
                    self.append_log("      514301 = 心跳超时 —— 下行静默太久被 DGCS 拆的。"
                                    "勾上「杆量流（保活）」再进会话，别等它静下来。")
                self.append_log("      要恢复就重新点「进入 DRC 会话」→「抢飞行控制权」。")
        elif method == "joystick_invalid_notify":
            self.append_log("    （DGCS 通知本地摇杆已失效/恢复，reason=%s）" % data.get("reason"))

    @staticmethod
    def _brief(obj):
        s = json.dumps(obj, ensure_ascii=False)
        return s if len(s) <= 300 else s[:300] + "…"

    def _on_up(self, payload):
        method = payload.get("method", "?")
        data = payload.get("data") or {}
        if method == "drone_control":
            r = data.get("result")
            self.append_log("← drc/up  drone_control  seq=%s result=%s (%s)"
                            % ((data.get("output") or {}).get("seq"), r,
                               RESULT_TEXT.get(r, "未知码")))
        elif method in ("drc_drone_state_push", "drc_camera_osd_info_push"):
            self.append_log("← drc/up  %s" % method)
            self.append_log("    %s" % self._brief(data))
        else:
            self.append_log("← drc/up  %s  %s" % (method, self._brief(data)))

    def _on_reply(self, payload):
        method = payload.get("method", "?")
        data = payload.get("data") or {}
        r = data.get("result")
        self.append_log("← services_reply  %s  result=%s%s"
                        % (method, r, "" if r is None else " (%s)" % RESULT_TEXT.get(r, "未知码")))
        if method == "drc_mode_enter" and r == 0:
            # 受理即视为会话存在：从这一刻起 _tick 必须持续发下行，否则 10s 就被拆
            self.sim.session_active = True
            self.sim.last_up_ms = now_ms()
            self.append_log("    → DGCS 已受理，正在连 DRC broker。"
                            "保活流已开启；连上之后再去点「抢飞行控制权」。")
        elif method in ("drc_mode_exit", "drc_mode_enter"):
            self.sim.session_active = False
        if method == "flight_authority_grab" and r == 327002:
            self.append_log("    × 327002 = 拿不到控制权。多半是 DGCS 的接管确认窗还没点「是」"
                            "（设置里 drcRequireLocalConsent 默认开），拒收在 DGCS 日志里是静默的。")

    def _on_osd(self, payload):
        data = payload.get("data") or {}
        self._osd_latest = data
        lines = []
        for k in ("mode_code", "battery", "height", "elevation", "latitude", "longitude",
                  "horizontal_speed", "vertical_speed", "capacity_percent"):
            if k in data:
                lines.append("%-18s %s" % (k, data[k]))
        pos = data.get("position_state")
        if isinstance(pos, dict):
            lines.append("%-18s %s" % ("position_state", json.dumps(pos, ensure_ascii=False)))
        ls = data.get("live_status")
        if isinstance(ls, list) and ls:
            lines.append("%-18s status=%s quality=%s video_id=%s"
                         % ("live_status", ls[0].get("status"),
                            ls[0].get("video_quality"), ls[0].get("video_id")))
        self._set_text(self.txt_osd, "\n".join(lines) if lines else json.dumps(
            data, ensure_ascii=False, indent=2))

    @staticmethod
    def _set_text(widget, text):
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        widget.insert("1.0", text)
        widget.configure(state="disabled")

    def _update_stats(self):
        s = self.sim
        up_age = ("" if not s.last_up_ms else "（上行 %d 秒前）"
                  % max(0, (now_ms() - s.last_up_ms) // 1000))
        self.v_stats.set("会话 %s   已发 drone_control %d 条%s   收到 drc/up %d 条   被拒 %d 条"
                         % ("在用" if s.session_active else "未建立",
                            s.tx_drone_control, up_age, s.rx_up, s.rej))
        if s.last_result is None:
            self.v_result.set("最近一次 drone_control 应答：—（还没收到应答）")
        else:
            self.v_result.set("最近一次 drone_control 应答：%s  %s"
                              % (s.last_result, RESULT_TEXT.get(s.last_result, "未知码")))

    def _set_conn_label(self, ok):
        self.v_conn_state.set("● 已连接" if ok else "● 未连接")
        self.lbl_conn.configure(foreground="#0a7a0a" if ok else "#b00000")

    def append_log(self, text):
        stamp = time.strftime("%H:%M:%S")
        self.txt_log.insert("end", "[%s] %s\n" % (stamp, text))
        self._log_lines += 1
        if self._log_lines > 3000:
            self.txt_log.delete("1.0", "500.0")
            self._log_lines -= 500
        self.txt_log.see("end")

    # ---------------- 退出 ----------------
    def on_close(self):
        # 云端持有控制权时直接关掉界面，DGCS 那边要等 deadman 超时才收手。
        # 主动交还一次更干净（拿不到控制权时这条也无害）。
        if self.sim.connected:
            try:
                self.sim.send_service("cloud_control_release")
                time.sleep(0.15)
            except Exception:
                pass
        if self._tick_job:
            self.root.after_cancel(self._tick_job)
        self.sim.session_active = False
        self.sim.disconnect()
        self.root.destroy()


def main():
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
