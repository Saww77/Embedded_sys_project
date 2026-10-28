# Buddy 1: Wi-Fi, MQTT commands and binary telemetry

This implements Buddy 1 against the uploaded repository. Project descriptions
supply scope; the new minimum-bit requirement supersedes text application
payloads. The professor's service-ownership guidance determines the bridge.
MQTT remains in `lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c`.

## Architecture and startup

```
other subsystem tasks -> s1_publish_* -> TX message buffer
                                             |
                    kernel Wi-Fi task -> mqtt_utk.c -> broker
                                             ^          |
Buddy 1 communication task <- RX message buffer <- callbacks
```

Only the Wi-Fi task calls lwIP/MQTT. Both message buffers are created by
`subsystem1_init()` before subsystem tasks start. The Wi-Fi task can start
earlier, so an acquire/release readiness gate publishes both handles only
after successful creation. A failed second allocation deletes the first.
There is one initialization caller. Buffer handles are immutable afterward.

All bridge operations use `TMO_POL`. The service processes at most two TX
frames per 2ms iteration. Buddy 1 handles at most four commands per 100ms
iteration; it never calls network APIs. RX fragments and topics are copied
before callbacks return. A recognized topic is converted into its fixed
three-bit dictionary ID before enqueueing, rather than storing a pointer or
repeating its string in every message. Binary bytes, including NUL, are preserved.

Existing Wi-Fi association/DHCP/rejoin remains owned by `cyw43_utk.c`. The
MQTT poll runs for both link-up and link-down. MQTT connection attempts and
failed subscriptions back off 3 seconds. Each accepted connection installs
callbacks and restores the command subscription. Failed SUBACKs retry too.
No application timer calls lwIP; no extra reconnect/heartbeat timeout chains.

Heartbeat is a retained binary status byte every 5 seconds; a retained one-byte
zero Last Will marks offline after broker detection. It does not imply an
instantaneous Wi-Fi-loss indication: allow MQTT's 60s keepalive and broker
failure detection. The local `s1_connection_status()` snapshot clears connection
bits on link loss. Drop bits are sticky until reboot, and describe service-side
drops; caller-side queue rejection is reported through the API return value.

## Build and network configuration

Use Pico SDK 2.2.0 with its `tinyusb`, `cyw43-driver`, and `lwip` submodules
initialized, plus `arm-none-eabi-gcc`, make and a host C++ compiler as in the
repository README. No worktree is required; use your existing isolated checkout.

```sh
cp config/wifi_secrets.example.h config/wifi_secrets.h
# Edit the ignored wifi_secrets.h with local Wi-Fi and broker settings.
# Use a broker IPv4 address, a unique MQTT_CLIENT_ID and a unique topic root.
cd build_make
make -j4 WIFI=cyw43 NET=lwip MQTT=1
# On-target self-tests (includes codec checks):
make -j4 WIFI=cyw43 NET=lwip MQTT=1 TEST=1
```

The example uses `robot/se09/v1` as the topic root. Topics must fit 63 bytes
including suffix; an overlong configured root causes a compilation error.
The fallback root for an older secrets header is `robot/<MQTT_CLIENT_ID>/v1`.
`MQTT_TOPIC` from the old heartbeat example is no longer used.

This is the provided lwIP TCP MQTT stack on port 1883, without broker login or
TLS. Use a trusted lab network and a broker configured for that access. Do not
expose remote STOP on a public unauthenticated broker. Never commit secrets.
With MQTT disabled, publishing returns `E_NOSPT` and the existing LED continues.

## Communication API

Include `app_program/subsystem1/s1_comm.h` from task code.

| API | Valid input | Behavior |
| --- | --- | --- |
| `s1_publish_vehicle(state, faults)` | state 0..7; five-bit faults | vehicle/controller snapshot |
| `s1_publish_line(ir, barcode)` | IR bitmask 0..7; barcode 0 none, 1..4 A..D | one-byte line snapshot |
| `s1_publish_terrain(height, phase)` | height 0..2000 mm; phase 0..5 | two-byte terrain snapshot |
| `s1_publish_obstacle(nearest, width, left, right)` | distances 0..4000 mm; width 0..2000 mm | six-byte obstacle snapshot |
| `s1_connection_status()` | none | atomic five-bit link/drop snapshot |

Return `E_OK` means copied into TX, not broker delivery. `E_TMOUT` means full,
`E_PAR` rejects invalid input, and `E_OBJ` means not initialized. Callers choose
whether to retry. Never call these APIs in an ISR. Pass data copied under the
owning subsystem's synchronization; the API cannot make a racy source snapshot
safe. Enqueued buffers are copied, so stack-local input is safe after return.
No new shared packed structure is updated by multiple owners.

The motion snapshot is published automatically every second using Buddy 2's
existing nine-byte `S2_STATUS`; unavailable motion is omitted. Its progress
fields are forwarded exactly as Buddy 2 defines them; raw encoder totals are
not exposed by the current motion public API. Sensor and vehicle topics are
published only when their owners invoke the APIs. Buddy 3 currently contains
the template consumer, so there is no real line/barcode source to wire up yet.
Buddy 4/5 integration is left to their owners. State and terrain phase meanings
must be agreed with the controller; their numerical ranges are enforced.
The initial measurement bounds above are explicit protocol limits, not claims
of calibrated sensor performance. Out-of-range input is rejected, never wrapped
or silently saturated. Revise the protocol/version if required ranges grow.

## Commands and acknowledgments

Publish binary data to `<root>/command`, QoS 0, retain=false:

| Command | Payload hex | Handling |
| --- | --- | --- |
| STOP | `00` | call Buddy 2's priority stop latch; no remote movement/start |
| SNAPSHOT | `01` | request current motion status |
| PERIOD | opcode 2 bits + ticks 7 bits, little-endian | interval 1..100 ticks of 100ms |
| example PERIOD 1000ms | `2a00` | 10 ticks |
| example PERIOD 10000ms | `9201` | 100 ticks |

STOP and SNAPSHOT require exactly one byte with all unused bits zero. PERIOD
requires exactly two bytes with padding zero and ticks in range. Opcode 3,
malformed lengths, unused bits and out-of-range values are rejected. PERIOD
also requests an immediate snapshot. No command controls direction or speed;
the robot's navigation remains autonomous.

`<root>/ack` is one byte: bits 0..1 opcode, bit 2 rejected, bits 3..7 zero.
ACK means Buddy 1 dispatched/accepted the request, not that a motion completed
or that motors physically stopped. ACK publication is best effort. There is
no command identifier or exactly-once guarantee; these commands are idempotent.
Do not retain commands. This lwIP callback does not expose the incoming retain
flag, so the receiver cannot enforce that policy. Clear any retained commands
on the broker before testing. A network STOP is not a certified emergency stop:
packet loss, disconnection or a full RX queue can prevent delivery.

## Bounded-memory and loss policy

TX and RX each allocate a 256-byte kernel ring. Maximum frame is 33 bytes:
three-bit topic ID in one byte plus 1..32 payload bytes. Actual v1 frames are
2..10 bytes. Each kernel record also has a four-byte header and four-byte
alignment. A full ring rejects the newest message without blocking. No queue
or payload grows dynamically after initialization.

Wrong/overlong topics, zero-length or >32-byte incoming payloads and fragment
length mismatches drop the whole message. No truncated command is executed.
The RX callback copies its topic into a 64-byte staging array and maps it to
command ID 7; fragments use a 32-byte staging array. The final kernel send
copies into RX before return. Non-final fragments are never visible to tasks.

Offline TX telemetry is drained/discarded (two frames per poll), not guaranteed
to replay. `ERR_MEM` holds one pending frame for at most 5 seconds and retries
on later service polls. Other immediate failures and expiry drop that frame.
All telemetry and commands use QoS 0, so there is no delivery guarantee or
application retry after lwIP accepts a publish. Do not infer successful broker
delivery from queue acceptance. The service's sticky drop flags aid diagnosis.

## Demo

On the PC, install the optional demo dependency in a virtual environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install paho-mqtt==2.1.0
python3 tools/buddy1_mqtt.py 192.168.1.10 listen
# Second terminal, same venv:
python3 tools/buddy1_mqtt.py 192.168.1.10 snapshot
python3 tools/buddy1_mqtt.py 192.168.1.10 period --ticks 20
python3 tools/buddy1_mqtt.py 192.168.1.10 stop
```

Raise the wheels before testing STOP and motion reporting. Confirm decoded
`/status`, `/motion`, and `/ack`, then restart the broker and power-cycle the
AP: confirm heartbeat recovery, a restored command subscription and a valid
new snapshot. Have each sensor owner call its publication API and compare
known boundary values. Measure latency and motion-control jitter under queue
load. Record actual logs and video; the host mocks do not replace this demo.

See `testing/subsystem1/TEST_LOG_BUDDY1.md` for executed checks and build blockers.
