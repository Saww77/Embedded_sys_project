# START / STOP browser dashboard

**Recommended standalone version:** [`dashboard/`](../../dashboard/) at the
repository top level. Copy that folder anywhere and run `python3 app.py` from
inside it after installing its own `requirements.txt`. It has no imports from
the firmware or tools folders. See [its README](../../dashboard/README.md).

The instructions below retain the earlier `tools/buddy1_dashboard.py` workflow
for compatibility.

The dashboard works with Buddy 1 v2 firmware using Buddy 2's real motion APIs.
If the Pico still has the former fake-API image, rebuild and reflash using
`make -j4 WIFI=cyw43 NET=lwip MQTT=1 BUDDY1_DEMO=1` first. Your existing Mosquitto broker stays on TCP
port 1883; no WebSocket listener or broker configuration change is required.

The browser talks to a small Python server on your laptop. That server uses
paho-mqtt to send the same binary START/STOP commands as the command-line tools.
Use Ubuntu/WSL to run it, since Windows Python was not installed in your setup.

## First run

1. Leave Mosquitto running on Windows. Plug in the Pico W with your existing
   real-motion firmware. Use the motor/encoder wiring agreed with Buddy 2.
2. Open **Ubuntu**, navigate to your project's root folder (the one containing
   `tools`, not `build_make`) and get the latest project update. If your Ubuntu
   terminal is currently in `build_make`, run `cd ..` first.
3. From that project root, run:

```sh
git pull origin main
sudo apt update
sudo apt install -y python3 python3-venv
python3 -m venv .dashboard-venv
. .dashboard-venv/bin/activate
python3 -m pip install -r tools/dashboard/requirements.txt
python3 tools/buddy1_dashboard.py
```

When prompted, enter your **actual broker IPv4 address**: the same laptop Wi-Fi
address used by `MQTT_BROKER_IP` in the Pico's firmware. Empty/invalid addresses
are rejected and prompted again. Do not type `YOUR_BROKER_IP` literally.

Keep this Ubuntu terminal running. Open your **Windows browser** at:

```
http://localhost:8080
```

Port **1883** is the MQTT broker; **8080** is this local dashboard. Open the
localhost address on the same computer running Ubuntu/WSL. The server binds
only to loopback, not to your Wi-Fi interface. Windows-to-WSL localhost
forwarding is supported by the standard WSL setup.

## Use the buttons

- Wait for **Broker connected** and **Pico ready for commands**. A fresh live
  heartbeat enables START; the first retained status from the broker alone
  does not confirm the Pico is currently alive.
- After Buddy 2's five-second readiness delay, click **START** to submit
  `motion_set_speed(1)` and `motion_forward()` to the motion task.
- Click **STOP** to call `motion_stop()` and cancel speed renewals.
- Check the Pico ACK feedback beneath the buttons. ACK means request submission,
  not physical movement or completion. No fake function-call logs are generated.

Duplicate START while active is idempotent; use STOP then START
to see a new forward request.

Commands use topic `robot/se09/v2/command`, one byte `01` for START and `00`
for STOP, QoS 0, retain=false. Neither dashboard startup nor broker reconnect
sends a command automatically. Clicks are posted in order, including STOP
clicked while START is posting. Commands are not automatically retried.

The dashboard receives the shared `/ack` topic, so acknowledgments from another
client also appear. There is no per-request ID in the firmware protocol; the
page deliberately labels them as the latest received ACK rather than proving
which client sent the request.

START is disabled if the broker/monitor is disconnected or no live ready
heartbeat has arrived in 15 seconds. STOP remains available whenever the
broker is connected, including while the Pico heartbeat is stale. The 15-second
UI timeout does not change the firmware's connection detection or motor logic.

Send STOP before closing if a motion request is active. Closing the browser or
dashboard server does not itself send STOP. This is especially relevant when
later using the real motor demo: the Pico keeps its own broker connection.

## Subsequent runs

From the project root in Ubuntu:

```sh
. .dashboard-venv/bin/activate
python3 tools/buddy1_dashboard.py
```

Enter the broker IP again. To provide it directly, for example:

```sh
python3 tools/buddy1_dashboard.py 192.168.1.10
```

Replace that example address with your actual address. Optional arguments:

```sh
python3 tools/buddy1_dashboard.py 192.168.1.10 --root robot/se09/v2 --broker-port 1883 --port 8081
```

The topic root must match `MQTT_TOPIC_ROOT` in your Pico firmware. If changing
the browser port, open the new localhost address printed in the terminal.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `tools/buddy1_dashboard.py` not found | Get the latest code; run from the project root. |
| `No module named paho` | Activate `.dashboard-venv` and install its requirements. |
| Broker disconnected | Use the actual broker LAN IP, keep Mosquitto running, and allow connections from Ubuntu/WSL through Windows Firewall. |
| Broker connected, no live heartbeat | Check Pico power, Wi-Fi credentials, broker IP and matching topic root. Wait for the next 5-second heartbeat. |
| START rejected by Pico | Read the latest ACK/USB serial. With real Buddy 2 firmware, allow its readiness/settling delay and check faults. |
| Browser cannot open localhost | Keep the Ubuntu command running; check the printed port. In standard WSL, localhost forwards to the Linux service. Check WSL localhost-forwarding settings if it does not. |
| Port already in use | Start with `--port 8081` and browse to localhost:8081. |

No Windows Python installation is required when you launch from Ubuntu. Initial
dependency installation requires Internet access; the running dashboard uses
only local assets and your configured MQTT broker. It supports the current
trusted-lab plaintext broker setup; it does not add broker login or TLS.

## Implementation and validation

- `tools/buddy1_dashboard.py`: loopback HTTP server, live MQTT monitor and binary
  command bridge; uses the existing `buddy1_mqtt.command_payload()`.
- `tools/dashboard/`: local HTML/CSS/JavaScript assets and pinned Python dependency.
- `testing/subsystem1/host/test_dashboard.py`: state/HTTP assertions with fake MQTT.
- `testing/subsystem1/host/test_dashboard_browser.py`: optional real Mosquitto +
  Chromium integration test with a simulated Pico protocol fixture.

The browser test verifies click -> HTTP -> real MQTT -> fixture ACK -> display,
exact binary bytes and non-retained QoS 0, rapid START/STOP ordering, stale
heartbeat behavior, broker loss and mobile layout. It does not replace a test
against your physical Pico. Test outcomes are in `logs/dashboard_tests.txt`.

HTTP command posts require a same-origin token and local Host/Origin checks;
assets use a same-origin content security policy. There are no external CDN
scripts, uploaded credentials or changes to the RTOS network ownership model.
The Python virtual environment and bytecode caches are ignored by Git.
