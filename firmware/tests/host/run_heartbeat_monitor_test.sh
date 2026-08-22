#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
firmware_root=$(CDPATH= cd -- "$root/../.." && pwd)
binary=$(mktemp /tmp/stc8g1k08-heartbeat-monitor-test.XXXXXX)
trap 'rm -f "$binary"' EXIT HUP INT TERM

gcc -std=c11 -Wall -Werror -DHEARTBEAT_MONITOR_HOST_TEST \
    -I"$firmware_root/Driver/inc" \
    "$root/heartbeat_monitor_test.c" \
    "$firmware_root/Driver/src/heartbeat_monitor.c" \
    -o "$binary"
"$binary"
