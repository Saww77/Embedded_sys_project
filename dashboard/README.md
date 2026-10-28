# Buddy 1 START / STOP dashboard

A standalone browser dashboard. This folder can be copied anywhere; it does
not need the Pico firmware repository, Pico SDK, compiler, RTOS sources or USB
serial access. It communicates only with your existing MQTT broker.

## What you need

- Your Pico W running Buddy 1 v2 firmware with Buddy 2's real motion APIs.
  Rebuild/reflash if you previously used the removed fake-API firmware.
- Your existing Mosquitto broker, running and reachable on TCP port 1883.
- Python 3.8+ and the dependency in requirements.txt on the computer running
  this app. Ubuntu/WSL works; Windows Python works too if installed.
- A browser on that same computer.

## Ubuntu / WSL setup

Extract/copy this folder somewhere on your computer, open Ubuntu and navigate
into it. Run these commands **inside this dashboard folder**:

```sh
sudo apt install -y python3 python3-venv
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r requirements.txt
python3 app.py
```

Enter the actual broker IPv4 address when prompted: the same laptop Wi-Fi
address used by the Pico firmware. Keep this terminal running. Open your
Windows browser at **http://localhost:8080**.

Subsequent runs only need:

```sh
. .venv/bin/activate
python3 app.py
```

Optional direct arguments (replace the example address):

```sh
python3 app.py 192.168.1.10 --root robot/se09/v2 --broker-port 1883 --port 8080
```

Port 1883 is the MQTT broker. Port 8080 is the local browser dashboard.
For Windows Python, use `py -m venv .venv`, activate
`.\.venv\Scripts\Activate.ps1`, then use `python` for the remaining commands.
Ubuntu/WSL avoids needing a separate Windows Python installation.

## Use it

1. Keep Mosquitto and your Pico powered on.
2. Wait for **Pico ready for commands**. START requires a fresh, live
   heartbeat; an old retained status alone does not enable it.
3. Click START, then STOP. Check the latest Pico ACK on the page.
4. START submits `motion_set_speed(1)` and `motion_forward()` to Buddy 2;
   STOP calls `motion_stop()`. Wait at least five seconds after boot for motion
   readiness. ACKs confirm request submission, not physical movement.

Only the broker/topic bar, START/STOP controls and heartbeat panel are shown.
Acknowledgments appear in the feedback line beneath the buttons.
START pauses after 15 seconds without a live ready heartbeat. STOP remains
available while the broker is connected. Send STOP before closing if a motion
request is active; closing the page/app does not itself send STOP.

No broker WebSocket listener is needed. The browser uses the local Python
server, which connects to your normal MQTT TCP listener. Nothing is flashed
onto the Pico by this app. The firmware and dashboard remain separate.

## MQTT protocol

Default topic root: `robot/se09/v2` (must match the Pico firmware).

| Topic | Payload | Direction |
| --- | --- | --- |
| `<root>/command` | one byte: 01 START, 00 STOP | app -> broker -> Pico |
| `<root>/status` | one-byte connection flags, every 5 seconds | Pico -> app |
| `<root>/ack` | bit 0 command, bit 1 rejected | Pico -> app |

Commands use QoS 0 and retain=false. No command is sent automatically on app
startup or broker reconnect. ACK is the latest received on a shared topic,
not a per-client correlated response. The app supports the current trusted-LAN
plaintext broker setup, without broker authentication/TLS.

## Files

- `app.py`: local web server and MQTT connection.
- `mqtt_protocol.py`: the two binary command values.
- `web/`: HTML, CSS and JavaScript dashboard.
- `requirements.txt`: pinned MQTT client dependency.
- `tests/`: state/HTTP and optional broker/browser tests.

## Checks

```sh
python3 tests/test_dashboard.py
```

Optional `tests/test_browser.py` additionally requires paho, Playwright, a
Chromium browser and a Mosquitto executable. It starts a temporary loopback
broker and simulated Pico protocol fixture; it is not a hardware test.

## Troubleshooting

- **Broker disconnected:** check the actual LAN IP, broker listener and
  Windows Firewall access from Ubuntu/WSL. Do not use localhost for a broker
  running on Windows when connecting from WSL unless your networking setup
  specifically supports that route.
- **No live heartbeat:** check the Pico power/network and matching topic root.
- **Localhost page unavailable:** keep app.py running; check the printed URL
  and WSL localhost forwarding. Try `--port 8081` if 8080 is occupied.
- **Missing paho:** activate the virtual environment and install requirements.

Do not put Wi-Fi passwords in this app. It only needs the broker address.
