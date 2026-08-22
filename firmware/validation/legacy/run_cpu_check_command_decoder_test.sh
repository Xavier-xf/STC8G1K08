#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=$(mktemp /tmp/stc8g1k08-cpu-check-decoder-test.XXXXXX)
trap 'rm -f "$binary"' EXIT HUP INT TERM

gcc -std=c11 -Wall -Wextra -Werror -DCPU_CHECK_COMMAND_DECODER_HOST_TEST \
    -I"$root/Driver/inc" \
    "$root/cpu_check_command_decoder_test.c" \
    "$root/Driver/src/cpu_check_command_decoder.c" \
    -o "$binary"
"$binary"
