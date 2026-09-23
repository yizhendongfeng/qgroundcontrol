#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""地面站"是否在线"的判别实验：一边听 MQTT 上行、一边采样后端 Redis 的在线键。

后端判在线 = Redis 里 `online:<sn>` 这个键存在（TTL 60 秒），
唯一续期入口是收到 OSD / update_topo 后调的 `setDeviceOnline`。
所以这张表把三件事并排放在一起：

    我们发了没有（TX 计数） | 后端认了没有（ttl 采样）

两列不一致的地方就是 bug 的位置：TX 在涨而 ttl 掉到 -2 → 发的东西后端没收下/路由失败；
TX 不涨 → 我们根本没发。

用法: python tools/gw_online_watch.py [--seconds 150]
"""
import argparse
import json
import subprocess
import threading
import time

import paho.mqtt.client as mqtt

BROKER = "192.168.144.110"
PORT = 1883
USER = "pilot"
PASS = "pilot123"
REDIS_CLI = r"D:\Program Files\Redis\redis-cli.exe"

GCS_SN = "dgcs001"
DRONE_SN = "drone001"


def ttl(key):
    out = subprocess.run([REDIS_CLI, "ttl", key], capture_output=True, text=True)
    try:
        return int(out.stdout.strip())
    except ValueError:
        return -999


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=float, default=150.0)
    ap.add_argument("--gcs", default=GCS_SN)
    ap.add_argument("--drone", default=DRONE_SN)
    args = ap.parse_args()

    tx = {}          # topic 尾 -> 计数
    first_tx = {}    # topic -> 首条时间
    last_tx = {}     # topic -> 末条时间
    topo = []        # update_topo 的到达时刻
    lock = threading.Lock()
    t0 = time.time()

    def on_connect(c, u, f, rc, p=None):
        c.subscribe("thing/product/%s/osd" % args.gcs, qos=1)
        c.subscribe("thing/product/%s/osd" % args.drone, qos=1)
        c.subscribe("sys/product/%s/status" % args.gcs, qos=1)
        print("subscribed  thing/product/%s/osd  +  %s/osd  +  sys/product/%s/status"
              % (args.gcs, args.drone, args.gcs), flush=True)

    def on_message(c, u, msg):
        if msg.topic.startswith("sys/"):
            # update_topo 是唯一能让后台"重新订阅"我们的报文。单看它多久来一次，
            # 就知道周期重播有没有在跑（DGCS 侧 kTopoReannounceMs = 60s）。
            with lock:
                topo.append(time.time())
                n = len(topo)
            print("  TOPO #%-3d t=%6.1fs  update_topo" % (n, time.time() - t0), flush=True)
            return

        sn = msg.topic.split("/")[2]
        key = "gcs" if sn == args.gcs else ("drone" if sn == args.drone else sn)
        try:
            data = (json.loads(msg.payload.decode("utf-8", "replace")).get("data") or {})
        except Exception:
            data = {}
        now = time.time()
        with lock:
            tx[key] = tx.get(key, 0) + 1
            first_tx.setdefault(key, now)
            last_tx[key] = now
            n = tx[key]
        # 只打前 3 条和每 20 条的摘要，避免刷屏
        if n <= 3 or n % 20 == 0:
            print("  TX %-5s #%-4d  t=%6.1fs  fields=%d  %s"
                  % (key, n, now - t0, len(data), ",".join(sorted(data)[:6])), flush=True)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="gw-online-watch")
    client.username_pw_set(USER, PASS)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, 30)
    client.loop_start()

    print("\n  t(s)  ttl(gcs)  ttl(drone)   TX(gcs)  TX(drone)   距上条 TX(s)")
    print("  " + "-" * 62)
    start = time.time()
    try:
        while time.time() - start < args.seconds:
            time.sleep(5)
            now = time.time()
            with lock:
                gc, dc = tx.get("gcs", 0), tx.get("drone", 0)
                gl = now - last_tx["gcs"] if "gcs" in last_tx else float("nan")
            print("  %5.0f  %8d  %10d   %7d  %9d   %12s"
                  % (now - start, ttl("online:" + args.gcs), ttl("online:" + args.drone),
                     gc, dc, ("%.1f" % gl) if gl == gl else "-"), flush=True)
    except KeyboardInterrupt:
        pass

    client.loop_stop()
    client.disconnect()

    print("\n--- update_topo 到达时刻 ---")
    with lock:
        if len(topo) < 2:
            print("  只收到 %d 条 —— 周期重播没在跑" % len(topo))
        else:
            gaps = [topo[i] - topo[i-1] for i in range(1, len(topo))]
            print("  共 %d 条，间隔: %s" % (len(topo), " ".join("%.1fs" % g for g in gaps)))

    print("\n--- 汇总（%.0fs）---" % args.seconds)
    with lock:
        for k in ("gcs", "drone"):
            if k in tx:
                print("  %-5s 共 %d 条  首条 t=%.1fs  末条 t=%.1fs"
                      % (k, tx[k], first_tx[k] - t0, last_tx[k] - t0))
            else:
                print("  %-5s 一条都没收到" % k)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
