# Minimum-bit representation and memory evidence

Bit offsets increase from bit 0 (LSB of byte 0), with explicit masks/shifts.
No compiler-dependent packed struct or native C bit-field is transmitted.
Each independent MQTT publication is rounded to whole octets; padding bits
are zero, never application data. `/v1` identifies the protocol version.

| Topic/field | Valid range | Bits | Offset / representation |
| --- | --- | ---: | --- |
| internal route ID | 0..7 | 3 | frame byte 0 bits 0..2 |
| status Wi-Fi/IP up | 0..1 | 1 | status bit 0 |
| status MQTT accepted | 0..1 | 1 | status bit 1 |
| status command subscription acknowledged | 0..1 | 1 | status bit 2 |
| status service TX drop latched | 0..1 | 1 | status bit 3 |
| status service RX drop latched | 0..1 | 1 | status bit 4 |
| vehicle/controller state | 0..7 | 3 | vehicle bits 0..2 |
| motor, encoder, IMU, no-echo, Wi-Fi faults | each 0..1 | 5 total | shared fault mask in vehicle bits 3..7 |
| motion mode (Buddy 2) | 0..3 | 2 | motion bits 0..1 |
| motion speed level (Buddy 2) | 0..4 | 3 | motion bits 2..4 |
| motion faults (Buddy 2) | four independent flags | 4 | motion bits 5..8 |
| left / right RPM (Buddy 2) | 9-bit source representation | 9 each | motion bits 9..17 / 18..26 |
| left / right PWM percent (Buddy 2) | 0..100 | 7 each | motion bits 27..33 / 34..40 |
| left / right progress (Buddy 2) | 13-bit source representation | 13 each | motion bits 41..53 / 54..66 |
| IR left/center/right | three booleans | 3 | line bits 0..2 |
| barcode | 0 none, 1 A, 2 B, 3 C, 4 D | 3 | line bits 3..5 |
| terrain height, mm | 0..2000 | 11 | terrain bits 0..10 |
| terrain phase | 0..5 | 3 | terrain bits 11..13 |
| obstacle nearest distance, mm | 0..4000 | 12 | obstacle bits 0..11 |
| obstacle width, mm | 0..2000 | 11 | obstacle bits 12..22 |
| obstacle left clearance, mm | 0..4000 | 12 | obstacle bits 23..34 |
| obstacle right clearance, mm | 0..4000 | 12 | obstacle bits 35..46 |
| command opcode | STOP 0, SNAPSHOT 1, PERIOD 2 | 2 | command bits 0..1 |
| period in 100ms ticks | 1..100 | 7 | PERIOD bits 2..8 |
| ACK opcode | 0..3 (includes rejected reserved opcode) | 2 | ACK bits 0..1 |
| ACK rejected | 0..1 | 1 | ACK bit 2 |

Motion bytes are forwarded unchanged, not repacked or redefined by Buddy 1.
Buddy 2's progress and measured-RPM bounds must be reviewed by its owner:
this table records that source encoding, not a new physical bound. A bound
of 240 RPM in a configuration comment is explicitly marked unproven there.
Changing that representation is outside Buddy 1's integration scope. Likewise,
this work does not certify the whole firmware's compliance with the change
requirement; other buddies must document and validate their own application data.

## Copied inter-task messages

Frames contain one topic dictionary byte and the exact payload bytes. No raw
pointer, native application struct, string terminator or unused max-size tail is
queued. MQTT topics are static dictionary entries; RX copies the topic string
into staging then stores its ID in RX before callback return.

| Data | Application bits | MQTT payload bytes | Bridge frame bytes |
| --- | ---: | ---: | ---: |
| status | 5 | 1 | 2 when enqueued; service heartbeat bypasses TX |
| vehicle | 8 | 1 | 2 |
| motion | 67 | 9 | 10 |
| line | 6 | 1 | 2 |
| terrain | 14 | 2 | 3 |
| obstacle | 47 | 6 | 7 |
| STOP / SNAPSHOT | 2 | 1 | 2 |
| PERIOD | 9 | 2 | 3 |
| ACK | 3 | 1 | 2 |

The old heartbeat sent `hb N`: 4 bytes for `hb 0`, rising to 13 bytes at the
maximum 32-bit count. The new binary status is always 1 byte (75% smaller than
the first heartbeat). Its information changes from a demo counter to live
connection/drop status, so this is not an equal-semantics counter comparison.

`testing/subsystem1/host/measure_sizes.c` also measures explicitly labeled
native uint32_t reference layouts against implemented encodings. Those sensor
messages did not exist in the original Buddy 1 template and are not claimed as
historical before measurements. Results are saved in `logs/message_sizes.txt`:
vehicle 24 -> 1, line 8 -> 1, terrain 8 -> 2, obstacle 16 -> 6, period command
8 -> 2 bytes. MQTT headers/topic bytes and kernel ring overhead are excluded.

## RAM measurements and tradeoffs

Measured by GNU Arm 14.2.1 `arm-none-eabi-size`, Cortex-M0+, `-O2`, SDK 2.2.0,
SMP=1, TEST=0, WIFI=cyw43, NET=lwip, MQTT=1. The original `mqtt_utk.c` from the
uploaded ZIP was compiled with the same options. These are object-level BSS
sizes, not a linked whole-firmware RAM measurement:

| Component | Original BSS | Updated BSS |
| --- | ---: | ---: |
| mqtt_utk.o | 208 | 169 |
| mqtt_bridge.o | absent | 10 |
| s1_comm.o + s1_wire.o | absent | 0 |
| sum of these object BSS sections | 208 | 179 |

This reduces the measured static sections by 29 bytes. It does **not** reduce
overall system RAM versus the LED/demo implementation: two new kernel buffers
allocate 512 bytes and the new communication task requests a 2048-byte stack,
plus kernel/allocator metadata and alignment. That is at least 2560 additional
runtime bytes before accounting for the 29-byte static reduction. The existing
LED task is retained for compatibility. MQTT client/lwIP allocations also depend
on connection state; the old implementation already used the MQTT stack.

RX payload staging shrinks from 128 to 32 bytes; topic staging stays 64 bytes.
The updated service adds one 33-byte pending TX frame. Kernel IDs use the
kernel's ABI width. The readiness gate occupies one byte for one logical bit;
the five shared status flags occupy a single atomic byte. Full-width monotonic
clock values retain the required 32-bit wrap range. Temporary arithmetic values,
API lengths and library callback parameters use the ABI's natural integer types;
they are not transmitted as padded fields.

Whole-firmware link/map and live free-heap/stack-watermark comparisons are
**outstanding** because the uploaded Buddy 3/4 files fail compilation. After
those owners fix their integration, compare `.data + .bss` in complete map files,
record allocator/stack measurements under the same workload, and retain both
logs. Do not label this partial evidence a completed whole-system RAM comparison.

## Shared-data correctness and timing

- Queue syscalls copy complete messages under the kernel's SMP synchronization.
  No shared packed word is modified with an unprotected read-modify-write.
- An acquire/release byte publishes immutable queue handles after both exist.
- Only the Wi-Fi service modifies the connection/drop mask; it release-stores a
  whole atomic byte for application acquire-loads. ARM object inspection shows
  no external atomic helper dependency.
- Producer snapshots are caller-owned. Motion uses Buddy 2's synchronized getter;
  line/terrain/obstacle callers must use their own coherent-snapshot mechanism.
- The reporting interval belongs to the communication task, stored in a byte
  (valid interval <=100); command wire fields use exactly nine bits.
- All network-context queue operations poll, and TX has a fixed two-frame budget;
  callback copying is bounded by 32 payload bytes and 63 topic characters.
- Host tests use ASan/UBSan and exhaustively check all one- and two-byte command
  payloads, terrain values and line values, plus obstacle boundary round trips.
  Fault injection covers initialization failure, full queues, fragmentation,
  retry/subscription errors, link loss, TX backpressure and command dispatch.
- These demonstrate representation and control-flow correctness in mocks, not
  real-time deadlines. On-Pico command latency, service jitter, stack headroom,
  Wi-Fi reconnection and broker round-trip evidence remain hardware checks.
