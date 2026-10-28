# Buddy 1: heartbeat and MQTT START/STOP demo

The immediate demo needs no sensor collection. The Pico publishes a binary
heartbeat; MQTT START asks Buddy 2 to drive forward; STOP asks it to stop.
The optional sensor publication APIs remain available for later integration
but are never called automatically by this demo.

## Only a Pico W and Wi-Fi available?

Use **[the Pico-only API test](PICO_ONLY_TEST.md)**. Build with
`WIFI=cyw43 NET=lwip MQTT=1 BUDDY1_API_TEST=1`; it logs the public forward/stop
API calls over USB serial and excludes all real motor/encoder code. It is
immediately ready and needs no motor hardware. The rest of this guide describes
the real Buddy 2 motor demo (`BUDDY1_API_TEST=0`). Both use real MQTT and the
same command task and binary command format.

## Build and flash

Use Pico SDK 2.2.0 with its `tinyusb`, `cyw43-driver` and `lwip` submodules,
`arm-none-eabi-gcc`, make and host g++ (repository README). Work in the existing
isolated checkout; do not create a worktree just for this workflow.

```sh
cp config/wifi_secrets.example.h config/wifi_secrets.h
# Edit config/wifi_secrets.h: Wi-Fi name/password, broker IPv4 address,
# MQTT_CLIENT_ID (unique), MQTT_TOPIC_ROOT "robot/se09/v2".
cd build_make
make -j4 WIFI=cyw43 NET=lwip MQTT=1 BUDDY1_DEMO=1
```

Output: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_b1demo.uf2`.
Hold BOOTSEL while attaching USB and copy that UF2 to RPI-RP2. View the USB
serial console at 115200 baud. Raise the wheels for the first motor test.

The demo builds and initializes only Buddy 1 and Buddy 2. Their init functions
run before their application tasks start. No sensor subsystems or automatic
motor-test producer are started. Normal `BUDDY1_DEMO=0` retains all five buddies
and still has the original Buddy 3/4 sample-interface errors. Demo source
selection is a build option, not a change to their code. Buddy 2 requires
SMP=1 for its core-1 encoder IRQ ownership, and uses GP0/GP1 for encoders:
use the USB console and CRASH_REPORT=0. Conflicting demo profiles are rejected.

Use the wiring and calibrated configuration in
`app_program/subsystem2/motion_config.h`. Buddy 2 takes five seconds to become
ready after its task starts. START before readiness is rejected; send a new
START after readiness. Firmware compilation does not validate your wiring.

## MQTT topics and compact binary commands

The default root is **`robot/se09/v2`**. If updating an existing ignored
`wifi_secrets.h`, change its root from v1 to v2 explicitly. Fallback for a
header without MQTT_TOPIC_ROOT is `robot/<MQTT_CLIENT_ID>/v2`.
V1 snapshot/interval commands have been removed. A new topic version prevents
old snapshot messages from being interpreted as START.

| Topic | Direction | Binary payload |
| --- | --- | --- |
| `<root>/status` | Pico -> broker | 1 byte, retained, every 5 seconds |
| `<root>/command` | PC -> Pico | `00` STOP; `01` START; QoS 0, **not retained** |
| `<root>/ack` | Pico -> PC | 1 byte: command bit 0, rejected bit 1 |

Command payloads contain one application bit and seven zero padding bits.
They are bytes, **not the strings `START`, `STOP`, `0` or `1`**. Exactly one byte
is required; extra bytes, zero length and nonzero padding are rejected.

ACK values: `00` STOP accepted, `01` START submitted, `02` malformed STOP-like
payload rejected, `03` START rejected. ACK means Buddy 1 accepted/submitted
requests, not that a physical movement completed. Buddy 2 can subsequently
reject a queued request (e.g. settling, stale request or fault); Buddy 1 checks
for velocity mode after four polling ticks and stops if it did not start.
A successful START ACK is not proof of motor movement. ACK delivery is best effort.

Heartbeat bits: bit 0 Wi-Fi/IP up, bit 1 MQTT connected, bit 2 command
subscription acknowledged, bit 3 sticky service TX drop, bit 4 sticky RX drop.
Bits 5..7 are zero. The retained Last Will is `00` (offline). A broker detects
failure using TCP/keepalive; it is not instantaneous. Keepalive is 60 seconds.

### Motor behavior

- Power-up is stopped. START submits `motion_set_speed(1)` followed by
  `motion_forward()`. Level 1 means 25% of Buddy 2's configured target RPM,
  **not 25% PWM**. Buddy 2 alone owns pins, encoder handling and PID.
- While active, Buddy 1 submits a speed renewal every four 50ms task sleeps
  (approximately 200ms plus scheduler/timer overhead), satisfying Buddy 2's
  existing 1-second velocity watchdog. START is not periodically reissued.
- Duplicate START while active is idempotent. A START is rejected if offline,
  not subscribed, not ready, faulted or busy with another request. A failed
  submission calls the priority stop to cancel partial queue submissions.
- STOP calls `motion_stop()` and clears the active request. STOP dominates
  any START later in the same drained batch. No further speed renewals occur.
- Observed Wi-Fi/MQTT/subscription loss, motion faults, unavailable status,
  rejected start or failed renewal stops requested motion and clears the latch.
  Reconnecting never restarts it automatically; a new START is required.
- The command task checks every 50ms sleep; Buddy 2 enforces its own scheduling,
  watchdog and fault protection. Actual stop latency requires measurement.
  A silent network partition may take MQTT keepalive time to become visible.
  A network STOP is not a substitute for a physical power-off switch.

Do not retain commands or run a repeated START publisher. Clear any old retained
command at your configured root before testing. The lwIP raw callback does not
expose the retain flag, so firmware cannot reject a retained START on that basis.
Use a trusted lab broker: the provided stack is plaintext TCP on port 1883,
without broker login or TLS. Keep Wi-Fi credentials out of Git.

## Run the PC demo

Set up an optional client virtual environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install paho-mqtt==2.1.0
python3 tools/buddy1_mqtt.py 192.168.1.10 listen
```

In another terminal using that venv, after the robot has booted for five seconds:

```sh
python3 tools/buddy1_mqtt.py 192.168.1.10 start
python3 tools/buddy1_mqtt.py 192.168.1.10 stop
```

Replace the address with your broker. Commands send a single non-retained byte.
`start` exits after sending; the firmware maintains motion until STOP or a
failure. The listener prints decoded heartbeat and ACK. It does not expect
sensor telemetry. Custom roots use `--root robot/<your-id>/v2` on both clients.

With mosquitto tools instead:

```sh
python3 -c 'import sys; sys.stdout.buffer.write(bytes([1]))' > /tmp/start.bin
python3 -c 'import sys; sys.stdout.buffer.write(bytes([0]))' > /tmp/stop.bin
mosquitto_sub -h 192.168.1.10 -t 'robot/se09/v2/#' -F '%t %x'
# Another terminal (do not add -r):
mosquitto_pub -h 192.168.1.10 -t robot/se09/v2/command -f /tmp/start.bin
mosquitto_pub -h 192.168.1.10 -t robot/se09/v2/command -f /tmp/stop.bin
```

## Bridge and ownership

```
Buddy 1 command task -> copied TX message buffer -> mqtt_utk.c -> broker
Buddy 1 command task <- copied RX message buffer <- mqtt_utk.c <- broker
           |
           +-> Buddy 2 motion API -> motor-owner task -> hardware
```

MQTT/lwIP calls remain exclusively in the kernel Wi-Fi service. The service
polls at 2ms intervals, including while the link is down. Heartbeat is emitted
there; no application task calls lwIP. Connection/subscription failures back off
3 seconds. Every accepted connection reinstalls callbacks and subscriptions.

The bridge initialization publishes both immutable handles through an
acquire/release byte only after both buffers exist; partial allocation rolls
back. Each buffer is a 256-byte kernel ring; maximum copied frame is 33 bytes
(three-bit dictionary ID in one byte plus up to 32 payload bytes). Commands and
ACKs currently use 2-byte frames. Kernel records have a four-byte header and
four-byte alignment. A full queue rejects newest; no task blocks on a bridge
operation. Application API errors report rejected sends to the caller.

RX copies the topic into a 64-byte staging array and maps it to dictionary ID 7;
fragments are copied into a 32-byte staging array. The completed payload and
route ID are copied to RX before callback return. Wrong/overlong topics,
length mismatches or oversized payloads drop the entire message; no truncated
command is executed. Topics and bytes never cross tasks as borrowed pointers.
The service processes at most two TX frames per poll, discards offline TX,
and holds an ERR_MEM pending frame for no longer than five seconds. QoS 0 is
best effort; queue success does not guarantee delivery. Shared status flags
have one writer, with whole-byte atomic publication for other tasks.

## Future sensor integration

The `s1_publish_vehicle`, `s1_publish_line`, `s1_publish_terrain` and
`s1_publish_obstacle` task-context APIs remain in `s1_comm.h`. Valid inputs and
bit encodings are in `DATA_REPRESENTATION.md`; owners must supply coherent
snapshots. They return E_OK when copied, E_TMOUT when full, E_PAR for invalid
input, E_OBJ before initialization, or E_NOSPT without MQTT. They are not ISR
APIs. The command-only demo does not call them and fabricates no readings.

## Validation

Host tests exercise canonical binary encoding, bridge initialization/copying,
fragmentation/bounds, retries/reconnects, START/STOP, speed renewals, connection
loss, motion faults, queue failures and no automatic restart. Full demo release
and TEST=1 images are built against real SDK/lwIP headers. Target execution,
physical motor operation and network round trips still require your Pico W.
See `testing/subsystem1/TEST_LOG_BUDDY1.md` for exact evidence and limits.
