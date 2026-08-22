#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
firmware_root=$(CDPATH= cd -- "$root/../.." && pwd)
binary=$(mktemp /tmp/stc8g1k08-maintenance-runtime-test.XXXXXX)
trap 'rm -f "$binary"' EXIT HUP INT TERM

gcc -std=c11 -Wall -Wextra -Werror -DMAINTENANCE_RUNTIME_HOST_TEST \
    -I"$firmware_root/Driver/inc" \
    "$root/maintenance_runtime_test.c" \
    "$firmware_root/Driver/src/maintenance_runtime.c" \
    -o "$binary"
"$binary"
