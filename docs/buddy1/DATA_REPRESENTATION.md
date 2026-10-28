# Buddy 1 binary protocol v2 and RAM evidence

The current demo publishes heartbeat and command ACKs only. Sensor APIs remain
for future use; they are not required or called by the demo. Bit offsets count
from the least-significant bit of byte 0. Explicit masks/shifts define the wire
format; no compiler-native bit-field layout is transmitted. Octet padding is
zero. The changed command meanings use a new `/v2` MQTT topic root.

## Active demo fields

| Field | Valid range | Required bits | Implementation |
| --- | --- | ---: | --- |
| command | 0 STOP, 1 START | 1 | payload byte bit 0; bits 1..7 zero |
| ACK command | 0..1 | 1 | payload byte bit 0 |
| ACK rejected | 0..1 | 1 | payload byte bit 1; bits 2..7 zero |
| Wi-Fi/IP up | boolean | 1 | shared status byte bit 0 |
| MQTT connected | boolean | 1 | shared status byte bit 1 |
| subscribed | boolean | 1 | shared status byte bit 2 |
| service TX drop latched | boolean | 1 | shared status byte bit 3 |
| RX drop latched | boolean | 1 | shared status byte bit 4 |
| topic dictionary ID | 0..7 | 3 | copied frame byte 0 bits 0..2 |
| command-task phase | 0 stopped, 1 pending, 2 running | 2 | private bit-field |
| command-task renewal counter | 0..4 | 3 | same private one-byte control struct |
| STOP seen in current batch | boolean | 1 | task-local byte flag |

Each heartbeat, command and ACK payload is one byte. An RX command or TX ACK
bridge frame is two bytes (dictionary byte + payload byte), with no native
struct padding or string terminator. Heartbeat is emitted directly by the
network owner and needs no TX queue frame. There is no application sequence
counter, periodic sensor collection or separate fault boolean per field.

`S1_CONTROL` uses five bits in one byte; a static assertion verifies its size.
It belongs exclusively to the command task. Its native bit-fields never cross
a task boundary or appear on the wire. Kernel IDs, clock values, temporary
arithmetic and library callback parameters retain their ABI-required widths.

## Optional sensor API fields

| Payload / field | Valid range | Bits | Bit offset |
| --- | --- | ---: | ---: |
| vehicle state | 0..7 | 3 | 0 |
| motor/encoder/IMU/no-echo/Wi-Fi faults | five independent flags | 5 | 3 |
| line IR | three booleans | 3 | 0 |
| barcode | 0 none, 1..4 A..D | 3 | 3 |
| terrain height, mm | 0..2000 | 11 | 0 |
| terrain phase | 0..5 | 3 | 11 |
| nearest obstacle, mm | 0..4000 | 12 | 0 |
| obstacle width, mm | 0..2000 | 11 | 12 |
| left / right clearance, mm | 0..4000 each | 12 each | 23 / 35 |

Vehicle/line/terrain/obstacle payloads are respectively 1/1/2/6 bytes. Bounds
are initial protocol limits, not calibrated physical claims. Out-of-range
values are rejected. Sensor owners must agree state/phase semantics and widen
or version the protocol if needed. None of these APIs reads a sensor itself.

Buddy 2's synchronized nine-byte status is read only for motion control. Mode
bits 0..1 and fault bits 5..8 are checked without changing its encoding. Buddy 2
owns its existing RPM, progress, fault, PID and encoder representations; this
work does not certify the whole firmware against the minimum-bit requirement.

## Message-size evidence

Run `testing/subsystem1/host/measure_sizes.c`; measured sizeof results are in
`logs/message_sizes.txt`. Explicitly labeled uint32_t reference layouts compare
vehicle 24 -> 1, line 8 -> 1, terrain 8 -> 2, obstacle 16 -> 6 and command
4 -> 1 bytes. These reference sensor structures did not exist in the original
Buddy 1 template; they are not historical before measurements. MQTT topic and
protocol headers and kernel queue headers are excluded from payload sizes.

The original heartbeat sent `hb N`: 4 bytes for `hb 0`, up to 13 bytes for a
32-bit count. The binary heartbeat is always 1 byte. Its meaning changes from
a demo counter to connection/drop flags, so this is not an equal-semantics
counter comparison. START and STOP now each require just one application bit.

## RAM evidence and limitations

GNU Arm 14.2.1, Cortex-M0+, -O2, SDK 2.2.0: original mqtt_utk.o BSS was 208
bytes; current mqtt_utk.o is 169 and mqtt_bridge.o 10; command/codec objects
have zero BSS. Their summed BSS is 179, a 29-byte static-section reduction.
See `logs/object_sizes.txt` for current object sizes. RX staging drops from
128 to 32 payload bytes; topic staging remains 64; one pending TX frame is 33.

Overall RAM does not decrease versus the original LED/demo feature set: two
new kernel buffers allocate 512 bytes and a new task requests a 2048-byte
stack, plus allocator/kernel metadata and alignment. Bridge readiness is one
atomic byte; the five published connection flags share one atomic byte.

The complete Buddy 1 + Buddy 2 demo now links and its ELF size is recorded in
`logs/demo_build.txt`. This is not an equal-target before/after comparison:
the original five-buddy firmware cannot link due to its existing interfaces,
and the demo intentionally omits unfinished sensor subsystems. Do not claim a
whole-system RAM reduction from different source selections. Runtime free-heap,
stack-watermark, timing and physical performance measurements remain target work.

## Synchronization and performance evidence

Kernel message buffers copy complete messages with kernel SMP synchronization.
An acquire/release byte publishes both immutable queue handles together. The
network owner is the sole writer of link/drop flags and atomically publishes
a whole byte. No shared packed application word uses an unprotected
read-modify-write. Optional API callers must provide coherent owned snapshots.

All Wi-Fi-context queue calls use TMO_POL and TX work is capped at two frames
per poll. Command processing is capped at four messages per 50ms task sleep.
The application renews velocity requests roughly every 200ms, below Buddy 2's
one-second watchdog under normal scheduling. Actual scheduling jitter and
physical stop time are not inferred from compile success.

Host assertions plus AddressSanitizer/UndefinedBehaviorSanitizer validate all
256 one-byte and 65,536 two-byte command inputs, fragment copying, full queues,
startup rollback, connection/subscription retries, START/STOP, duplicate START,
STOP dominance, readiness/fault/queue rejection, speed renewal, owner rejection
and stop-on-loss without reconnect auto-resume. These are mocks, not hardware
or network-throughput measurements.
