"""Quick read-only probe: what's on the cloud broker right now.

Talks to the DJI cloud MQTT broker with the pilot credentials from
CloudServer.SettingsGroup.json and dumps a few seconds of traffic so we can see
which SNs are live and whether DGCS's main connection is up.

Usage:  python tools/drc_probe.py [seconds]
"""
import json
import sys
import time

import paho.mqtt.client as mqtt

HOST = "192.168.144.110"
PORT = 1883
USER = "pilot"
PASSWORD = "pilot123"

seen = {}


def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"connected rc={reason_code}")
    client.subscribe("thing/product/#", qos=0)
    client.subscribe("sys/product/#", qos=0)


def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode("utf-8", "replace"))
    except Exception:
        payload = msg.payload[:120]
    method = payload.get("method") if isinstance(payload, dict) else None
    key = (msg.topic, method)
    seen[key] = seen.get(key, 0) + 1
    print(f"[{seen[key]:4d}] {msg.topic}  method={method}")


def main():
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="drc-probe")
    client.username_pw_set(USER, PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(HOST, PORT, 30)
    client.loop_start()
    time.sleep(seconds)
    client.loop_stop()
    client.disconnect()
    print("\n--- summary (topic, method) -> count ---")
    for (topic, method), count in sorted(seen.items()):
        print(f"{count:5d}  {topic}  {method}")


if __name__ == "__main__":
    main()
