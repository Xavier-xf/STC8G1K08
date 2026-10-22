# STC8G1K08 UART Maintenance Mode Implementation Plan

> Execute this plan in order. Keep the validated normal reset behavior intact;
> hardware and board acceptance remain separate from host/static verification.

## Goal

Add a volatile, lease-based maintenance mode controlled through the STC DEBUG
UART. While active, AP-RESET stays high impedance and missing CPU_CHECK edges do
not trigger a reset. The internal WDT remains enabled in every mode. Remove the
old once-per-second status output and make `MNT STATUS` the explicit full-state
query.

## Scope

- Add host-testable `maintenance_controller` parsing and state transitions.
- Add a focused `uart.c/.h` module for UART1 TX/RX and a bounded RX ring.
- Keep Timer0, GPIO, WDT, heartbeat, and reset behavior in `main.c`.
- Update the formal Keil project with the two new production sources.
- Do not change V853 application code, P5.4 signaling, or the Tina SDK.

## Tasks

### 1. Pure controller test first

- Add `maintenance_controller_test.c` and its runner.
- Cover command parsing, default/min/max leases, invalid values, renew/exit,
  expiry, millisecond wraparound, and reset-to-normal initialization.
- Run the runner before adding the implementation and record the expected
  compile failure.

### 2. Implement pure maintenance controller

- Add `Driver/inc/maintenance_controller.h` and
  `Driver/src/maintenance_controller.c`.
- Keep it free of SFR/UART calls and compatible with host GCC and Keil C51.
- Represent maintenance as RAM-only state with a start timestamp and bounded
  lease duration.

### 3. Add UART module

- Add `Driver/inc/uart.h` and `Driver/src/uart.c`.
- Configure UART1 for 9600 8N1, enable RX interrupt, handle RI/TI in one ISR,
  and expose non-blocking byte reads plus blocking TX helpers.
- Keep the RX ring bounded and report overflow to the main loop once.

### 4. Integrate commands and silent operation in `main.c`

- Remove the one-second `app_report` timer.
- Parse complete lines in main context and emit one ACK/NACK per command.
- Enter/renew/exit maintenance with the lease controller; force AP-RESET
  high-Z on entry and reinitialize heartbeat/reset state on resume or expiry.
- Emit a complete `MNT STATUS` snapshot only when requested; in maintenance
  report `heartbeat=paused state=maintenance`.
- Keep startup banner, WDT service, INT2 heartbeat capture, and normal reset
  state transitions intact.

### 5. Project and verification

- Add both new source files to `STC8G1K08-RESET.uvproj`.
- Extend static checks for UART RX, maintenance commands, no periodic report,
  and formal project membership.
- Run host tests, shell syntax checks, static checks, and inspect the diff.
- Leave Windows Keil zero-warning build and board maintenance workflow for
  现场 acceptance; do not claim them locally.

## Acceptance

- Host maintenance test passes with `-Wall -Wextra -Werror`.
- Existing heartbeat/reset host tests still pass.
- RESET static check passes and finds `uart.c` and `maintenance_controller.c`
  in the formal Keil project.
- No `app_report` call remains in the periodic main-loop path.
