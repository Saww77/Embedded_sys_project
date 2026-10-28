#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
output=$(mktemp /tmp/buddy1-host.XXXXXX)
trap 'rm -f "$output"' EXIT HUP INT TERM
${CC:-cc} -DTM_MQTT=1 -std=c11 -Wall -Wextra -Werror -pedantic -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Itesting/subsystem1/host/include \
    testing/subsystem1/host/test_buddy1.c \
    lib/liblwip/sysdepend/pico_rp2040/mqtt_bridge.c \
    app_program/subsystem1/s1_wire.c \
    app_program/subsystem1/s1_comm.c -o "$output"
"$output"
${CC:-cc} -DTM_MQTT=1 -DAPP_BUDDY1_API_TEST=1 -std=c11 -Wall -Wextra -Werror -pedantic -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Itesting/subsystem1/host/include \
    testing/subsystem1/host/test_motion_stub.c \
    app_program/subsystem1/s1_motion_stub.c \
    app_program/subsystem1/s1_comm.c \
    app_program/subsystem1/s1_wire.c -o "$output"
"$output"
