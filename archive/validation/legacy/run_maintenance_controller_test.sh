#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=$(mktemp /tmp/stc8g1k08-maintenance-controller-test.XXXXXX)
trap 'rm -f "$binary"' EXIT HUP INT TERM

gcc -std=c11 -Wall -Wextra -Werror -DMAINTENANCE_CONTROLLER_HOST_TEST \
    -I"$root/Driver/inc" \
    "$root/maintenance_controller_test.c" \
    "$root/Driver/src/maintenance_controller.c" \
    -o "$binary"
"$binary"
