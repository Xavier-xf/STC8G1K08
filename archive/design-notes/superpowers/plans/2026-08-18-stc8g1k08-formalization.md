# STC8G1K08 Formal Firmware Consolidation Implementation Plan

> **For agentic workers:** Execute this plan task-by-task with checkpoints. Keep the validated reset behavior unchanged while completing each testable step.

**Goal:** Consolidate the validated STC reset firmware into one formal Keil project with a `main.c` entry point, apply two evidence-backed V853 fixes, and verify the result through host/static tests and Windows/board acceptance.

**Architecture:** Keep `STC8G1K08-RESET.uvproj` as the only formal target. `main.c` owns hardware initialization, the main loop, and reporting; `heartbeat_monitor.c` and `reset_controller.c` remain pure logic modules. Archive SMOKE/MONITOR snapshots outside the formal target. On V853, keep service lifecycle in `app_run()` and make `video_mode_enable()` display-only; make GPIO release conditional on successful heartbeat-thread join.

**Tech Stack:** Keil C51/uVision XML project, STC8G1K08 SFRs, C11/C++11 V853 application, POSIX pthread/GPIO abstraction, shell static checks, host GCC state-machine tests.

---

### Task 1: Establish the failing acceptance checks

**Files:**
- Modify: `STC8G1K08/firmware/reset_firmware_static_test.sh`
- Modify: `app/common/system_static_test.c`
- Modify: `app/common/cpu_check_heartbeat_static_test.c`
- Test: existing host tests and static-test binaries

- [ ] **Step 1: Update RESET checks to require the formal entry name and version banner**

Change the RESET script variables and assertions from `User/Reset.c` to `User/main.c`, and require:

```sh
reset="$root/User/main.c"
grep -Fq '<FilePath>..\\User\\main.c</FilePath>' "$project"
grep -Fq '#define STC8G1K08_FIRMWARE_VERSION "1.0.0"' "$config"
grep -Fq 'STC8G1K08 reset firmware v' "$reset"
```

Keep the existing device, output, WDT, GPIO, and INT2 assertions. Add a failure if any formal `RVMDK/STC8G1K08-{SMOKE,MONITOR}.uvproj` remains.

- [ ] **Step 2: Add static checks for display-only video mode and safe stop ordering**

Add a `require_count()` helper to `system_static_test.c`, then assert exactly one `sensor_service_init()` and one `front_white_light_init()` in `system.c`, plus the display guard:

```c
ok &= require_count(system_c, "sensor_service_init()", 1U);
ok &= require_count(system_c, "front_white_light_init()", 1U);
ok &= require_contains(system_c,
                       "if (disp == NULL || disp->driver == NULL)");
```

In `cpu_check_heartbeat_static_test.c`, require the join-error block and ensure the GPIO cleanup text occurs after the join-error return block:

```c
ok &= require_contains(source,
                       "if (join_result != 0) {\n            LOG_ERROR");
ok &= require_order(source,
                    "return;\n        }\n    }\n    set_result = gpio_port_set_value",
                    "gpio_port_release(&heartbeat.line);");
```

- [ ] **Step 3: Run the changed checks before implementation**

Run:

```sh
sh STC8G1K08/firmware/reset_firmware_static_test.sh
gcc -std=c11 -Wall -Wextra -Werror app/common/system_static_test.c -o /tmp/system_static_test
/tmp/system_static_test
gcc -std=c11 -Wall -Wextra -Werror app/common/cpu_check_heartbeat_static_test.c -o /tmp/cpu_check_heartbeat_static_test
/tmp/cpu_check_heartbeat_static_test
```

Expected: RESET checks fail because `main.c` and its project path do not yet exist; the V853 checks fail because duplicate initialization and unsafe join cleanup are still present. Record the failures in `progress.md`; do not proceed by weakening the assertions.

### Task 2: Consolidate the STC formal project and entry source

**Files:**
- Rename: `STC8G1K08/firmware/User/Reset.c` -> `STC8G1K08/firmware/User/main.c`
- Modify: `STC8G1K08/firmware/RVMDK/STC8G1K08-RESET.uvproj`
- Modify: `STC8G1K08/firmware/User/Config.h`
- Modify: `STC8G1K08/firmware/Driver/inc/stc8g1k08_regs.h`
- Modify: `STC8G1K08/firmware/reset_firmware_static_test.sh`

- [ ] **Step 1: Rename the production source without changing behavior**

Move the file with `mv`/`git mv`, then preserve its includes, ISR declarations, SFR writes, reset-state transitions, timing constants, and UART text while making only the planned structure changes below.

- [ ] **Step 2: Add the version macro and formal include guard**

In `Config.h`, rename the smoke guard to `STC8G1K08_CONFIG_H` and add:

```c
#define STC8G1K08_FIRMWARE_VERSION "1.0.0"
```

Keep `MAIN_FOSC`, the heartbeat/reset timing, and WDT values unchanged. Add the comment `/* 6 = 128 分频，约 2.1 s @ 24 MHz。 */` immediately above `WDT_PRESCALE`.

In `stc8g1k08_regs.h`, update comments from “冒烟固件” to “正式 RESET 固件”; do not change SFR addresses or pin bindings.

- [ ] **Step 3: Split the entry into `app_init`, `app_report`, and `app_run`**

Keep all helpers `static`. Change the report signature to:

```c
static void app_report(const heartbeat_monitor_t *heartbeat,
                       heartbeat_monitor_ms_t now_ms);
```

`app_report` computes the status from its parameters and emits the existing fields. It must not call `heartbeat_snapshot()`.

Move the current initialization sequence into:

```c
static void app_init(void);
static void app_run(void);
```

`app_run` owns the existing loop body: snapshot, status calculation, `reset_controller_update`, `apply_reset_output`, `wdt_clear`, and once-per-second `app_report(&heartbeat, now_ms)`. `main` becomes:

```c
void main(void)
{
    app_init();
    app_run();
}
```

Add the version to the startup banner without changing line order:

```c
uart1_puts("\r\nSTC8G1K08 reset firmware v" STC8G1K08_FIRMWARE_VERSION "\r\n");
```

- [ ] **Step 4: Update the Keil XML source path**

Change only the source entry:

```xml
<FileName>main.c</FileName>
<FileType>1</FileType>
<FilePath>..\User\main.c</FilePath>
```

Leave target name, include path, memory model, output name, and the two pure logic source entries unchanged.

- [ ] **Step 5: Move validation-only artifacts out of formal build directories**

Create:

```text
STC8G1K08/firmware/validation/
```

Move `Main.c`, `Monitor.c`, `STC8G1K08-SMOKE.uvproj`, `STC8G1K08-MONITOR.uvproj`, the monitor project `.orig` snapshot, and the two smoke/monitor static scripts there as historical snapshots. Add `validation/README.md` stating they are non-production snapshots and are not part of the formal Keil target. Do not move the pure host tests or the formal RESET static test.

- [ ] **Step 6: Run the RESET static check**

Run:

```sh
sh STC8G1K08/firmware/reset_firmware_static_test.sh
```

Expected: `OK: STC8G1K08 RESET 固件静态检查通过`.

### Task 3: Fix V853 display-mode ownership and null handling

**Files:**
- Modify: `app/common/system.c:281-297`
- Modify: `app/common/system_static_test.c`

- [ ] **Step 1: Replace `video_mode_enable` with display-only logic**

Use this shape, preserving existing LVGL calls and transparency values:

```c
void video_mode_enable(int en) {
    lv_disp_t *disp = lv_disp_get_default();

    if (disp == NULL || disp->driver == NULL) {
        LOG_ERROR("video mode: no default display\n");
        return;
    }
    disp->driver->screen_transp = en;
    lv_disp_set_bg_opa(disp, en ? LV_OPA_TRANSP : LV_OPA_COVER);
    lv_obj_set_style_bg_opa(current_screen(),
                             en ? LV_OPA_TRANSP : LV_OPA_COVER,
                             LV_PART_MAIN);
    if (en) {
        video_mode_clear_draw_buffer(disp);
        lv_obj_invalidate(current_screen());
        lv_refr_now(disp);
    }
}
```

Do not call `sensor_service_init()` or `front_white_light_init()` here.

- [ ] **Step 2: Run the focused static test**

Run the `system_static_test` compile/run command from Task 1. Expected: exit 0. Then run the existing app build/static suite available in the repository before moving to the thread fix.

### Task 4: Make heartbeat-thread shutdown safe

**Files:**
- Modify: `app/common/cpu_check_heartbeat.c:41-68,114-149`
- Modify: `app/common/cpu_check_heartbeat_static_test.c`

- [ ] **Step 1: Move loop-local declarations to function scope**

At the top of `cpu_check_heartbeat_thread`, declare `int value; int set_result;`, then remove declarations from inside the loop. Do not change lock boundaries or toggle timing.

- [ ] **Step 2: Gate GPIO cleanup on successful join**

Use this exact shutdown structure:

```c
if (thread_started) {
    int join_result = pthread_join(thread, NULL);

    if (join_result != 0) {
        LOG_ERROR("CPU_CHECK heartbeat thread join failed: %s\n",
                  strerror(join_result));
        return;
    }
}
set_result = gpio_port_set_value(&heartbeat.line, 0);
```

Keep the existing successful cleanup and state reset after the GPIO release. A failed join leaves `initialized` and the GPIO request intact so a later stop call can retry.

- [ ] **Step 3: Run the focused static test and compile the module**

Run:

```sh
gcc -std=c11 -Wall -Wextra -Werror app/common/cpu_check_heartbeat_static_test.c -o /tmp/cpu_check_heartbeat_static_test
/tmp/cpu_check_heartbeat_static_test
```

Expected: exit 0. Then run the app's normal CMake build or the repository's closest available compile target; no `-Werror` warning may be introduced.

### Task 5: Update documentation and verification records

**Files:**
- Modify: `STC8G1K08/firmware/README.md`
- Modify: `STC8G1K08/firmware/验证记录.md`
- Create: `STC8G1K08/firmware/validation/README.md`
- Modify: `progress.md`

- [ ] **Step 1: Rewrite formal build instructions**

Document only `RVMDK/STC8G1K08-RESET.uvproj`, `User/main.c`, and `RVMDK/list/stc8g1k08_reset.hex`. Remove instructions that present SMOKE as the current firmware; link to `validation/README.md` for historical snapshots.

- [ ] **Step 2: Add the version and final board acceptance checklist**

Record the banner version, normal 10-minute run, three pause/reset cycles, startup grace observation, and recovery to `healthy/monitoring`. Leave measurement fields honest where no oscilloscope measurement was performed.

- [ ] **Step 3: Record changed files and test results in `progress.md`**

Include the exact commands and outputs, plus any sandbox/tool errors; do not claim a Windows Keil build was run from Linux.

### Task 6: Verify host logic, static checks, app build, and formal project

**Files:**
- Test: `STC8G1K08/firmware/run_heartbeat_monitor_test.sh`
- Test: `STC8G1K08/firmware/run_reset_controller_test.sh`
- Test: `STC8G1K08/firmware/reset_firmware_static_test.sh`
- Test: focused app static tests and app build

- [ ] **Step 1: Run pure logic tests**

```sh
sh STC8G1K08/firmware/run_heartbeat_monitor_test.sh
sh STC8G1K08/firmware/run_reset_controller_test.sh
```

Expected outputs include `OK: heartbeat monitor state test passed` and `OK: reset controller state test passed`.

- [ ] **Step 2: Run formal RESET static checks and XML sanity checks**

```sh
sh STC8G1K08/firmware/reset_firmware_static_test.sh
rg -n '<FileName>|<FilePath>|<OutputName>|<IncludePath>' \
  STC8G1K08/firmware/RVMDK/STC8G1K08-RESET.uvproj
```

Expected: exactly `main.c`, `heartbeat_monitor.c`, and `reset_controller.c` as source entries; no formal SMOKE/MONITOR projects remain in `RVMDK`.

- [ ] **Step 3: Run the app static/build checks**

Compile/run the focused system and heartbeat static tests, then run the repository's normal app build command. Record success or environment blockers; do not modify SDK files to make a test pass.

- [ ] **Step 4: Windows Keil acceptance**

Open `STC8G1K08-RESET.uvproj` on Windows and require:

```text
compiling main.c...
compiling heartbeat_monitor.c...
compiling reset_controller.c...
0 Error(s), 0 Warning(s)
```

Burn the resulting `RVMDK/list/stc8g1k08_reset.hex` only after that log is clean.

- [ ] **Step 5: Board regression**

Run normal operation for 10 minutes, then perform three separate `ps`/`kill -STOP <PID>` cycles. Verify one reset per pause, reboot banner with the version, no startup false reset during the approximately 16-second V853 boot, and heartbeat recovery. Use `kill -CONT` only to release a process that was intentionally paused.

- [ ] **Step 6: Commit in focused units**

After each verified logical group, commit only the STC formalization, V853 fixes, or documentation/tests that belong to that group. Use outcome-oriented messages such as:

```text
stc: consolidate reset firmware as formal target
app: make video mode display-only and safe
app: retain heartbeat GPIO on join failure
docs: record formal firmware acceptance
```


## Execution Status (2026-08-18)

- Task 1 complete: acceptance checks updated; old implementation failures recorded before fixes.
- Task 2 complete: production entry renamed to `User/main.c`, XML/config/comments updated, validation snapshots archived, RESET static check passed.
- Task 3 complete: `video_mode_enable()` is display-only and null-safe; focused static test passed.
- Task 4 complete: heartbeat thread declarations moved and GPIO cleanup is gated on successful join; focused static test and app build passed.
- Task 5 complete: formal README, verification record, validation README, and progress log updated.
- Task 6 steps 1-3 complete: host logic tests, RESET/XML checks, focused app tests, and `make -C app/build KCMLobbyPhone -j2` passed.
- Task 6 steps 4-5 pending: Windows Keil zero-warning build and v1.0.0 board regression require the user's hardware environment.
- Task 6 step 6 pending: commits are left to the repository owner; no SDK files were modified.
