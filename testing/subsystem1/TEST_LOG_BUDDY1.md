# Buddy 1 heartbeat + START/STOP validation

Current protocol: v2. No physical Pico W is attached to the cloud environment.

| Check | Result | Scope |
| --- | --- | --- |
| Host codec/bridge/service/application tests | PASS, five groups | logs/host_tests.txt; ASan/UBSan, assertions enabled |
| Canonical commands | PASS | all 256 one-byte values; all 65,536 two-byte values rejected |
| Optional sensor encoding | PASS | all terrain/line values; 4001 obstacle vectors and invalid bounds |
| Bridge ownership/limits/init rollback | PASS in mocks | copied fragments/topic mapping, binary NUL, full queue, partial init failure |
| Service retry/reconnect/heartbeat | PASS in mocks | connect and subscription failures, bounded TX, ERR_MEM expiry, link-loss flags |
| Application START/STOP | PASS in mocks | ready/fault/busy/queue gates, low-speed forward, duplicate START, STOP dominance |
| Velocity watchdog renewals | PASS in mocks | level-1 speed renewal every four task ticks; failed renewal stops |
| Loss/reconnect/owner rejection | PASS in mocks | stop and clear run request; restoring link does not restart |
| Demo client binary encoding/decoding | PASS, two Python tests | no broker dependency |
| Complete release demo ELF + UF2 | PASS | real ARM toolchain + SDK; BUDDY1_DEMO=1, TEST=0 |
| Complete test demo ELF + UF2 | PASS | BUDDY1_DEMO=1, TEST=1; tests compiled, not executed on target |
| Selected-object compatibility profiles | PASS, five profiles | logs/cross_compile.txt; invalid demo options rejected |
| Serial-only API stubs | PASS in host tests | correct signatures, range checks, simulated mode and log messages |
| Command task -> actual test stubs | PASS in host tests | START/STOP frames dispatched, serial logs and accepted ACKs verified |
| Complete Pico-only API image | PASS | BUDDY1_API_TEST=1, real network stack; no real Buddy 2 source linked |
| Switch back to real motor demo | PASS | original motion_transport implementation and control task restored |
| Normal five-subsystem profile | original integration errors remain | SAMPLE_MSG, mpf_samples, sem_slots in Buddy 3/4 |
| Physical Wi-Fi/MQTT and motor operation | NOT RUN | requires your board, wiring and lab network |
| Live latency/jitter/heap/stack measurements | NOT MEASURED | do not substitute host test or ELF size for runtime evidence |

Build evidence: `logs/demo_build.txt`. Historical full-profile failures remain
in `logs/full_build_errors.txt` and `logs/original_archive_errors.txt`. Demo mode
changes source selection and startup calls only; buddies 2..5 source is unchanged.
No motor autotest is started. Build images used public placeholder network
configuration; configure your own ignored wifi_secrets.h and rebuild before use.

## Reproduce

```sh
# From repo root:
ASAN_OPTIONS=detect_leaks=0 sh testing/subsystem1/host/run.sh
PYTHONDONTWRITEBYTECODE=1 python3 testing/subsystem1/host/test_demo.py
cc -std=c11 -Wall -Wextra -Werror testing/subsystem1/host/measure_sizes.c -o /tmp/s1-sizes
/tmp/s1-sizes
# With ARM compiler on PATH and PICO_SDK_PATH set:
cd build_make
make -j4 WIFI=cyw43 NET=lwip MQTT=1 BUDDY1_DEMO=1
make -j4 WIFI=cyw43 NET=lwip MQTT=1 BUDDY1_DEMO=1 TEST=1
```

This cloud command sandbox uses tracing that blocks LeakSanitizer. The recorded
run disables only leak detection; AddressSanitizer memory-error checking and
UndefinedBehaviorSanitizer remain enabled. There are no host heap allocations
in the tested bridge/controller code. Outside that sandbox the default host
script keeps leak checking enabled.

## Pico-only API-dispatch test

For users without motor hardware, follow `docs/buddy1/PICO_ONLY_TEST.md`. The
API-test image replaces Buddy 2's public functions with serial-logging stubs;
normal builds still use its real code. `logs/api_test_build.txt` records the
linked image sizes and symbol/source exclusion checks. Real MQTT/USB behavior
still requires flashing the Pico. A stub log must never be presented as evidence
that Buddy 2's hardware implementation ran successfully.

## On-board checklist (real motor demo)

1. Confirm existing Buddy 2 wiring; raise wheels before first START.
2. Configure a trusted broker and a fresh v2 topic root. Do not retain commands.
3. Boot: confirm heartbeat while motors remain stopped. Wait five seconds for
   Buddy 2 readiness; START before readiness should be rejected, not deferred.
4. Send binary START 01: confirm ACK and forward operation at speed level 1.
   Wait longer than one second to verify speed renewals prevent watchdog expiry.
5. Send binary STOP 00: confirm ACK, physical stop and no subsequent renewal.
6. Start again after the motor settling interval; disconnect AP or broker.
   Verify stop after failure is detected, and no auto-restart after reconnect.
7. Record serial/MQTT logs, stop latency, timing jitter, heap and stack headroom.

A silent partition may require the MQTT keepalive interval before disconnection
is observed. Network STOP is best effort; keep physical motor power control
available. A START ACK is submission confirmation, not proof the owner accepted
or executed the movement. Compare the actual motor behavior during the demo.
