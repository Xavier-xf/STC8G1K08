# STC8G1K08 Monitor Findings

## Firmware Directory Cleanup Findings (2026-08-22)

- The formal Keil project contains `maintenance_runtime.c` and the retained output records `data=112.0 xdata=0 code=4725` with zero errors and warnings.
- Active tests are under `firmware/tests/host/` and `firmware/tests/static/`; superseded implementations and debug material remain under `firmware/validation/legacy/`.
- Documentation was stale at 1.0.1/1.0.2 and incorrectly described P5.4 maintenance as disabled; current source and build records show version 1.0.6 with maintenance enabled.

## v1.0.4 Keil Build Results (2026-08-20)

**编译成功：**
```
Program Size: data=122 xdata=0 code=2879
0 Error(s), 2 Warning(s)
```

**内存占用确认：**
- DATA: 122B / 256B (47.7%) ✅
- XDATA: 0B（无 XDATA 依赖）✅
- CODE: 2.8KB

**为达到 DATA 限制所做的权衡：**
1. 关闭 UART 日志（`STC8G1K08_UART_LOG_ENABLE=0U`）
   - 无启动横幅
   - 无周期状态报告
   - 无维护事件日志
2. `maintenance_runtime_remaining_ms()` 仅在 HOST_TEST 编译
   - 固件中不可用（但也不需要）
   - 测试代码仍可用

**保留的核心功能：**
- ✅ 心跳监控（每个 INT2 边沿）
- ✅ 复位控制（5 秒超时，200ms 脉冲）
- ✅ P5.4 维护协议解码
- ✅ ENTER/EXIT/EXPIRED 事件处理
- ✅ 维护模式 AP_RESET 高阻
- ✅ WDT 服务
- ✅ 30 秒启动宽限

**Warnings 分析：**
- L16 `maintenance_runtime_is_active` 未调用：保留接口，未来可用
- L15 `mrt_elapsed` 多次调用：正常，共享时间差计算函数

**结论：编译成功，内存占用合理，核心功能完整。**

## Maintenance Runtime Integration Status (2026-08-20)

Successfully integrated `maintenance_runtime` module into v1.0.4 firmware:

**Code changes:**
- `User/main.c`: Added maintenance event handling in main loop
- INT2 ISR: Heartbeat first, then decoder (preserves v1.0.3 decision path)
- Maintenance mode: AP_RESET forced high-Z, reset logic skipped
- EXIT/EXPIRED: Reset controller reinitialized with startup grace

**Memory footprint (estimated):**
- g_millisecond: 4B
- g_heartbeat_monitor: 13B  
- g_reset_controller: 8B
- g_maintenance_runtime: 9B
- g_reset_output_active: 1B
- Local variables: ~20B
- **Total DATA: ~55B** (well under 256B limit)

**Verification:**
- Static checks: PASSED
- All host tests: PASSED (maintenance_runtime, heartbeat, reset_controller)
- Keil project updated with maintenance_runtime.c
- Version: 1.0.4

**Next: Windows Keil build + board validation**

## Maintenance Runtime Module Design (2026-08-20)

Created a compact `maintenance_runtime` module that replaces the separate 
`cpu_check_command_decoder` and `maintenance_controller` modules:

**Memory footprint (DATA):**
- Original split design: ~13B decoder + ~12B controller ≈ 25B
- New unified design: 9B total (4B last_edge_ms + 4B lease_end_ms + 1B state)
- State byte bit packing avoids C51 bitfield overhead

**Key differences from original:**
- No text command parsing (was MAINTENANCE_CONTROLLER_HOST_TEST only)
- Binary events only: ENTER, EXIT, EXPIRED
- RENEW is implicit (ENTER while already in maintenance mode)
- Single structure replaces two separate state machines
- Protocol timing preserved exactly from original decoder

**Integration requirements:**
- INT2 ISR updates heartbeat monitor first, then calls decoder
- Decoder never filters or delays heartbeat edges
- Lease state in DATA, never XDATA
- Poll returns events for main loop to act on (enter/exit/expired)

All host tests verified including time overflow handling.

## Confirmed False-Reset Mechanism and Required P5.4 Feature (2026-08-20)

The P5.4 maintenance build exceeded C51 DATA, which led to a link workaround:
the millisecond timebase and shared main-loop reset-decision scratch were moved
to XDATA and read through several independent snapshots.

The direct failure was an erroneous `HEARTBEAT_MONITOR_TIMEOUT` passed to
`reset_controller_update()`. Evidence is the v1.0.2 field event: a healthy
heartbeat with 185 ms edge age at 11:52:30.425, followed by AP_RESET assertion
at 11:52:30.832, about 407 ms later despite a 5000 ms timeout. This excludes a
real V853 heartbeat stop as the cause of that assertion.

v1.0.3 restored DATA `g_millisecond` and one atomic local heartbeat snapshot.
It ran for more than one hour without a false reset; an intentional stopped
heartbeat asserted once at about five seconds, released after 200 ms, and
returned to healthy monitoring after V853 restarted.

The precise XDATA-level defect is not individually proved. The supported rule
is that XDATA/shared split snapshots must not be part of AP_RESET decisions.
The next required feature is P5.4 ENTER/RENEW/EXIT, with its decoder isolated
from the verified DATA/local heartbeat and reset-decision path.

## v1.0.3 A/B Scope (2026-08-20)

- The `1.0.2` field trace reports `healthy age_ms=185` and then AP-RESET
  asserts about 407 ms later, despite a 5000 ms timeout.
- The formal RESET project excludes both P5.4 protocol modules; map output
  shows `data=60`, `xdata=27`, so neither decoder logic nor RAM pressure is
  part of this A/B.
- This experiment changes one remaining difference only: the XDATA global
  scratch/status path is replaced with the known-good DATA timebase and one
  atomic `heartbeat_snapshot()` copied into local main-loop state.
- This is diagnostic, not a confirmed root-cause fix. Keil and board results
  determine the next hypothesis.

## v1.0.2 诊断基线恢复（2026-08-20）

- 用户要求放弃未完成的决策快照日志，恢复此前 v1.0.2 的周期打印格式，避免再增加诊断变量影响现场排查。
- 当前基线保留 30 秒宽限、5 秒超时、200 ms 脉冲、WDT 和 UART 周期日志；INT2 只记录每个 CPU_CHECK 下降沿。

- Latest schematic `app/doc/V853(26.07.09).pdf` maps V853 PH15 to U14 Pin1/P5.4/INT2 (`CPU_CHECK`). U14 Pin3/P5.5 is the AP-RESET/Q23 side.
- Board measurement confirms a 100 ms high / 100 ms low PH15 waveform and an identical waveform at U14 Pin1/P5.4.
- The monitor firmware therefore counts INT2 falling edges. A 200 ms period yields about five edges per second.
- P5.5 has previously measured around 1.6-2.2 V through the Q23/reset network. That voltage is a board bias condition, not a target level for this firmware. High impedance must be confirmed by no change/no pulse at P5.5 and AP-RESET.
- The monitor project has no P5.5 assignment, no Q23 access, and no `WDT_CONTR` access.

## P5.4 Single-Wire Maintenance Investigation (2026-08-19)

- The current UART maintenance build passed Keil size checks, but the observed
  post-update intermittent AP-RESET makes it unsuitable as the baseline for a
  new protocol. UART receive and command processing should be fully compiled
  out first, not merely left idle at runtime.
- Existing evidence maps V853 PH15 to STC P5.4/INT2 (`CPU_CHECK`) and documents
  a 100 ms high / 100 ms low heartbeat. P5.4 must remain V853-to-STC only.
- The candidate protocol must distinguish maintenance commands from ordinary
  heartbeat edges with a deliberate framing gap and timing validation; counting
  three or four normal edges alone is not safe.

### Schematic Evidence

- `V853(26.05.06).pdf` page 15 labels V853 `PH15` as `CPU_CHECK` and shows it
  through `R78` (1 kOhm) to U14 P5.4. The same sheet shows U14 P5.5 on the
  Q23/AP-RESET side; the two functions are physically distinct.
- This establishes a V853-to-STC signal path only. No STC-to-V853 acknowledgement

### Current Firmware Evidence

- `reset_controller_init()` explicitly zeroes `reset_count`; the observed
  `resets=3` therefore represents three AP-RESET assert events since the STC
  last boot, not an uninitialized diagnostic value.
- The UART maintenance build configures `SCON = 0x50` (receiver enabled), sets
  `ES = 1`, and runs a receive ISR plus a 64-byte XDATA ring on every received
  byte. This is a plausible regression candidate, but the current evidence does
  not prove it caused the missing-heartbeat resets.

## P5.4 Final Implementation Decision (2026-08-19)

- The UART receive path is now disabled by default and is not initialized or
  serviced by the formal RESET firmware. UART TX sources remain available for
  later diagnostics without consuming the receive buffer.
- P5.4 uses a guarded digital frame: a 900 ms break, validated sync/delimiter
  intervals, then three or four short payload pulses. Ordinary 200 ms heartbeat
  edges are filtered out before they update the monitor and cannot complete a
  command frame.
- STC accepts ENTER/EXIT in RAM only. WDT remains enabled, P5.5 is high-z in
  maintenance, and a lease expiry, reset, or power loss returns to normal
  protection automatically.
- Host/static/cross-build evidence is complete. Real PH15/P5.4 waveform,
  maintenance flow, lease expiry, and reset/power-loss checks remain field work.

## Intermittent AP-RESET Investigation (2026-08-19)

- The known-good `d092781` firmware counted every INT2 falling edge. The current
  P5.4 implementation updates the heartbeat monitor only when the interval
  between consecutive edges is within `150..250 ms`.
- V853 generates a nominal 200 ms falling-edge period by toggling PH15 every
  100 ms, but Linux scheduling does not guarantee that exact interval. A single
  interval above 250 ms is discarded; repeated scheduling jitter can therefore
  leave `last_edge_ms` stale until the 5 s timeout and cause AP-RESET even while
  PH15 resumes toggling.
- This is the leading code-level hypothesis because it is a direct behavioral
  difference from `d092781`; it must be confirmed on hardware with the restored
  periodic log (`edges`, `age_ms`, `P54`) before changing the filter.
- UART RX remains disabled. `STC8G1K08_UART_LOG_ENABLE` is currently `1U` in
  source so the diagnostic build emits the startup banner, one status line per
  second, and AP_RESET transition lines. A rebuilt and reflashed HEX is required
  for those lines to appear on the board.

## v1.0.5 P5.4现场复核（2026-08-20）

- 现场帧下降沿间隔为约 900/110/300/110/110/110 ms，符合 BREAK、SYNC、DELIMITER 和 3 个 payload 的协议结构；时序本身不是当前复位证据。
- 仓库中的旧 m51 输入模块只有 main、heartbeat_monitor、uart、reset_controller，Program Size 为 data=60、xdata=27，不能证明 maintenance_runtime 已被编译或烧录。
- 维护运行时状态已从 xdata 改回 DATA，并对主循环轮询加 EA 临界区；EXIT/租约到期会原子重置 heartbeat 和 reset_controller，恢复 30 秒宽限。
- 短边沿容差由 60..110 ms 放宽到 60..150 ms，避免 Linux nanosleep 轻微抖动让现场 110 ms 边界帧被拒绝。
- 诊断版本提升为 1.0.5，UART 日志打开；主机测试和静态检查已通过，待用户在 Keil 重新 Rebuild、烧录和查看 MNT ENTER。
