# Buddy 1 validation record

Executed in the cloud against the uploaded ZIP. No Pico W is attached.

| Check | Result | Evidence / limit |
| --- | --- | --- |
| Host codec + message-buffer + service + application tests | PASS, five groups | `logs/host_tests.txt`; assertions enabled, ASan/UBSan |
| Exhaustive one-/two-byte command validation | PASS | 256 one-byte + 65,536 two-byte inputs |
| Terrain + line value round trips | PASS | all 2001 x 6 terrain combinations; all 8 x 5 line combinations |
| Obstacle boundary/offset round trips | PASS | 4001 varied distance vectors; invalid upper bounds rejected |
| Initialization order and allocation rollback | PASS (mock) | no MQTT before both handles; failed RX allocation deletes TX |
| Queue-copy lifetime, binary NUL and fragmented RX | PASS (mock) | overwrite source after callback/send, inspect received bytes |
| Full/oversize/malformed RX and TX budgets | PASS (mock) | whole-message rejection, nonblocking kernel calls, two TX attempts/poll |
| MQTT backoff, SUBACK failure, reconnect subscription | PASS (mock) | injected failures, deterministic clock; Wi-Fi loss clears flags |
| Command dispatch and typed telemetry API | PASS (mock) | STOP latch called once; invalid command rejected; snapshot and interval ACKs |
| ARM selected-object build matrix | PASS, five profiles | `logs/cross_compile.txt`; real SDK/lwIP headers, no mock headers |
| Original vs updated object RAM | measured | `logs/object_sizes.txt`; whole-firmware/runtime RAM outstanding |
| Complete production firmware build | FAIL, pre-existing integration | Buddy 3 `SAMPLE_MSG`, `mpf_samples`, `sem_slots`; Buddy 4 latter two |
| Original archive Buddy 3 compile | FAIL, same identifiers | confirms failure predates these changes |
| On-target tests S1-U01..U05 | compiled, NOT RUN | TEST=1; no hardware |
| Wi-Fi/broker round trip and reconnect | NOT RUN on hardware | follow `docs/buddy1/README.md` demo |
| Real-time jitter, latency, live RAM/stack headroom | NOT MEASURED | hardware evidence still required |

The full-build failures are not hidden by disabling buddies or replacing their
code. No UF2 is supplied as a working integrated image. Buddy 2, 3, 4 and 5
source files are unchanged. The uploaded Buddy 3 is the template consumer, not
a completed line/barcode implementation.

## Reproduce

From the project root:

```sh
testing/subsystem1/host/run.sh
cc -std=c11 -Wall -Wextra -Werror testing/subsystem1/host/measure_sizes.c -o /tmp/s1-sizes
/tmp/s1-sizes
```

This cloud command sandbox uses tracing that prevents LeakSanitizer execution.
The recorded test run used `ASAN_OPTIONS=detect_leaks=0` for that environment;
AddressSanitizer bounds/use-after-free and UndefinedBehaviorSanitizer stayed
enabled. The tested code has no host heap allocations. The script's default
retains leak checking where supported.

Selected-object matrix: SMP=1/TEST=0/MQTT=1, SMP=1/TEST=1/MQTT=1,
SMP=0/TEST=1/MQTT=1, SMP=1/TEST=0/MQTT=0 and SMP=1/TEST=1/MQTT=0.
MQTT profiles used WIFI=cyw43, NET=lwip, USB console; disabled profiles used
WIFI=none, NET=none, UART console. Every subsystem1 object and, when enabled,
both MQTT port objects and the modified CYW43 service were compiled.

## Integration follow-up for other owners

Restore a coherent interface between Buddy 2 and the remaining template
consumer/monitor, or replace those templates with the real Buddy 3/4 controllers.
Do not restore unused legacy sample types merely to hide the mismatch. Then run
the full build and target tests. Connect real sensor owners to `s1_publish_*`;
agree on vehicle states, terrain phases, fault ownership and measurement bounds.
Run a lab broker, clear retained command topics, and use the supplied demo
client. Record real serial output, MQTT captures and timing/RAM measurements
for Week 13. Do not mark those checks as passed based on these host mocks.
