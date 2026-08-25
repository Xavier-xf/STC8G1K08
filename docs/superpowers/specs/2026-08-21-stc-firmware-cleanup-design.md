# STC8G1K08 Firmware Directory Cleanup Design

## Background

`STC8G1K08/firmware` now contains the validated RESET firmware, active host-side tests, superseded maintenance implementations, one-off debug programs, historical SMOKE/MONITOR snapshots, and Keil build outputs. The mixed layout makes it difficult to distinguish production inputs from verification material, while the Keil project depends on its current relative source paths.

This phase is a conservative repository cleanup only. The validated STC behavior remains the baseline: P5.4 maintenance protocol, maintenance lease handling, reset state machine, DATA/XDATA placement, watchdog behavior, and generated firmware are unchanged.

## Goals

1. Make the production boundary obvious without changing the paths referenced by the formal Keil project.
2. Group active host tests and static checks under explicit test directories.
3. Preserve superseded implementations and validation history for traceability, but keep them out of production paths.
4. Remove only clearly identified editor/patch backup files from the formal firmware tree.
5. Bring `README.md` and `验证记录.md` in line with the current 1.0.6/P5.4 baseline.

## Non-goals

- Do not modify `sdk/V85X_Tina_V1.3/`.
- Do not change C source behavior, protocol timings, interrupt handling, reset polarity, DATA/XDATA layout, or Keil compiler settings.
- Do not rename or relocate files referenced by `RVMDK/STC8G1K08-RESET.uvproj`.
- Do not delete `firmware/validation/` historical snapshots.
- Do not clean or reduce `firmware/RVMDK/list/` in this phase.
- Do not reintroduce `cpu_check_command_decoder` or `maintenance_controller` into the formal build.

## Production Boundary

These paths stay exactly where they are and remain the only C sources referenced by the formal RESET project:

```text
firmware/User/main.c
firmware/User/Config.h
firmware/Driver/inc/heartbeat_monitor.h
firmware/Driver/inc/maintenance_runtime.h
firmware/Driver/inc/reset_controller.h
firmware/Driver/inc/stc8g1k08_regs.h
firmware/Driver/inc/uart.h
firmware/Driver/src/heartbeat_monitor.c
firmware/Driver/src/maintenance_runtime.c
firmware/Driver/src/reset_controller.c
firmware/Driver/src/uart.c
firmware/RVMDK/STC8G1K08-RESET.uvproj
```

The project-relative paths remain `..\\User` and `..\\Driver\\...`, so opening and building the Keil project does not require project-file edits. The current build evidence (`data=112.0`, `xdata=0`, `code=4725`, zero errors and warnings) is retained as the reference result.

## Proposed Layout

### Active tests

Move the active host tests and their runners to:

```text
firmware/tests/host/
firmware/tests/static/
```

`tests/host/` contains:

```text
heartbeat_monitor_test.c
reset_controller_test.c
maintenance_runtime_test.c
run_heartbeat_monitor_test.sh
run_reset_controller_test.sh
run_maintenance_runtime_test.sh
```

`tests/static/` contains:

```text
reset_firmware_static_test.sh
```

After moving, each runner must derive its root from its new directory and use explicit `../` paths for the production `Driver`, `User`, and `RVMDK` directories. The test commands and compiler flags remain otherwise unchanged. All three host tests and the static check must pass from their new locations.

### Legacy implementations and debug material

Move superseded maintenance code and its tests to:

```text
firmware/validation/legacy/
```

Move the one-off experiments below its debug subdirectory:

```text
firmware/validation/legacy/debug/
```

The legacy set is:

```text
Driver/inc/cpu_check_command_decoder.h
Driver/src/cpu_check_command_decoder.c
cpu_check_command_decoder_test.c
run_cpu_check_command_decoder_test.sh
Driver/inc/maintenance_controller.h
Driver/src/maintenance_controller.c
maintenance_controller_test.c
run_maintenance_controller_test.sh
```

The debug set is:

```text
debug_exit.c
debug_exit2.c
debug_exit3.c
debug_mrt.c
maintenance_runtime_integration_example.c
```

The legacy runners may be retained for historical reproduction, but they are not part of the active test command and must not be referenced by the formal Keil project.

### Historical validation snapshots

Keep the existing `firmware/validation/` SMOKE/MONITOR projects, source snapshots, and `RVMDK-list-history/` outputs. If a second archival move is needed, only move the existing `.orig` files into:

```text
firmware/validation/legacy/source-history/
```

Do not delete or rewrite those snapshots. Their purpose is historical comparison, not current validation.

### Temporary files eligible for deletion

Only these clearly identified backups are eligible for deletion from the formal tree:

```text
Driver/inc/cpu_check_command_decoder.h.orig
User/Config.h.orig
User/main.c.orig
reset_firmware_static_test.sh.orig
RVMDK/STC8G1K08-RESET.uvproj.Administrator.tmp
```

Before deletion, verify each file is not referenced by a script, project file, or documentation and record the paths in the implementation log. No file under `validation/` is included in this deletion list.

## Documentation Updates

Update `firmware/README.md` to describe:

- firmware version `1.0.6`;
- `maintenance_runtime.c` as part of the formal RESET build;
- the P5.4 ENTER/RENEW/EXIT protocol and its operational order;
- maintenance mode, lease expiry, exit grace period, and reset behavior;
- the current build result, including `xdata=0`;
- the new active test locations and the preserved validation archive.

Update `firmware/验证记录.md` to describe:

- the 1.0.6 firmware and P5.4 protocol baseline;
- 3-pulse ENTER/RENEW and 4-pulse EXIT;
- P5.5 high-impedance behavior during maintenance;
- the 30-second post-exit grace period and lease expiry;
- current Keil build evidence and device-level result fields that remain to be filled.

Documentation changes must not claim a board test result that has not been recorded.

## Execution Order

1. Record the clean state of the STC sub-repository and verify the production project references.
2. Create `tests/host`, `tests/static`, and `validation/legacy` directories.
3. Move active tests, then update only their path calculations and run them.
4. Move legacy implementations and one-off debug files; verify no formal project or active test references them.
5. Handle the five explicitly listed temporary files after a reference check.
6. Update `README.md` and `验证记录.md`.
7. Run all active host tests, the static check, `git diff --check`, and a Keil build if Windows Keil is available.
8. Review the final file list and compare it with this design before committing.

Each move or deletion is a separate, reviewable change. If a test fails after a move, restore the path or correct only the path calculation before continuing. Do not change production logic to accommodate directory organization.

## Rollback

Rollback is path-based and recoverable:

- restore moved files to their original paths using the STC sub-repository history;
- restore test scripts before rerunning the old command locations;
- restore any deleted temporary file from the pre-cleanup commit if needed;
- revert documentation independently from source organization.

The rollback procedure never requires reverting `sdk/V85X_Tina_V1.3/` or changing the validated firmware binary.

## Acceptance Criteria

- The formal Keil project opens without path changes and still references exactly the production source set.
- Active tests run from `firmware/tests/host/` and `firmware/tests/static/` and pass with their existing warning settings.
- Legacy and debug files are absent from formal source/test paths but remain recoverable under `validation/legacy/`.
- No `validation/` historical snapshot is deleted.
- `RVMDK/list/` contents are unchanged in this phase.
- README and verification records match firmware `1.0.6`, P5.4 maintenance behavior, and the `data=112.0 xdata=0 code=4725` reference build.
- `git diff --check` passes, and no files under `sdk/V85X_Tina_V1.3/` are modified.
