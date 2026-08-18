# STC8G1K08 UART Maintenance Mode Design

## Goal

Allow a technician to temporarily suspend the STC heartbeat-timeout-to-AP-RESET path while upgrading or servicing V853. The maintenance state is volatile, expires after a lease, and is controlled manually through the STC DEBUG UART using a PC USB-UART adapter.

## Confirmed Hardware

- STC `P3.0/RXD/INT` and `P3.1/TXD` are exposed as `DEBUG_RX` and `DEBUG_TX` on the board's DEBUG connector.
- The current STC firmware emits its 9600 8N1 diagnostic banner on `DEBUG_TX` but has `SCON = 0x40`, so UART RX is not enabled yet.
- V853 `/dev/ttyS0` is the Linux CPUX console and is not part of this protocol. V853 automation and P5.4 command multiplexing are explicitly out of scope.

## States and Safety Contract

The STC retains its internal WDT in every state. Maintenance only gates the heartbeat timeout decision.

`NORMAL`:

- Record CPU_CHECK falling edges and run the existing heartbeat monitor.
- After the existing 30,000 ms startup grace and 1,000 ms heartbeat timeout, execute the existing 200 ms AP-RESET pulse.

`MAINTENANCE`:

- Keep P5.5/AP-RESET high-impedance; release it immediately when entering maintenance even if the reset controller was asserting it.
- Ignore missing CPU_CHECK edges for AP-RESET purposes.
- Continue the Timer0 tick, UART command handling, status output, and WDT clearing.
- Require an active lease. The default lease is 1,800 s; accepted leases are bounded to 60..3,600 s.

`RESUME` is the transition performed by `MNT EXIT` or lease expiry:

- Clear the maintenance flag in RAM.
- Reinitialize heartbeat and reset-controller startup timestamps without persisting reset state.
- Re-enter the normal 30,000 ms startup grace, then the normal 1,000 ms timeout behavior.

Any STC reset or power cycle clears all maintenance variables because no maintenance state is written to EEPROM or other persistent storage.

## UART Protocol

The protocol is line-oriented ASCII at 9600 8N1. A command is accepted only after a complete line beginning with `MNT `; unknown, malformed, or overlong lines are discarded. The physical DEBUG connector is the trust boundary, so no password is required in this phase.

Commands:

```text
MNT ENTER [seconds]\r\n
MNT RENEW [seconds]\r\n
MNT EXIT\r\n
MNT STATUS\r\n
```

- `ENTER` switches to maintenance and sets the lease. Omitting seconds uses 1,800 s.
- `RENEW` is valid only in maintenance and replaces the lease from the current tick.
- `EXIT` leaves maintenance and starts the normal resume sequence.
- `STATUS` is read-only and reports `mode`, `lease_ms`, heartbeat/reset state, and WDT state.
- Successful commands return an `MNT ACK ...` line. Invalid commands or out-of-range leases return `MNT NACK reason=...`.
- The command parser is bounded and non-blocking. It must never wait for a complete command in the main loop or ISR.

## Implementation Boundaries

- Add a small pure `maintenance_controller` module for line parsing, lease arithmetic, and state transitions. It has no SFR or UART calls and is host-testable.
- Keep SFR initialization, UART transport, Timer0, GPIO, WDT, and the existing heartbeat/reset logic in the current formal project structure. Do not split the old hardware functions as part of this feature.
- Add UART1 RX support in `main.c` with a bounded receive ring and a serial ISR. TX completion must continue to work while RX bytes are buffered; the existing blocking diagnostic output must not clear a TX completion flag before `uart1_putc()` observes it.
- Poll parsed command events from the main loop. Enter/exit side effects occur in main context: update the maintenance controller, release AP-RESET, and reinitialize heartbeat/reset state on resume.
- Keep `INT2` dedicated to CPU_CHECK falling edges. Do not add a P5.4 command encoding or change its electrical role.
- Do not modify V853 application code, device-tree files, `/dev/ttyS0`, RF UART, or radar UART in this phase.

## Manual Maintenance Workflow

1. Connect the PC USB-UART to STC DEBUG_TX/DEBUG_RX/GND and verify the v1.0.0 banner.
2. Send `MNT ENTER 1800` and wait for `MNT ACK mode=maintenance`.
3. Stop `KCMLobbyPhone`; use `kill -9` only after the ACK if graceful termination is unavailable.
4. Perform NFS mounting, replacement, or upgrade work.
5. Start the new application and wait until CPU_CHECK heartbeat output has resumed.
6. Send `MNT EXIT` and verify `MNT ACK mode=resume` followed by `heartbeat=healthy state=monitoring`.

If the technician forgets `EXIT`, the lease expires and normal monitoring resumes. If the STC resets, normal monitoring also resumes immediately after boot.

## Error Handling

- No ACK means maintenance was not entered; the technician must not stop V853 on that attempt.
- A full receive buffer or overlong line is dropped and reported once, without changing state.
- A lease overflow, zero lease, or lease outside 60..3,600 s is rejected.
- UART, parser, or maintenance failures must not disable the internal WDT.
- AP-RESET is forced high-impedance whenever maintenance is active, including entry during an assertion pulse.

## Verification

- Host tests cover command parsing, lease bounds, renewal, exit, expiry, timestamp wraparound, and reset-to-normal initialization.
- Keil build must remain `0 Error(s), 0 Warning(s)` and include only the production RESET target plus the new maintenance logic module.
- Board test with PC USB-UART covers valid/invalid commands, status responses, no AP-RESET during a maintenance lease while V853 is stopped, lease expiry, STC reset during maintenance, and successful resume after V853 heartbeat recovery.
- Existing normal-mode tests remain unchanged: healthy heartbeat, one reset pulse after timeout, WDT enabled, and P5.5 high-z outside the pulse.

## Rollback

If the new firmware fails validation, reflash the previously validated `stc8g1k08_reset.hex`. Since maintenance state is RAM-only and V853 is unchanged, rollback restores the original normal watchdog behavior without data migration.
