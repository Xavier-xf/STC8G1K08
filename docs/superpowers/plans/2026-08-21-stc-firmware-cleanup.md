# STC8G1K08 Firmware Directory Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Organize `STC8G1K08/firmware` into clear production, active-test, and legacy-validation areas without changing validated STC behavior, formal Keil source paths, or current build outputs.

**Architecture:** Keep `firmware/User`, `firmware/Driver`, `firmware/RVMDK/STC8G1K08-RESET.uvproj`, and `firmware/RVMDK/list` as the production boundary. Move active host tests to `firmware/tests/host`, the static check to `firmware/tests/static`, and superseded implementations/debug material to `firmware/validation/legacy` while preserving their relative include layout.

**Tech Stack:** Git path moves, POSIX shell test runners, GCC host tests, Keil C51 project paths, Markdown documentation.

---

## File Map

Production files remain at their existing paths:

```text
firmware/User/main.c
firmware/User/Config.h
firmware/Driver/inc/{heartbeat_monitor.h,maintenance_runtime.h,reset_controller.h,stc8g1k08_regs.h,uart.h}
firmware/Driver/src/{heartbeat_monitor.c,maintenance_runtime.c,reset_controller.c,uart.c}
firmware/RVMDK/STC8G1K08-RESET.uvproj
firmware/RVMDK/list/*
```

Active tests move to `firmware/tests/host/` and `firmware/tests/static/`. Legacy code keeps its include/source relationship under `firmware/validation/legacy/Driver/`. One-off debug files move to `firmware/validation/legacy/debug/`. The five explicitly approved temporary backups are deleted. `firmware/validation/` historical snapshots and `firmware/RVMDK/list/` remain unchanged.

### Task 1: Capture the Baseline

**Files:**
- Read: `firmware/RVMDK/STC8G1K08-RESET.uvproj`
- Read: `firmware/RVMDK/list/stc8g1k08_reset.build_log.htm`
- Read: `firmware/RVMDK/list/stc8g1k08_reset.m51`

- [ ] **Step 1: Confirm the STC sub-repository is clean except for the committed design document.**

Run from `STC8G1K08/`:

```bash
git status --short --branch
git log -1 --oneline
```

Expected: branch `main`, no unstaged files, and the latest commit is `docs: add STC firmware cleanup design`.

- [ ] **Step 2: Record the production project references and build baseline.**

```bash
rg -n '<FilePath>|<ListingPath>|<OutputName>|<Device>' firmware/RVMDK/STC8G1K08-RESET.uvproj
rg -n 'Program Size|0 Error\(s\), 0 Warning\(s\)' firmware/RVMDK/list/stc8g1k08_reset.build_log.htm firmware/RVMDK/list/stc8g1k08_reset.m51
```

Expected: the project references only `main.c`, `heartbeat_monitor.c`, `uart.c`, `reset_controller.c`, and `maintenance_runtime.c`; the build reports `data=112.0 xdata=0 code=4725` and zero errors/warnings.

- [ ] **Step 3: Run the pre-move active tests.**

```bash
sh firmware/run_heartbeat_monitor_test.sh
sh firmware/run_reset_controller_test.sh
sh firmware/run_maintenance_runtime_test.sh
sh firmware/reset_firmware_static_test.sh
```

Expected: each command exits with status 0 and prints its existing `OK` result.

- [ ] **Step 4: Commit the implementation plan before any path move.**

```bash
git add docs/superpowers/plans/2026-08-21-stc-firmware-cleanup.md
git commit -m "docs: add STC firmware cleanup implementation plan"
```

### Task 2: Move Active Tests and Update Their Paths

**Files:**
- Create: `firmware/tests/host/`
- Create: `firmware/tests/static/`
- Move: active host test files/runners from `firmware/`
- Move: `firmware/reset_firmware_static_test.sh` to `firmware/tests/static/reset_firmware_static_test.sh`
- Modify: the four moved runner scripts

- [ ] **Step 1: Create test directories and move files with Git history.**

```bash
mkdir -p firmware/tests/host firmware/tests/static
git mv firmware/heartbeat_monitor_test.c firmware/tests/host/heartbeat_monitor_test.c
git mv firmware/reset_controller_test.c firmware/tests/host/reset_controller_test.c
git mv firmware/maintenance_runtime_test.c firmware/tests/host/maintenance_runtime_test.c
git mv firmware/run_heartbeat_monitor_test.sh firmware/tests/host/run_heartbeat_monitor_test.sh
git mv firmware/run_reset_controller_test.sh firmware/tests/host/run_reset_controller_test.sh
git mv firmware/run_maintenance_runtime_test.sh firmware/tests/host/run_maintenance_runtime_test.sh
git mv firmware/reset_firmware_static_test.sh firmware/tests/static/reset_firmware_static_test.sh
```

- [ ] **Step 2: Update each host runner root calculation.**

In each moved host runner, replace the current root declaration with:

```sh
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
firmware_root=$(CDPATH= cd -- "$root/../.." && pwd)
```

Use `"$root/<test>.c"` for the test source, `-I"$firmware_root/Driver/inc"` for headers, and `"$firmware_root/Driver/src/<production-module>.c"` for the production module. Keep compiler flags, temporary-binary cleanup, and final binary invocation unchanged.

- [ ] **Step 3: Update the static check root paths.**

At the top of `firmware/tests/static/reset_firmware_static_test.sh`, use:

```sh
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
firmware_root=$(CDPATH= cd -- "$root/../.." && pwd)
reset="$firmware_root/User/main.c"
config="$firmware_root/User/Config.h"
regs="$firmware_root/Driver/inc/stc8g1k08_regs.h"
project="$firmware_root/RVMDK/STC8G1K08-RESET.uvproj"
logic="$firmware_root/Driver/src/reset_controller.c"
uart="$firmware_root/Driver/src/uart.c"
maintenance_runtime="$firmware_root/Driver/src/maintenance_runtime.c"
maintenance_runtime_header="$firmware_root/Driver/inc/maintenance_runtime.h"
heartbeat="$firmware_root/Driver/src/heartbeat_monitor.c"
heartbeat_header="$firmware_root/Driver/inc/heartbeat_monitor.h"
reset_header="$firmware_root/Driver/inc/reset_controller.h"
```

Keep every assertion and diagnostic message below this block unchanged.

- [ ] **Step 4: Run the moved active tests.**

```bash
sh firmware/tests/host/run_heartbeat_monitor_test.sh
sh firmware/tests/host/run_reset_controller_test.sh
sh firmware/tests/host/run_maintenance_runtime_test.sh
sh firmware/tests/static/reset_firmware_static_test.sh
```

Expected: all four commands pass and no production source file is modified.

- [ ] **Step 5: Commit the active-test organization.**

```bash
git add firmware/tests firmware
git commit -m "test: organize STC active verification files"
```

### Task 3: Archive Superseded Implementations

**Files:**
- Create: `firmware/validation/legacy/Driver/inc/` and `firmware/validation/legacy/Driver/src/`
- Move: the two legacy modules, their headers, tests, and runners

- [ ] **Step 1: Create the preserved relative layout.**

```bash
mkdir -p firmware/validation/legacy/Driver/inc firmware/validation/legacy/Driver/src
```

- [ ] **Step 2: Move the old decoder and maintenance-controller files.**

```bash
git mv firmware/Driver/inc/cpu_check_command_decoder.h firmware/validation/legacy/Driver/inc/cpu_check_command_decoder.h
git mv firmware/Driver/src/cpu_check_command_decoder.c firmware/validation/legacy/Driver/src/cpu_check_command_decoder.c
git mv firmware/cpu_check_command_decoder_test.c firmware/validation/legacy/cpu_check_command_decoder_test.c
git mv firmware/run_cpu_check_command_decoder_test.sh firmware/validation/legacy/run_cpu_check_command_decoder_test.sh
git mv firmware/Driver/inc/maintenance_controller.h firmware/validation/legacy/Driver/inc/maintenance_controller.h
git mv firmware/Driver/src/maintenance_controller.c firmware/validation/legacy/Driver/src/maintenance_controller.c
git mv firmware/maintenance_controller_test.c firmware/validation/legacy/maintenance_controller_test.c
git mv firmware/run_maintenance_controller_test.sh firmware/validation/legacy/run_maintenance_controller_test.sh
```

- [ ] **Step 3: Verify archived runners still compile.**

```bash
sh firmware/validation/legacy/run_cpu_check_command_decoder_test.sh
sh firmware/validation/legacy/run_maintenance_controller_test.sh
```

Expected: both historical tests exit 0. They remain available for reproduction but are not active tests.

- [ ] **Step 4: Verify the formal Keil project does not reference archived modules.**

```bash
! rg -n 'cpu_check_command_decoder|maintenance_controller' firmware/RVMDK/STC8G1K08-RESET.uvproj firmware/User firmware/Driver/inc firmware/Driver/src
```

Expected: no output and status 0.

- [ ] **Step 5: Commit the legacy implementation archive.**

```bash
git add firmware/validation/legacy firmware/Driver firmware
git commit -m "chore: archive superseded STC maintenance modules"
```

### Task 4: Archive Debug Files and Remove Explicit Temporary Backups

**Files:**
- Create: `firmware/validation/legacy/debug/`
- Move: five one-off debug C files
- Delete: five explicitly approved backup files

- [ ] **Step 1: Move one-off debug programs into the legacy archive.**

```bash
mkdir -p firmware/validation/legacy/debug
git mv firmware/debug_exit.c firmware/validation/legacy/debug/debug_exit.c
git mv firmware/debug_exit2.c firmware/validation/legacy/debug/debug_exit2.c
git mv firmware/debug_exit3.c firmware/validation/legacy/debug/debug_exit3.c
git mv firmware/debug_mrt.c firmware/validation/legacy/debug/debug_mrt.c
git mv firmware/maintenance_runtime_integration_example.c firmware/validation/legacy/debug/maintenance_runtime_integration_example.c
```

- [ ] **Step 2: Confirm the backup files are not used by runtime paths.**

```bash
! rg -n 'cpu_check_command_decoder\.h\.orig|Config\.h\.orig|main\.c\.orig|reset_firmware_static_test\.sh\.orig|STC8G1K08-RESET\.uvproj\.Administrator\.tmp' firmware/RVMDK/STC8G1K08-RESET.uvproj firmware/User firmware/Driver firmware/tests
```

Expected: no output and status 0. Documentation references do not count as runtime use.

- [ ] **Step 3: Delete only the approved temporary files.**

```bash
git rm firmware/Driver/inc/cpu_check_command_decoder.h.orig
git rm firmware/User/Config.h.orig
git rm firmware/User/main.c.orig
git rm firmware/reset_firmware_static_test.sh.orig
git rm firmware/RVMDK/STC8G1K08-RESET.uvproj.Administrator.tmp
```

Do not delete or move any file already under `firmware/validation/`, including its `.orig` historical snapshots. Do not alter `firmware/RVMDK/list/`.

- [ ] **Step 4: Commit the debug archive and temporary-file cleanup.**

```bash
git add firmware/validation/legacy firmware
git commit -m "chore: archive STC debug files and temp backups"
```

### Task 5: Update Current Documentation

**Files:**
- Modify: `firmware/README.md`
- Modify: `firmware/验证记录.md`

- [ ] **Step 1: Update `README.md` to the current production baseline.**

The document must state version `1.0.6`, formal inclusion of `maintenance_runtime.c`, enabled P5.4 ENTER/RENEW/EXIT, reference build `data=112.0 xdata=0 code=4725` with zero errors/warnings, active test locations, and the preserved validation archive.

Replace the obsolete instruction not to run `cpu_check_ctl` with this sequence:

```text
1. /home/application/cpu_check_ctl enter
2. stop or replace KCMLobbyPhone while STC remains in maintenance mode
3. use /home/application/cpu_check_ctl renew every 10..20 minutes for long maintenance
4. send /home/application/cpu_check_ctl exit before restarting KCMLobbyPhone
```

- [ ] **Step 2: Update `验证记录.md` without inventing board results.**

Include firmware `1.0.6`; ENTER/RENEW with 3 payload pulses; EXIT with 4; P5.5 high impedance during maintenance; 30-second grace after exit; and the current Keil result. Keep unmeasured board fields as `待填写` and retain ENTER-before-stop, RENEW-while-stopped, EXIT-before-restart order.

- [ ] **Step 3: Run a documentation consistency scan.**

```bash
! rg -n '1\.0\.[012]|不.*执行.*cpu_check_ctl|UART RX 维护|maintenance_runtime.*不编译' firmware/README.md firmware/验证记录.md
rg -n '1\.0\.6|maintenance_runtime|P5\.4|ENTER|RENEW|EXIT|data=112\.0 xdata=0 code=4725' firmware/README.md firmware/验证记录.md
```

Expected: the first command produces no output; the second finds all required current terms.

- [ ] **Step 4: Commit the documentation update.**

```bash
git add firmware/README.md firmware/验证记录.md
git commit -m "docs: align STC firmware records with maintenance baseline"
```

### Task 6: Full Verification and Delivery Review

**Files:**
- Verify: `firmware/tests/`, `firmware/validation/`, `firmware/RVMDK/STC8G1K08-RESET.uvproj`, and `firmware/RVMDK/list/*`

- [ ] **Step 1: Run every active and retained test.**

```bash
sh firmware/tests/host/run_heartbeat_monitor_test.sh
sh firmware/tests/host/run_reset_controller_test.sh
sh firmware/tests/host/run_maintenance_runtime_test.sh
sh firmware/tests/static/reset_firmware_static_test.sh
sh firmware/validation/legacy/run_cpu_check_command_decoder_test.sh
sh firmware/validation/legacy/run_maintenance_controller_test.sh
```

Expected: all six commands exit 0.

- [ ] **Step 2: Verify production project references and output baseline.**

```bash
rg -n '<FilePath>|<OutputName>|<Device>' firmware/RVMDK/STC8G1K08-RESET.uvproj
rg -n 'Program Size|0 Error\(s\), 0 Warning\(s\)' firmware/RVMDK/list/stc8g1k08_reset.build_log.htm firmware/RVMDK/list/stc8g1k08_reset.m51
```

Expected: exactly the five production C source references remain; output name stays `stc8g1k08_reset`; reference build stays `data=112.0 xdata=0 code=4725` with zero errors/warnings.

- [ ] **Step 3: Verify historical snapshots and list outputs remain present.**

```bash
test -f firmware/validation/STC8G1K08-MONITOR.uvproj
test -f firmware/validation/STC8G1K08-SMOKE.uvproj
test -f firmware/validation/RVMDK-list-history/stc8g1k08_monitor.hex
test -f firmware/RVMDK/list/stc8g1k08_reset.hex
test -f firmware/RVMDK/list/stc8g1k08_reset.m51
```

Expected: every command exits 0.

- [ ] **Step 4: Run repository hygiene checks.**

```bash
git diff --check
git status --short --branch
```

Expected: no whitespace errors, unexpected files, production C edits, or Keil project edits.

- [ ] **Step 5: Perform the optional Keil build when Windows Keil is available.**

Open `firmware/RVMDK/STC8G1K08-RESET.uvproj` in Keil C51 and rebuild `STC8G1K08-RESET`. Confirm `Program Size: data=112.0 xdata=0 code=4725` and `0 Error(s), 0 Warning(s)`. If Keil is unavailable, report that limitation and retain the existing verified build log; do not substitute a Linux compiler.

- [ ] **Step 6: Review the delivered commit sequence and clean worktree.**

```bash
git status --short
git log --oneline --max-count=8
```

Expected: the worktree is clean after the final cleanup/documentation commit, with test moves, legacy archive, temporary cleanup, and documentation kept as reviewable commits.
