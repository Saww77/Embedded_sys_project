"""Buddy 1 protocol v2: binary command payloads; no firmware imports."""

def command_payload(command):
    if command == "start":
        return b"\x01"
    if command == "stop":
        return b"\x00"
    raise ValueError("command must be start or stop")
