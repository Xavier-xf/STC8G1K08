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
## UART Maintenance Mode Implementation

1. [completed] Update the maintenance specification for silent operation and explicit `MNT STATUS`.
2. [completed] Add and test the host-only lease/parser controller.
3. [completed] Add the formal UART1 RX/TX module and wire its source into the RESET Keil project.
4. [completed] Integrate `MNT ENTER`, `MNT RENEW`, `MNT EXIT`, and `MNT STATUS` into `main.c`.
5. [completed] Run final static checks and prepare the Windows Keil/board acceptance handoff.

## Current Safety Contract

- Maintenance state is RAM-only and expires after 60..3600 seconds, default 1800 seconds.
- Internal WDT remains enabled and serviced in normal and maintenance modes.
- AP-RESET is forced high impedance during maintenance; resume/expiry reinitializes the 30-second startup grace.
- Normal runtime is silent except for the startup banner and command ACK/NACK; complete state is emitted only by `MNT STATUS`.

## Keil DATA RAM Overflow Fix (2026-08-19)

1. [completed] Trace `L107` segments to the UART RX buffer and existing DATA group.
2. [completed] Add a failing static assertion requiring the UART ring in `xdata`.
3. [completed] Move only the 64-byte UART RX ring to `xdata`; retain the command line in DATA for C51 pointer compatibility.
4. [completed] Run static/host regression checks.
5. [completed] Rebuild `STC8G1K08-RESET` in Windows Keil and confirm no DATA overflow.

## Keil DATA Group Reduction (2026-08-19)

1. [completed] Confirm the remaining failure is `_DATA_GROUP_ LENGTH 0x50` after UART XDATA migration.
2. [completed] Add a static acceptance rule for a 16-byte command line, sufficient for all 14-byte protocol commands.
3. [completed] Reduce the command line from 64 to 16 bytes and rerun host/static tests.
4. [completed] Rebuild in Windows Keil and inspect the remaining DATA allocation.

## Keil MAIN Local Workspace Reduction (2026-08-19)

1. [completed] Confirm the new remaining linker failure is `?DT?MAIN LENGTH 0x36` with global DATA reduced to 147 B and UART RX storage at 64 B XDATA.
2. [completed] Replace full `heartbeat_monitor_t` snapshots with short atomic scalar snapshots for state, edge count, and edge age.
3. [completed] Run host/static regression checks.
4. [completed] Rebuild `STC8G1K08-RESET` in Windows Keil and confirm no DATA overflow.
