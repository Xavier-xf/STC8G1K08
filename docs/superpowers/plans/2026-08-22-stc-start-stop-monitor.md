# STC Start-Stop Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make STC monitoring disabled by default except after an STC watchdog reset, and control it through V853 START/STOP frames without changing the wire timing.

**Architecture:** Use reset-cause flags captured before watchdog setup to choose the initial monitor state. Keep the existing compact decoder and state byte, remove the lease timestamp and expiry branch, and let START/STOP transition a persistent monitor-enabled bit. The main loop always services the STC WDT, while reset decisions run only when monitoring is enabled.

**Tech Stack:** Keil C51, STC8G1K08 SFRs, existing host-test compatibility mode.

---

### Task 1: Reset cause and monitor initialization

**Files:** `firmware/Driver/inc/stc8g1k08_regs.h`, `firmware/User/main.c`, `firmware/Driver/inc/maintenance_runtime.h`

- [ ] Define PCON and WDT flag accessors, capture reset cause before `wdt_init()`, and initialize monitor state accordingly.

### Task 2: Persistent START/STOP decoder

**Files:** `firmware/Driver/inc/maintenance_runtime.h`, `firmware/Driver/src/maintenance_runtime.c`, `firmware/tests/host/maintenance_runtime_test.c`

- [ ] Remove lease fields/events and implement idempotent 3-pulse START and 4-pulse STOP behavior, including decoding while monitoring is disabled.

### Task 3: Main-loop gating and verification

**Files:** `firmware/User/main.c`, `firmware/tests/static/reset_firmware_static_test.sh`

- [ ] Gate AP_RESET decisions by monitor state, keep WDT clearing unconditional, and verify DATA/XDATA output after Keil build.
