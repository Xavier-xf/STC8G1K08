#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=$(mktemp /tmp/stc8g1k08-heartbeat-monitor-test.XXXXXX)
trap 'rm -f "$binary"' EXIT HUP INT TERM

gcc -std=c11 -Wall -Werror -DHEARTBEAT_MONITOR_HOST_TEST \
    -I"$root/Driver/inc" \
    "$root/heartbeat_monitor_test.c" \
    "$root/Driver/src/heartbeat_monitor.c" \
    -o "$binary"
"$binary"
