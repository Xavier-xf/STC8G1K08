#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
firmware_root=$(CDPATH= cd -- "$root/../.." && pwd)
work=$(mktemp -d /tmp/stc8g1k08-ap-reset-output-test.XXXXXX)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Compile the actual GPIO functions, without C51-only SFR/ISR syntax.
# Fail if their declarations change rather than silently testing copied logic.
python3 - "$firmware_root/User/main.c" "$work/ap_reset_output_under_test.inc" <<'PY'
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text()
parts = []
if re.search(r"^#define P5_CPU_CHECK_MASK\b", source, re.M):
    raise SystemExit("P5.4 belongs to cpu_check_link; main.c must not define its mask")
for name in ("P5_AP_RESET_MASK", "P3_UART_RX_MASK", "P3_UART_TX_MASK"):
    match = re.search(r"^#define " + name + r" .+$", source, re.M)
    if match is None:
        raise SystemExit("missing GPIO mask: " + name)
    parts.append(match.group())
for name in ("ap_reset_release", "ap_reset_assert", "gpio_board_init",
             "monitor_restart", "reset_grace_ms", "apply_reset_output",
             "heartbeat_status_name", "reset_state_name", "app_report"):
    match = re.search(r"^static [\w *]+?" + name + r"\([^)]*\)\n\{\n.*?^\}",
                      source, re.M | re.S)
    if match is None:
        raise SystemExit("missing GPIO function: " + name)
    parts.append(match.group())
pathlib.Path(sys.argv[2]).write_text("\n\n".join(parts) + "\n")
PY

gcc -std=c11 -Wall -Wextra -Werror \
    -DSTC8G1K08_REGS_H -DHEARTBEAT_MONITOR_HOST_TEST -DRESET_CONTROLLER_HOST_TEST \
    -I"$work" -I"$firmware_root/User" -I"$firmware_root/Driver/inc" \
    "$root/ap_reset_output_test.c" \
    "$firmware_root/Driver/src/heartbeat_monitor.c" \
    "$firmware_root/Driver/src/reset_controller.c" \
    -o "$work/test"
"$work/test"
