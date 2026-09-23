"""只读旁听无人机 osd，用来判断"飞控到底执行了没有"。

存在的理由：`drone_control` 应答 result=0 只证明地面站收下了指令，
证明不了飞控执行了它。而 attitude_* 比地速/位移更早反应 —— 位置控制器
收下杆量后先给倾角，然后才有位移。所以飞机在地面上时，姿态角是唯一
看得见的证据。

不参与 DRC，不建任何会话，纯订阅 thing/product/+/osd。

用法:  python tools/osd_watch.py [--seconds 15] [--sn drone001]
"""
import argparse
import json
import math
import time

import paho.mqtt.client as mqtt

HOST = "192.168.144.110"
PORT = 1883
USER = "pilot"
PASSWORD = "pilot123"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=HOST)
    ap.add_argument("--port", type=int, default=PORT)
    ap.add_argument("--user", default=USER)
    ap.add_argument("--password", default=PASSWORD)
    ap.add_argument("--sn", default="", help="只记这台飞机；留空则自动挑带 horizontal_speed 的那台")
    ap.add_argument("--seconds", type=float, default=15.0)
    args = ap.parse_args()

    samples = []
    t0 = None
    picked = [args.sn] if args.sn else []

    def on_connect(client, userdata, flags, reason_code, properties=None):
        client.subscribe("thing/product/+/osd", qos=1)
        print(f"connected rc={reason_code}  recording {args.seconds:.0f}s")

    def on_message(client, userdata, msg):
        nonlocal t0
        try:
            payload = json.loads(msg.payload.decode("utf-8", "replace"))
        except Exception:
            return
        # osd 也走 DJI 信封 {bid,data,tid,timestamp,gateway}，真字段在 data 里。
        # 直接在顶层找 horizontal_speed 会一条都匹配不上（这里踩过一次）。
        data = payload.get("data") or {}
        # 地面站自己的 osd 是另一套字段（live_status 等），只认飞控那份
        if "horizontal_speed" not in data:
            return
        sn = msg.topic.split("/")[2]
        if not picked:
            picked.append(sn)
        if sn != picked[0]:
            return

        if t0 is None:
            t0 = time.time()
        t = time.time() - t0
        row = (t,
               data.get("horizontal_speed"), data.get("vertical_speed"),
               data.get("elevation"), data.get("attitude_pitch"),
               data.get("attitude_roll"), data.get("attitude_head"),
               data.get("latitude"), data.get("longitude"), data.get("mode_code"))
        samples.append(row)
        print(f"t={t:6.2f}  spd={_f(row[1])}  vspd={_f(row[2])}  alt={_f(row[3])}  "
              f"pitch={_f(row[4])}  roll={_f(row[5])}  head={_f(row[6],0)}  "
              f"mode={row[9]}  @{row[7]:.6f},{row[8]:.6f}", flush=True)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="osd-watch")
    client.username_pw_set(args.user, args.password)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, args.port, 30)
    client.loop_start()
    time.sleep(args.seconds)
    client.loop_stop()
    client.disconnect()

    print(f"\n--- {len(samples)} 个样本 / {args.seconds:.0f}s"
          f"（{len(samples)/max(args.seconds,1e-9):.1f} Hz），sn={picked[0] if picked else '无'} ---")
    if len(samples) < 2:
        print("样本太少，判定不了。")
        return

    print(f"  地速   min={min(s[1] for s in samples):8.3f}  max={max(s[1] for s in samples):8.3f}")
    print(f"  倾角   min={min(s[4] for s in samples):8.3f}  max={max(s[4] for s in samples):8.3f}")
    print(f"  横滚   min={min(s[5] for s in samples):8.3f}  max={max(s[5] for s in samples):8.3f}")
    print(f"  高程   min={min(s[3] for s in samples):8.3f}  max={max(s[3] for s in samples):8.3f}")

    lat1, lon1 = samples[0][7], samples[0][8]
    lat2, lon2 = samples[-1][7], samples[-1][8]
    dla = (lat2 - lat1) * 111320.0
    dlo = (lon2 - lon1) * 111320.0 * math.cos(math.radians(lat1))
    print(f"  位移   ({lat1:.6f},{lon1:.6f}) -> ({lat2:.6f},{lon2:.6f})"
          f"  净位移={math.hypot(dla, dlo):.2f} m")


def _f(v, w=8):
    return "  (无)  ".rjust(w) if v is None else f"{v:{w}.2f}"


if __name__ == "__main__":
    main()
