"""Simulated backend remote controller for the DJI Cloud API DRC feature set.

The cloud backend does *not* publish stick commands — the web panel's browser does.
This script plays both roles so a ground station can be driven end to end without a
browser or a real backend:

  * server role  — publishes `drc_mode_enter` / `flight_authority_grab` / `drc_mode_exit`
                   to thing/product/{sn}/services on the main connection, exactly like
                   DrcServiceImpl.deviceDrcEnter() does.
  * browser role — connects to the DRC broker handed out by `drc_mode_enter` and streams
                   `drone_control` to thing/product/{sn}/drc/down at 10 Hz, the way
                   use-manual-control.ts does.

Then it reads the uplink back and reports, per second, how many stick commands the ground
station accepted (result 0) versus refused (327002 = no flight authority). If the ground
station is configured to require local operator consent, the refused->accepted transition
is the moment somebody clicked the dialog — which makes the lockout auditable from a log.

Usage:
    python tools/drc_remote_sim.py                       # 40 s run against the defaults below
    python tools/drc_remote_sim.py --duration 60
    python tools/drc_remote_sim.py --pattern fixed --fixed-x 5
"""
import argparse
import json
import math
import os
import sys
import threading
import time
import uuid

import paho.mqtt.client as mqtt

# Result codes from DjiDrcControlMapper.h
RESULT_SUCCESS = 0
RESULT_NO_AUTHORITY = 327002

# Axis ranges from the protocol (see DjiDrcControlMapper.cc). h is asymmetric on purpose.
X_MAX, Y_MAX, W_MAX = 17.0, 17.0, 90.0
H_UP_MAX, H_DOWN_MAX = 5.0, 4.0

DRC_CONTROL_RATE = 10  # Hz; protocol allows 2..10


def now_ms():
    return int(time.time() * 1000)


def envelope(method, data):
    """DRC and services both use {tid, bid, timestamp, method, data}. tid/bid must survive
    the round trip: the ground station echoes them back in the reply and the backend
    correlates on them."""
    return {
        "tid": str(uuid.uuid4()),
        "bid": str(uuid.uuid4()),
        "timestamp": now_ms(),
        "method": method,
        "data": data,
    }


def drift(t, period, low, high):
    """A single slow sweep from low to high and back, for one axis at a time."""
    phase = (t % period) / period
    # triangle wave: 0 -> 1 -> 0
    tri = 2 * phase if phase < 0.5 else 2 * (1 - phase)
    return low + (high - low) * tri


class RemoteController:
    def __init__(self, args):
        self.args = args
        self.sn = args.sn
        self.seq = 0
        self.lock = threading.Lock()

        # ack accounting
        self.acks = {}                 # result code -> count
        self.acks_by_second = {}       # second index -> {code: count}
        self.first_accept_at = None
        self.sent = 0
        self.t0 = None                 # 杆量流的起点；osd 回调早于它到达时按 0 记
        self.events = []               # (method, data) off the events topic
        self.drc_state = None
        self.exit_seen = False

        # 无人机遥测（旁听 thing/product/+/osd，只读不写）。用途：证明杆量**真的被飞控执行了**。
        # 应答 result=0 只说明地面站收下了指令，不说明飞机在动；把命令值和飞控报回来的
        # 地速/位置摆在同一行，才能看出中间那一环断没断。
        self.tlm = []                  # (t, horizontal_speed, elevation, lat, lon, mode_code)

        self.stop_flag = threading.Event()
        self.mode_enter_reply = threading.Event()
        self.grab_reply = threading.Event()
        self.drc_connected = threading.Event()

        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=f"drc-remote-sim-{uuid.uuid4().hex[:8]}",
        )
        self.client.username_pw_set(args.user, args.password)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    # ---------------- topic helpers ----------------

    @property
    def t_services(self):
        return f"thing/product/{self.sn}/services"

    @property
    def t_services_reply(self):
        return f"thing/product/{self.sn}/services_reply"

    @property
    def t_events(self):
        return f"thing/product/{self.sn}/events"

    @property
    def t_drc_down(self):
        return f"thing/product/{self.sn}/drc/down"

    @property
    def t_drc_up(self):
        return f"thing/product/{self.sn}/drc/up"

    # ---------------- callbacks ----------------

    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code != 0:
            print(f"!! connect failed: {reason_code}")
            return
        print(f"connected to {self.args.host}:{self.args.port} rc={reason_code}")
        # osd 用通配订阅：droneSn 是地面站的设置项，脚本不想再要一个参数
        for topic in (self.t_services_reply, self.t_events, self.t_drc_up, "thing/product/+/osd"):
            client.subscribe(topic, qos=1)
            print(f"  subscribed {topic}")

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8", "replace"))
        except Exception:
            return
        method = payload.get("method")
        data = payload.get("data") or {}

        if msg.topic == self.t_services_reply:
            print(f"  <- services_reply {method} result={data.get('result')}")
            if method == "drc_mode_enter":
                self.mode_enter_reply.set()
            elif method == "flight_authority_grab":
                self.grab_reply.set()
            elif method == "drc_mode_exit":
                self.exit_seen = True
            return

        if msg.topic == self.t_events:
            self.events.append((method, data))
            if method == "drc_status_notify":
                state = data.get("drc_state")
                self.drc_state = state
                print(f"  <- drc_status_notify result={data.get('result')} drc_state={state}")
                if state == 2:
                    self.drc_connected.set()
            elif method == "cloud_control_auth_notify":
                status = (data.get("output") or {}).get("status")
                print(f"  <- cloud_control_auth_notify status={status}")
            elif method == "joystick_invalid_notify":
                print(f"  <- joystick_invalid_notify reason={data.get('reason')}")
            return

        if msg.topic == self.t_drc_up:
            if method == "drone_control":
                result = data.get("result")
                with self.lock:
                    self.acks[result] = self.acks.get(result, 0) + 1
                    if result == RESULT_SUCCESS and self.first_accept_at is None:
                        self.first_accept_at = time.time()
            return

        if msg.topic.endswith("/osd"):
            # 无人机 osd 才有 horizontal_speed；地面站 osd 走的是另一套字段（live_status 等）
            if "horizontal_speed" not in data:
                return
            with self.lock:
                self.tlm.append((
                    time.time() - self.t0 if self.t0 else 0.0,
                    data.get("horizontal_speed"),
                    data.get("elevation"),
                    data.get("latitude"),
                    data.get("longitude"),
                    data.get("mode_code"),
                ))
            return

    # ---------------- stages ----------------

    def send_services(self, method, data):
        self.client.publish(self.t_services, json.dumps(envelope(method, data)), qos=1)
        print(f"-> services {method}")

    def enter_drc(self):
        """Server role: hand the ground station a DRC broker plus the uphlink rates."""
        broker = {
            "address": f"{self.args.host}:{self.args.port}",
            "client_id": f"{self.args.drc_client_id or ('drc-gcs-' + uuid.uuid4().hex[:8])}",
            "username": self.args.user,
            "password": self.args.password,
            "enable_tls": False,
        }
        self.send_services("drc_mode_enter", {
            "mqtt_broker": broker,
            "osd_frequency": 10,
            "hsi_frequency": 1,
        })
        return self.drc_connected.wait(timeout=20)

    def grab_authority(self):
        """Server role: the official web panel's postFlightAuth(). On a ground station
        with local consent required this is what pops the confirmation dialog."""
        self.send_services("flight_authority_grab", {})
        self.grab_reply.wait(timeout=10)

    def exit_drc(self):
        self.send_services("drc_mode_exit", {})
        deadline = time.time() + 8
        while time.time() < deadline and not self.exit_seen:
            time.sleep(0.2)

    # ---------------- stick streaming ----------------

    def pattern(self, t):
        """Exercise one axis at a time so a screenshot shows unambiguously which axis moved."""
        if self.args.pattern == "fixed":
            return (self.args.fixed_x, self.args.fixed_y, self.args.fixed_h, self.args.fixed_w)
        if self.args.pattern == "zero":
            return (0.0, 0.0, 0.0, 0.0)

        period = self.args.pattern_period
        quarter = period / 4
        slot = int(t / quarter) % 4
        if slot == 0:
            return (drift(t, period, -X_MAX, X_MAX), 0.0, 0.0, 0.0)
        if slot == 1:
            return (0.0, drift(t, period, -Y_MAX, Y_MAX), 0.0, 0.0)
        if slot == 2:
            return (0.0, 0.0, drift(t, period, -H_DOWN_MAX, H_UP_MAX), 0.0)
        return (0.0, 0.0, 0.0, drift(t, period, -W_MAX, W_MAX))

    def stick_loop(self, t0):
        interval = 1.0 / DRC_CONTROL_RATE
        next_at = time.time()
        while not self.stop_flag.is_set():
            t = time.time() - t0
            x, y, h, w = self.pattern(t)
            self.seq += 1
            self.client.publish(self.t_drc_down, json.dumps(envelope("drone_control", {
                "seq": self.seq,
                "x": round(x, 2),
                "y": round(y, 2),
                "h": round(h, 2),
                "w": round(w, 2),
                "freq": DRC_CONTROL_RATE,
                "delay_time": 100,
            })), qos=0)
            self.sent += 1
            next_at += interval
            sleep = next_at - time.time()
            if sleep > 0:
                time.sleep(sleep)
            else:
                next_at = time.time()  # fell behind; resync instead of spinning

    def heartbeat_loop(self):
        hb_seq = 0
        while not self.stop_flag.is_set():
            hb_seq += 1
            self.client.publish(self.t_drc_down, json.dumps(envelope("heart_beat", {
                "seq": hb_seq,
                "timestamp": now_ms(),
            })), qos=0)
            self.stop_flag.wait(timeout=1.0)

    # ---------------- reporting ----------------

    def latest_tlm(self):
        with self.lock:
            return self.tlm[-1] if self.tlm else None

    def report_second(self, second, before):
        with self.lock:
            snapshot = dict(self.acks)
        delta = {code: snapshot.get(code, 0) - before.get(code, 0) for code in snapshot}
        delta = {code: n for code, n in delta.items() if n}
        accepted = delta.get(RESULT_SUCCESS, 0)
        refused = delta.get(RESULT_NO_AUTHORITY, 0)
        marker = ""
        if self.first_accept_at is not None:
            marker = f"  [first accept at t+{self.first_accept_at - self.t0:.1f}s]"

        # 命令了什么 vs 飞控报回来了什么。地速一直是 0 就说明最后一环没通 ——
        # 应答 result=0 只到地面站为止，管不到飞控执不执行。
        x, y, h, w = self.pattern(time.time() - self.t0) if self.t0 else (0, 0, 0, 0)
        tlm = self.latest_tlm()
        tlm_str = "  tlm: (无 osd)" if tlm is None else \
            (f"  tlm: 地速={tlm[1]:6.2f} 高={tlm[2]:7.2f} "
             f"@{tlm[3]:.6f},{tlm[4]:.6f} mode={tlm[5]}")

        print(f"  t+{second:3d}s  sent={self.sent:5d}  accepted={accepted:3d}  refused(327002)={refused:3d}{marker}")
        print(f"            cmd: x={x:6.2f} y={y:6.2f} h={h:5.2f} w={w:6.2f}{tlm_str}")
        return snapshot

    def summary(self):
        print("\n================ summary ================")
        print(f"drone_control sent        : {self.sent}")
        with self.lock:
            acks = dict(self.acks)
        total = sum(acks.values())
        print(f"replies received          : {total}")
        for code, n in sorted(acks.items()):
            label = {0: "accepted (云台已执行)",
                     RESULT_NO_AUTHORITY: "refused 327002"}.get(code, "other")
            print(f"  result {code:>7}  x{n:<5} {label}")
        # 327002 有三个来源，应答码分不出来，只能看地面站的日志：
        #   1) 未持有飞行控制权  2) 无可用飞机(no active vehicle)
        #   3) 飞机未解锁 / 模式不接受手动控制(not armed / wrong mode)
        # 所以这条汇总本身不能证明控制权拿到了 —— 要看地面站日志里的拒绝文案。
        print("  NOTE: 327002 无法区分「未持有控制权 / 无可用飞机 / 未解锁 / 模式不对」，")
        print("        要判定控制权是否生效请看地面站日志里的拒绝文案。")
        if self.first_accept_at is not None:
            print(f"first accepted at         : t+{self.first_accept_at - self.t0:.1f}s")
        else:
            print("first accepted at         : never (全部 327002，见上面的 NOTE)")
        print("events seen               :")
        for method, data in self.events:
            print(f"  {method} {json.dumps(data, ensure_ascii=False)}")
        print(f"drc_mode_exit reply seen  : {self.exit_seen}")
        self.movement_verdict()
        print("========================================")

    def movement_verdict(self):
        """最后一环：飞控到底动没动。

        应答 result=0 只能证明"地面站收下了并往下发了"，证明不了 PX4 真的执行了
        MANUAL_CONTROL。飞机有没有动只能看飞控报回来的地速和位置。
        """
        with self.lock:
            samples = list(self.tlm)
        print("telemetry (thing/product/*/osd) :")
        if not samples:
            print("  !! 一条无人机 osd 都没收到 —— 无法判断飞机是否运动")
            print("     （地面站没在跑、或 osd 周期上报没启动；先跑 tools/drc_probe.py 看看）")
            return

        # 前 1 秒的样本还可能停在推杆之前，用它们当基准算位移
        moving = [s for s in samples if s[0] >= 1.0] or samples
        speeds = [s[1] for s in samples if s[1] is not None]
        elevs = [s[2] for s in samples if s[2] is not None]
        print(f"  samples={len(samples)}  地速 最大={max(speeds) if speeds else float('nan'):.2f} "
              f"最小={min(speeds) if speeds else float('nan'):.2f} m/s")
        print(f"             相对高 {min(elevs) if elevs else float('nan'):.2f} ~ "
              f"{max(elevs) if elevs else float('nan'):.2f} m")
        print(f"  起点 {moving[0][3]:.6f},{moving[0][4]:.6f}  ->  "
              f"终点 {moving[-1][3]:.6f},{moving[-1][4]:.6f}")

        # 经纬度差换算成米（纬度 1° ≈ 111320 m，经度按纬度收缩）
        dla = (moving[-1][3] - moving[0][3]) * 111320.0
        dlo = (moving[-1][4] - moving[0][4]) * 111320.0 * math.cos(math.radians(moving[0][3]))
        moved = math.hypot(dla, dlo)
        print(f"  净位移 {moved:.2f} m")

        max_speed = max(speeds) if speeds else 0.0
        if moved < 1.0 and max_speed < 0.5:
            print("  ==> 飞控没有执行这些 MANUAL_CONTROL：地速与位置都没变。")
            print("      应答 result=0 只覆盖「云端→地面站→sendJoystickDataThreadSafe」，")
            print("      再往下的 PX4 那一环没通（链路/模式/飞控参数，与 DRC 无关）。")
        else:
            print("  ==> 飞控执行了：地速/位置随杆量变化，最后一环是通的。")

    def run(self):
        self.client.connect(self.args.host, self.args.port, 30)
        self.client.loop_start()
        time.sleep(1.0)

        print("\n--- stage 1: drc_mode_enter (server role) ---")
        if not self.enter_drc():
            print("!! ground station never reported drc_state=2; giving up")
            self.client.loop_stop()
            return 2

        if self.args.skip_grab:
            print("\n--- stage 2: skipped (--skip-grab) ---")
            print("    ==> 没有抢控制权，全程不用人工点确认框；heldFlightAuthority 会是 false")
        else:
            print("\n--- stage 2: flight_authority_grab (server role) ---")
            print("    ==> 地面站应该在这一步弹出确认框（若设置了需要本地操作员同意）")
            self.grab_authority()

        print(f"\n--- stage 3: streaming drone_control at {DRC_CONTROL_RATE} Hz for "
              f"{self.args.duration}s (browser role) ---")
        print("    ==> 同意之前每条应答都是 327002；同意之后变成 0，摇杆跟着动")
        self.t0 = time.time()
        stick = threading.Thread(target=self.stick_loop, args=(self.t0,), daemon=True)
        heart = threading.Thread(target=self.heartbeat_loop, daemon=True)
        stick.start()
        heart.start()

        before = {}
        regrab_at = int(self.args.regrab_after)
        kill_at = int(self.args.kill_after)
        for second in range(1, int(self.args.duration) + 1):
            time.sleep(1.0)
            if kill_at and second >= kill_at:
                # os._exit 不走 atexit、不发 DISCONNECT、不跑 finally —— 对端看到的
                # 就是"控制台没了"。地面站靠 deadman（500ms 归零）+ 心跳超时（两个心跳
                # 周期）发现，然后弹"云端控制已断开"。
                print(f"  ** HARD KILL at t+{second}s (simulating the console dying)")
                sys.stdout.flush()
                os._exit(7)
            before = self.report_second(second, before)
            if regrab_at and second == regrab_at:
                # 再抢一次控制权，模拟"云端二次接管"（官方网页端上就是再点一次"一键起飞"旁边的接管）。
                # 曾经用它绕过地面站的一个 bug：操作员点"同意"之后控制权没有真的交出去，
                # 只有再发一次抢权才会落到 _cloudFlightAuthority = true。
                # 该 bug 已在 DjiDrcClient::drcRespondAuth() 里修掉，现在同意即生效，
                # 这个开关只留给"云端确实二次接管"这一场景。
                print(f"  ** re-sending flight_authority_grab at t+{second}s (cloud re-seize)")
                self.send_services("flight_authority_grab", {})

        print("\n--- stage 4: stopping sticks, drc_mode_exit ---")
        self.stop_flag.set()
        stick.join(timeout=3)
        heart.join(timeout=3)
        time.sleep(0.5)
        self.exit_drc()
        time.sleep(1.0)

        self.summary()
        self.client.loop_stop()
        self.client.disconnect()
        return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.144.110")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--sn", default="dgcs001", help="gcsSn, i.e. the cloud's gateway_sn")
    ap.add_argument("--user", default="pilot")
    ap.add_argument("--password", default="pilot123")
    ap.add_argument("--drc-client-id", default="", help="client_id handed to the ground station for the DRC link")
    ap.add_argument("--duration", type=int, default=40, help="seconds of stick streaming")
    ap.add_argument("--regrab-after", type=int, default=0,
                    help="re-send flight_authority_grab this many seconds into stage 3 "
                         "(models the cloud taking control a second time after the local "
                         "operator consented); 0 = never")
    ap.add_argument("--skip-grab", action="store_true",
                    help="skip stage 2 (flight_authority_grab). The ground station asks the local "
                         "operator to consent, and that dialog needs a real click — with this flag "
                         "the whole run stays click-free, at the price of cloudFlightAuthority "
                         "staying false")
    ap.add_argument("--kill-after", type=int, default=0,
                    help="hard-kill this process (os._exit, no drc_mode_exit, no MQTT DISCONNECT) "
                         "this many seconds into stage 3 — models the cloud console dying mid-flight, "
                         "which is what makes the ground station raise the 'control lost' popup")
    ap.add_argument("--pattern", choices=["sweep", "fixed", "zero"], default="sweep")
    ap.add_argument("--pattern-period", type=float, default=16.0, help="seconds for one full axis sweep")
    ap.add_argument("--fixed-x", type=float, default=0.0)
    ap.add_argument("--fixed-y", type=float, default=0.0)
    ap.add_argument("--fixed-h", type=float, default=0.0)
    ap.add_argument("--fixed-w", type=float, default=0.0)
    args = ap.parse_args()

    rc = RemoteController(args)
    try:
        return rc.run()
    except KeyboardInterrupt:
        print("\ninterrupted; tearing down")
        rc.stop_flag.set()
        rc.exit_drc()
        rc.summary()
        rc.client.loop_stop()
        rc.client.disconnect()
        return 130


if __name__ == "__main__":
    sys.exit(main())
