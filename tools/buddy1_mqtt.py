#!/usr/bin/env python3
"""Buddy 1 binary MQTT demo client (pip install paho-mqtt==2.1.0)."""
import argparse
import json
import threading


def bits(data, start, count):
    return (int.from_bytes(data, "little") >> start) & ((1 << count) - 1)


def decode(topic, data):
    name = topic.rsplit("/", 1)[-1]
    expected = {"status": 1, "vehicle": 1, "line": 1, "terrain": 2,
                "obstacle": 6, "motion": 9, "ack": 1}
    if name not in expected or len(data) != expected[name]:
        return {"hex": data.hex(), "error": "unknown topic or incorrect payload length"}
    if name == "status":
        return dict(zip(("wifi", "mqtt", "subscribed", "tx_drop", "rx_drop"),
                        (bool(data[0] & (1 << i)) for i in range(5))))
    if name == "vehicle":
        return {"state": bits(data, 0, 3), "fault_mask": bits(data, 3, 5)}
    if name == "line":
        return {"ir_bits": bits(data, 0, 3), "barcode": bits(data, 3, 3)}
    if name == "terrain":
        return {"height_mm": bits(data, 0, 11), "phase": bits(data, 11, 3)}
    if name == "obstacle":
        return {"nearest_mm": bits(data, 0, 12), "width_mm": bits(data, 12, 11),
                "left_mm": bits(data, 23, 12), "right_mm": bits(data, 35, 12)}
    if name == "motion":
        return {"mode": bits(data, 0, 2), "speed_level": bits(data, 2, 3),
                "faults": bits(data, 5, 4), "left_rpm": bits(data, 9, 9),
                "right_rpm": bits(data, 18, 9), "left_pwm": bits(data, 27, 7),
                "right_pwm": bits(data, 34, 7), "left_progress": bits(data, 41, 13),
                "right_progress": bits(data, 54, 13)}
    return {"opcode": bits(data, 0, 1), "rejected": bool(data[0] & 2)}


def command_payload(command):
    if command == "stop":
        return b"\x00"
    if command == "start":
        return b"\x01"
    raise ValueError("command must be start or stop")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="MQTT broker IPv4 address")
    parser.add_argument("action", choices=("listen", "start", "stop"))
    parser.add_argument("--root", default="robot/se09/v2")
    parser.add_argument("--port", type=int, default=1883)
    args = parser.parse_args()
    try:
        import paho.mqtt.client as mqtt
    except ImportError as exc:
        raise SystemExit("Install demo dependency: python3 -m pip install paho-mqtt==2.1.0") from exc
    if args.action != "listen":
        try:
            payload = command_payload(args.action)
        except ValueError as exc:
            parser.error(str(exc))
    connected = threading.Event()
    failure = []
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    def on_connect(connection, userdata, flags, reason, properties):
        if reason.is_failure:
            failure.append(str(reason))
        elif args.action == "listen":
            connection.subscribe(args.root + "/#", qos=0)
        connected.set()

    def on_message(connection, userdata, message):
        print(message.topic, json.dumps(decode(message.topic, message.payload)), flush=True)

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, args.port, 60)
    client.loop_start()
    try:
        if not connected.wait(10) or failure:
            raise SystemExit("MQTT connection failed: " + (failure[0] if failure else "timeout"))
        if args.action == "listen":
            print("Listening; Ctrl-C to exit", flush=True)
            while True:
                threading.Event().wait(1)
        else:
            result = client.publish(args.root + "/command", payload, qos=0, retain=False)
            result.wait_for_publish(timeout=5)
            if not result.is_published():
                raise SystemExit("Publish did not complete")
            print("Sent", payload.hex(), "(transport only; inspect /ack on listener)")
    except KeyboardInterrupt:
        pass
    finally:
        client.disconnect()
        client.loop_stop()


if __name__ == "__main__":
    main()
