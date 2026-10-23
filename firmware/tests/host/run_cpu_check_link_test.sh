#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
firmware_root=$(CDPATH= cd -- "$root/../.." && pwd)
binary=$(mktemp /tmp/stc8g1k08-cpu-check-link-test.XXXXXX)
trap 'rm -f "$binary"' EXIT HUP INT TERM

for test in cpu_check_link_test cpu_check_link_16bit_test; do
    gcc -std=c11 -Wall -Wextra -Werror -DCPU_CHECK_LINK_HOST_TEST \
        -I"$firmware_root/Driver/inc" \
        "$root/$test.c" \
        "$firmware_root/Driver/src/cpu_check_link.c" \
        -o "$binary"
    "$binary"
done
