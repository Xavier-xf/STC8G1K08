#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
firmware_root=$(CDPATH= cd -- "$root/../.." && pwd)
binary=$(mktemp /tmp/stc8g1k08-reset-controller-test.XXXXXX)
trap 'rm -f "$binary"' EXIT HUP INT TERM

gcc -std=c11 -Wall -Werror -DHEARTBEAT_MONITOR_HOST_TEST -DRESET_CONTROLLER_HOST_TEST \
    -I"$firmware_root/Driver/inc" \
    "$root/reset_controller_test.c" \
    "$firmware_root/Driver/src/reset_controller.c" \
    -o "$binary"
"$binary"
