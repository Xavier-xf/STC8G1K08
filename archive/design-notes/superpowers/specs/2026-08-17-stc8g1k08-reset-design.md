# STC8G1K08 External Reset Stage Design

## Goal

Add a separate STC8G1K08 RESET firmware that converts a sustained CPU_CHECK timeout into one controlled P5.5/Q23 reset pulse, while the STC itself is protected by its internal watchdog. Use a 30-second startup grace because the observed V853 heartbeat starts about 16 seconds after power-up.

## Scope

- Keep the validated SMOKE and MONITOR projects unchanged.
- Reuse P5.4/INT2 falling-edge heartbeat capture.
- Keep P5.5 high impedance during startup, healthy monitoring, wait-for-recovery, and STC reset startup.
- Drive P5.5 only during a 200 ms reset assertion pulse.
- Use WDT_CONTR at C1H with EN_WDT=0x20, CLR_WDT=0x10, and WDT_PS=6 (128 divisor, about 2.10 s at 24 MHz).
- Treat reset polarity as a compile-time constant AP_RESET_ASSERT_LEVEL; default is low assertion pending later electrical confirmation.

## State Machine

STARTUP -> MONITORING -> RESET_ASSERT -> WAIT_RECOVERY -> MONITORING

- STARTUP: no output drive; heartbeat grace period is supplied by the heartbeat monitor.
- MONITORING: P5.5 is released high impedance; a timeout enters RESET_ASSERT.
- RESET_ASSERT: P5.5 is driven at the configured assertion level for 200 ms.
- WAIT_RECOVERY: P5.5 is released high impedance and no second pulse is issued until a new heartbeat is observed.

## Safety

GPIO mode changes are ordered as release first, then assertion drive. A watchdog reset returns SFRs to reset values, so P5.5 starts in the safe input-only configuration. UART output is diagnostic only and is never performed inside an ISR.

## Verification

Host tests cover one-shot assertion, pulse completion, wait-for-recovery, repeated recovery cycles, and unsigned millisecond rollover. Board testing covers normal heartbeat, timeout-driven V853 reset, recovery, and STC WDT self-reset. The current skipped P5.5/AP-RESET oscilloscope measurement is recorded as an explicit residual risk.
