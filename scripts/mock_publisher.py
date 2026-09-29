#!/usr/bin/env python3
"""
Aquaponics Edge Simulator - Standalone Mock Publisher
Emulates ESP32 firmware telemetry behavior over MQTT for quick local UI testing.
"""

import argparse
import datetime
import json
import random
import signal
import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Error: 'paho-mqtt' is required. Install with: pip install paho-mqtt")
    sys.exit(1)


class AquaponicsSimulator:
    def __init__(self, mode="NORMAL", seed=42):
        if seed:
            random.seed(seed)
        self.mode = mode
        self.ph = 6.80
        self.temp = 23.50
        self.ec = 1.50
        self.do = 7.20

    def step(self):
        ph_noise = (random.random() * 2 - 1) * 0.02
        temp_noise = (random.random() * 2 - 1) * 0.08
        ec_noise = (random.random() * 2 - 1) * 0.015
        do_noise = (random.random() * 2 - 1) * 0.04

        if self.mode == "PH_DRIFT":
            self.ph += -0.05 + ph_noise
            self.temp += (23.50 - self.temp) * 0.05 + temp_noise
            self.ec += (1.50 - self.ec) * 0.05 + ec_noise
            self.do += (7.20 - self.do) * 0.05 + do_noise
        elif self.mode == "HIGH_TEMPERATURE":
            self.ph += (6.80 - self.ph) * 0.05 + ph_noise
            self.temp += 0.25 + temp_noise
            self.ec += 0.01 + ec_noise
            self.do += -0.08 + do_noise
        elif self.mode == "LOW_DISSOLVED_OXYGEN":
            self.ph += (6.80 - self.ph) * 0.05 + ph_noise
            self.temp += (23.50 - self.temp) * 0.05 + temp_noise
            self.ec += (1.50 - self.ec) * 0.05 + ec_noise
            self.do += -0.20 + do_noise
        else:  # NORMAL
            self.ph += (6.80 - self.ph) * 0.05 + ph_noise
            self.temp += (23.50 - self.temp) * 0.05 + temp_noise
            self.ec += (1.50 - self.ec) * 0.05 + ec_noise
            self.do += (7.20 - self.do) * 0.05 + do_noise

        # Clamp to physical boundaries
        self.ph = max(0.0, min(14.0, self.ph))
        self.temp = max(0.0, min(50.0, self.temp))
        self.ec = max(0.0, min(5.0, self.ec))
        self.do = max(0.0, min(20.0, self.do))

        return {
            "ph": round(self.ph, 2),
            "temperature": round(self.temp, 2),
            "ec": round(self.ec, 2),
            "dissolvedOxygen": round(self.do, 2),
        }


def main():
    parser = argparse.ArgumentParser(description="Aquaponics Edge MQTT Mock Publisher")
    parser.add_argument("--broker", default="localhost", help="MQTT broker hostname")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("--device-id", default="esp32-sim-01", help="Edge Device ID")
    parser.add_argument("--interval", type=float, default=5.0, help="Publish interval (seconds)")
    parser.add_argument(
        "--mode",
        choices=["NORMAL", "PH_DRIFT", "HIGH_TEMPERATURE", "LOW_DISSOLVED_OXYGEN"],
        default="NORMAL",
        help="Operational simulation scenario",
    )
    parser.add_argument("--seed", type=int, default=42, help="Random seed for reproducibility")
    args = parser.parse_args()

    client_id = f"mock-{args.device_id}"
    topic_telemetry = f"aquaponics/devices/{args.device_id}/telemetry"
    topic_status = f"aquaponics/devices/{args.device_id}/status"

    client = mqtt.Client(client_id=client_id)

    # Set Last Will & Testament (LWT)
    lwt_payload = json.dumps({
        "deviceId": args.device_id,
        "status": "offline",
        "reason": "unexpected_disconnect"
    })
    client.will_set(topic_status, payload=lwt_payload, qos=1, retain=True)

    print(f"Connecting to MQTT broker at {args.broker}:{args.port}...")
    try:
        client.connect(args.broker, args.port, keepalive=60)
    except Exception as e:
        print(f"Failed to connect to broker: {e}")
        sys.exit(1)

    client.loop_start()

    # Announce online status (Retained)
    online_payload = json.dumps({"deviceId": args.device_id, "status": "online"})
    client.publish(topic_status, payload=online_payload, qos=1, retain=True)

    sim = AquaponicsSimulator(mode=args.mode, seed=args.seed)
    running = True

    def sig_handler(sig, frame):
        nonlocal running
        print("\nShutting down publisher...")
        running = False

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    print(f"Publishing telemetry for '{args.device_id}' [Mode: {args.mode}] every {args.interval}s...")
    print(f"Telemetry Topic: {topic_telemetry}")
    print(f"Status Topic:    {topic_status}\n")

    try:
        while running:
            readings = sim.step()
            now_iso = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
            payload = {
                "deviceId": args.device_id,
                "timestamp": now_iso,
                "readings": readings
            }
            json_str = json.dumps(payload)
            print(f"[{now_iso}] pH={readings['ph']} | Temp={readings['temperature']}°C | EC={readings['ec']} | DO={readings['dissolvedOxygen']} mg/L")
            client.publish(topic_telemetry, payload=json_str, qos=1)
            time.sleep(args.interval)
    finally:
        shutdown_payload = json.dumps({
            "deviceId": args.device_id,
            "status": "offline",
            "reason": "graceful_shutdown"
        })
        client.publish(topic_status, payload=shutdown_payload, qos=1, retain=True)
        time.sleep(0.5)
        client.loop_stop()
        client.disconnect()
        print("Disconnected cleanly.")


if __name__ == "__main__":
    main()
