# STC8G1K08 CPU_CHECK Monitor Follow-up

## Goal

Finish the monitor-only STC8G1K08 delivery: document the independent Keil project and board test procedure while preserving the no-reset safety boundary.

## Phases

1. [completed] Confirm the V853 PH15 heartbeat reaches U14 Pin1/P5.4.
2. [completed] Add monitor-only firmware and an independent `STC8G1K08-MONITOR.uvproj` project.
3. [in_progress] Update operator documentation and the board verification record.
4. [pending] Run host/static checks and hand off the Windows compile/burn steps.

## Safety Constraints

- P5.4/INT2 only records CPU_CHECK falling edges.
- P5.5 stays high-impedance; this stage must not drive Q23 or AP-RESET.
- Do not enable or access `WDT_CONTR` in monitor firmware.
- Keep the existing smoke project and local Keil build outputs untouched.

## Errors Encountered

| Error | Resolution |
|---|---|
| Root directory is not a Git repository | STC firmware is tracked by its own `STC8G1K08/.git`; no root worktree action is applicable. |
| `apply_patch` could not run | The sandbox helper failed with `bwrap: loopback: Failed RTM_NEWADDR`; use the standard `patch` command for narrow local edits. |
| Temporary `.orig` and `.rej` patch files were left in `firmware/User` and `firmware/Driver/inc` | Remove only these patch residues; retain the user's Keil `list/` outputs. |
