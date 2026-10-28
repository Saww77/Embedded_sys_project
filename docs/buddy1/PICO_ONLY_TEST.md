# Pico W + Wi-Fi only: test MQTT-to-API calls

No motors, encoders, sensors or dashboard are required. This mode keeps real
Wi-Fi, lwIP, MQTT and the TX/RX bridge, but replaces Buddy 2's implementation
with serial-logging functions having the same public API signatures.

It verifies command delivery and dispatch to the API boundary. It does **not**
verify the real Buddy 2 implementation, encoder handling, PID or motor movement.
Normal motor-demo builds still use the real Buddy 2 code unchanged.

## 1. Configure your broker and Wi-Fi

You need an MQTT broker reachable by both your PC client and the Pico W. Use
an existing lab broker or Mosquitto on your computer. For a local test, its
listener must accept LAN connections on port 1883; localhost-only access will
not work for the Pico. Use the computer's LAN IPv4 address, not 127.0.0.1.
If using WSL, a broker running on Windows is generally easier for LAN access
than a broker inside WSL's default NAT. Allow the listener on your private LAN
firewall only. No dashboard is involved.

From the project root, create `config/wifi_secrets.h` from the example if it
is not already present. Keep this ignored file private:

```c
#define WIFI_SSID       "YOUR_2_4_GHZ_WIFI"
#define WIFI_PASSWORD   "YOUR_PASSWORD"
#define MQTT_BROKER_IP  "192.168.1.10" /* your broker's LAN address */
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID  "se09-pico"
#define MQTT_TOPIC_ROOT "robot/se09/v2"
```

Use a unique client ID/root when multiple robots share a broker. Do not retain
commands. The raw MQTT callback cannot distinguish retained commands; use a
fresh root or clear old retained commands before testing.

## 2. Build the Pico-only image

With your ARM compiler and Pico SDK configured as in the root README:

```sh
cd build_make
make -j4 WIFI=cyw43 NET=lwip MQTT=1 BUDDY1_API_TEST=1
```

`BUDDY1_API_TEST=1` automatically selects `BUDDY1_DEMO=1`. Its source selection
excludes **all** real subsystem2 C files, including motor setup, encoder IRQ
registration, PID and control task. It substitutes `s1_motion_stub.c` in Buddy 1.
Buddy 3..5 remain excluded. The default is API test off; the build profile and
UF2 name distinguish the modes and switching discards incompatible objects.

Flash this exact output using BOOTSEL and the RPI-RP2 drive:

```
mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_b1demo_api.uf2
```

The API test is immediately ready; the real motor demo's five-second readiness
delay does not apply. The stub reports a simulated motion state locally so the
command task can exercise its renewal/STOP behavior. No sensor telemetry is
published or presented as real measurements.

## 3. Open USB serial

On Windows, use PuTTY or a serial monitor on the Pico's COM port at 115200 baud.
On Linux, for example, use `screen /dev/ttyACM0 115200` (adjust the device).

The firmware identifies the build clearly:

```
Buddy 1 API TEST: simulated motion API, no motor driver loaded
[API TEST] ready: motion functions are serial-only stubs
```

If you attach the serial monitor after boot you may miss those startup lines;
START/STOP logs are printed when each command is received.

## Optional START / STOP dashboard

If you prefer buttons, use [the browser dashboard](DASHBOARD.md). It runs in
Ubuntu/WSL with the same broker and this firmware. No rebuild/reflash is needed.
It shows heartbeat/ACK feedback and sends the same binary commands as below.

## 4. Send MQTT commands from your PC

From the project root, create/activate a Python virtual environment and install
the optional client dependency. On Linux/WSL:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install paho-mqtt==2.1.0
python3 tools/buddy1_mqtt.py 192.168.1.10 listen
```

Replace the IP with your broker address. Wait for a `/status` message showing
`wifi`, `mqtt` and `subscribed` all true. Heartbeat repeats every five seconds.

In a second terminal, activate the same venv and send a single START:

```sh
python3 tools/buddy1_mqtt.py 192.168.1.10 start
```

The USB serial monitor should print:

```
[API TEST] motion_set_speed(1) called
[API TEST] motion_forward() called
```

The MQTT listener should show `/ack` with `opcode: 1, rejected: false`.
Repeated speed renewals at the same level are silent to avoid log flooding.
A duplicate START while active is idempotent and does not call forward again.

Then send STOP:

```sh
python3 tools/buddy1_mqtt.py 192.168.1.10 stop
```

Expected USB serial:

```
[API TEST] motion_stop() called
```

Expected ACK: `opcode: 0, rejected: false`. START works again after STOP.
Observed connection loss also calls stop and clears the request. Reconnecting
without a new START must not call forward again.

The script sends compact binary bytes `01`/`00`, not the words START/STOP.
No actual motor pin is driven by the test motion implementation. Record both
serial output and MQTT messages as your API-dispatch evidence.

## 5. Later integration with Buddy 2

Remove `BUDDY1_API_TEST=1` (or pass `BUDDY1_API_TEST=0`) and build the real
motor demo with `BUDDY1_DEMO=1`. That source selection excludes the stubs and
links Buddy 2's original implementation. Verify wiring and raise wheels before
using it. The application command task calls the same API names in either mode.
