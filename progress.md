# Progress Log

## v1.0.4 Keil Build Successful (2026-08-20)

Windows Keil 编译成功！

**编译结果：**
```
Program Size: data=122.0 xdata=0 code=2879
0 Error(s), 2 Warning(s)
Generated: stc8g1k08_reset.hex
```

**内存使用分析：**
- DATA: 122 字节 / 256 字节 (47.7%)
- XDATA: 0 字节（完全符合 v1.0.3 设计原则）
- CODE: 2879 字节

**Warnings（无害）：**
- L16: `maintenance_runtime_is_active` 未调用（保留用于未来扩展）
- L15: `mrt_elapsed` 多次调用（正常的共享辅助函数）

**配置调整：**
- 关闭 UART 日志输出（`STC8G1K08_UART_LOG_ENABLE=0U`）
- `maintenance_runtime_remaining_ms` 仅在 HOST_TEST 时编译
- 核心功能完全保留

**下一步：板上验证（Phase 4）**

## Maintenance Runtime Integration Completed (2026-08-20)

Phase 1-3 已完成，固件已集成 `maintenance_runtime` 模块：

**集成内容：**
- 添加 `g_maintenance_runtime` 全局变量（9 字节 DATA）
- 更新 INT2 ISR：先调用 `heartbeat_monitor_on_falling_edge()`，再调用 `maintenance_runtime_on_falling_edge()`
- 主循环轮询维护事件：ENTER/EXIT/EXPIRED
- 维护模式下 AP_RESET 保持高阻，跳过复位逻辑
- EXIT/EXPIRED 事件重新初始化复位控制器，恢复启动宽限期
- 更新 Keil 项目，添加 `maintenance_runtime.c`
- 版本号更新为 v1.0.4

**测试结果：**
- ✅ `reset_firmware_static_test.sh` 通过
- ✅ `run_maintenance_runtime_test.sh` 通过（8/8 测试）
- ✅ `run_heartbeat_monitor_test.sh` 通过
- ✅ `run_reset_controller_test.sh` 通过

**下一步（Phase 4）：**
- Windows Keil 重新编译
- 检查 DATA 使用量（预期 <100 字节）
- 板上验证：P5.4 ENTER/EXIT 命令、租约到期、掉电恢复

## Maintenance Runtime Module Implementation (2026-08-20)

- Created `maintenance_runtime` module to combine P5.4 decoder and maintenance 
  controller into a single compact implementation.
- DATA footprint: 9 bytes total (4B last_edge_ms + 4B lease_end_ms + 1B state)
- State byte packs: decoder_state (2 bits), have_edge (1 bit), payload_count 
  (3 bits), maintenance_active (1 bit), reserved (1 bit)
- Supports ENTER (3 payload pulses), EXIT (4 payload pulses), and automatic 
  lease expiry (default 30 minutes)
- Protocol timing identical to original `cpu_check_command_decoder`: BREAK 
  ≥750ms, SYNC 60-110ms, DELIMITER 240-360ms, PAYLOAD 60-110ms, TRAILER ≥500ms
- All 8 host tests pass: init, enter_command, exit_command, lease_expiry, 
  normal_heartbeat_ignored, break_interrupted, renew_extends_lease, time_overflow
- Next: integrate into main.c following v1.0.3 design (heartbeat first, then 
  decoder; no XDATA for reset path)

## Root Cause Closed; P5.4 Reintegration Required (2026-08-20)

- The P5.4 integration exceeded DATA, so a linker workaround moved reset-related
  time/status scratch into XDATA and used split snapshots.
- The resulting v1.0.2 firmware produced a false timeout: AP_RESET asserted
  roughly 407 ms after `healthy age_ms=185`, versus a 5000 ms timeout setting.
- v1.0.3 restored the DATA timer and atomic heartbeat snapshot. Board evidence
  now includes one hour without false reset plus a successful intentional
  5-second timeout, 200 ms AP_RESET pulse, and automatic V853 recovery.
- New requirement: add P5.4 ENTER/RENEW/EXIT without using XDATA or split
  snapshots in the heartbeat/reset/AP_RESET decision path; protocol observation
  must occur after the direct heartbeat update on every INT2 edge.

## v1.0.3 A/B: Restore d092781 Snapshot Path (2026-08-20)

- The static RED contract first failed on the old `1.0.2` version and, after
  the version-only edit, failed on the missing single heartbeat snapshot.
- Restored `g_millisecond` to DATA and one atomic `heartbeat_snapshot()` plus
  local `app_run()` state; reset timing, UART logs, WDT, P5.4 direct edge
  handling, V853, and Keil project membership were not changed.
- Static, heartbeat, reset-controller, maintenance-controller, and excluded


## v1.0.3 Field A/B Result (2026-08-20)

- Keil rebuilt successfully: `data=93 xdata=0 code=3173`, zero warnings and
  errors. The board booted with the expected v1.0.3 banner.
- A 30-minute-plus observation contains no AP-RESET assertion. The sampled
  86.829-second interval measures 434 CPU_CHECK falling edges (4.998 Hz),
  with continuous healthy monitoring and `resets=0`.
- The regression is bounded to the former XDATA global-scratch decision path,
## v1.0.2 诊断基线恢复（2026-08-20）

- 已撤销未完成的决策快照日志，`apply_reset_output(active)` 和 `app_run()` 恢复为 v1.0.2 的原有状态机调用顺序。
- 静态检查、心跳、复位、CPU_CHECK 解码器和维护控制器主机测试均已通过；Windows Keil 重新构建与板上观测仍由现场完成。

## 2026-08-17

- Resumed the monitor-only STC8G1K08 phase after PH15-to-P5.4 hardware propagation was measured.
- Verified the independent monitor project and source contract are present.
## 2026-08-19

- Implemented the pure `maintenance_controller` with bounded command parsing,
  lease validation, renewal, expiry, timestamp rollover handling, and volatile
  reset-to-normal initialization.
- Added `firmware/Driver/inc/uart.h` and `firmware/Driver/src/uart.c` with UART1
  9600 8N1 RX interrupt handling, a 64-byte ring buffer, TX completion flag,
  and blocking output helpers.
- Integrated maintenance commands and explicit `MNT STATUS` output into the
  formal RESET `main.c`; removed the old once-per-second status report.
- Added the two production sources to `STC8G1K08-RESET.uvproj` and extended the
  RESET static check.
- Host heartbeat, reset-controller, and maintenance-controller tests pass;
  Windows Keil build is accepted; board validation remains pending.

## Keil DATA RAM Overflow Fix (2026-08-19)

- 现场 Keil C51 构建报告 `L107 ADDRESS SPACE OVERFLOW`：`_DATA_GROUP_` 为 `0x50`，`?DT?UART` 为 `0x44`，合计使 DATA 到 259 B。
- 根因是新 UART 的 64 B RX 环形缓冲及其 4 B 控制变量进入 STC8G1K08 的 256 B 内部 DATA RAM。
- 修复：仅将 `g_uart_rx_buffer[64]` 显式声明为 `xdata`；命令行仍保留 DATA，避免把 XDATA 指针传入现有 DATA `const char *` 命令解析接口。
- 预计释放 64 B DATA（加上缓冲段控制布局的余量），使用 Keil 工程已配置的 `XRAM(0x0000..0x03FF)`。
- 本地静态/逻辑验收通过；需在 Windows Keil 重新 Rebuild 确认 `0 Error(s), 0 Warning(s)`，随后再做烧录和现场命令回归。

## Keil DATA Group Reduction (2026-08-19)

- UART RX 环形缓冲迁移后，Keil 输出变为 `data=195 xdata=64`，`?DT?UART` 溢出已消失，但 `_DATA_GROUP_ LENGTH 0x50` 仍无法放置。
- 最长合法维护命令为 `MNT ENTER 3600` 或 `MNT RENEW 3600`，长度均为 14 B；包含 NUL 只需 15 B。
- 将 `COMMAND_LINE_BUFFER_SIZE` 从 64 收敛为 16，保持全部合法命令不变，超长输入仍走已有 `line-too-long` NACK。
- 预期 `_DATA_GROUP_` 由 0x50 降到约 0x20；已通过静态检查和三个主机逻辑测试，等待 Windows Keil Rebuild 结论。

## Keil MAIN Local Workspace Reduction (2026-08-19)

- 现场第三次 Rebuild 结果：全局 DATA 降至 147 B、XDATA 为 64 B，`_DATA_GROUP_` 与 `?DT?UART` 已不再报错；唯一剩余错误是 `?DT?MAIN LENGTH 0x36`。
- 后续只处理 `main.c` 的自动局部变量工作区，重点是命令处理和主循环中的 `heartbeat_monitor_t` 快照，协议、维护状态机和硬件时序保持不变。

- 根因确认：`app_run()` 和其调用的命令路径同时持有 13 B `heartbeat_monitor_t` 快照；Small memory model 下这些自动对象处于同一调用链，不能覆盖，令 `?DT?MAIN` 达到 0x36。
- 修复：删除完整结构体快照，改为各自短暂关闭中断的状态、边沿数和边沿年龄标量快照；中断会在所有 UART TX 前恢复，接收 ISR 与 TX 完成 ISR 保持可用。
- 验收通过：`reset_firmware_static_test.sh`、`run_heartbeat_monitor_test.sh`、`run_reset_controller_test.sh`、`run_maintenance_controller_test.sh` 和 `git diff --check`。
- `run_maintenance_controller_test.sh` 首次因执行沙箱无法建立临时资源失败；在允许的本地环境以相同脚本重跑通过。
- Windows Keil Rebuild 仍是唯一待验证项。

## Keil Build Accepted (2026-08-19)

- Windows Keil `Rebuild target 'STC8G1K08-RESET'` 已通过。
- 结果：`Program Size: data=121.0 xdata=64 code=5651`，生成 `stc8g1k08_reset.hex`，`0 Error(s), 0 Warning(s)`。

## P5.4 Single-Wire Maintenance Design (2026-08-19)

- Began a design-only replacement assessment after the UART maintenance build
  showed intermittent field AP-RESET events. No production source has been
  changed in this phase.
- Scope: validate the PH15/P5.4 electrical path, then define or reject a
  guarded unidirectional pulse frame. The user explicitly requested no
  multi-agent delegation for this assessment.

## P5.4 Design Findings Completed (2026-08-19)

- Verified PH15 -> R78 (1 kOhm) -> U14 P5.4 in the V853 schematic. P5.5 is
  separate AP-RESET circuitry, so P5.4 remains V853-to-STC only.
- Chosen direction for review: guarded digital pulse frames inspired by a
  LIN-style break/sync boundary, with a local V853 control endpoint for ENTER
  before the application releases PH15. A standalone direct tool is reserved
  for RENEW/EXIT after the application has stopped.
- Rejected simple three/four-edge counting, analog waveform ideas, and a
  post-kill-only enter workflow because each permits false commands or a
  GPIO-ownership/timeout race.

## P5.4 Implementation and Verification (2026-08-19)

- STC UART RX/log path is compile-time disabled (`STC8G1K08_UART_LOG_ENABLE=0U`); the previous DATA-RAM-heavy receive buffer is no longer part of the formal build.
- `cpu_check_command_decoder` distinguishes normal 200 ms heartbeat edges from a framed P5.4 command. Three short payload pulses enter/renew maintenance; four resume normal monitoring.
- `HEARTBEAT_MONITOR_TIMEOUT_MS` is 5000 ms. The 30000 ms startup grace, 200 ms reset pulse, WDT servicing, and RAM-only maintenance state remain unchanged.
- Host decoder, maintenance-controller, heartbeat, reset-controller, static checks, strict V853 compiles, and the full `KCMLobbyPhone` cross build passed. Windows Keil/board validation is still pending.

## Intermittent AP-RESET Investigation (2026-08-19)

- Removed the generated `firmware/User/main.c.orig` residue.
- Re-ran `reset_firmware_static_test.sh`, `run_cpu_check_command_decoder_test.sh`,
  and `git diff --check`; all returned success.
- Re-enabled formal RESET diagnostics in source (`STC8G1K08_UART_LOG_ENABLE=1U`)
  and restored the 1 s report plus AP_RESET transition lines. UART RX remains
  compiled out.
- Compared current `main.c` with known-good `d092781`: current code filters
  heartbeat edges to `150..250 ms`, while `d092781` accepted every INT2 edge.
  This timing filter is the leading hypothesis for the intermittent reset and
  needs board evidence (`edges` stopping and `age_ms` approaching 5000) before
  a targeted change.
- Next operator action: rebuild `STC8G1K08-RESET` in Keil, flash
  `firmware/RVMDK/list/stc8g1k08_reset.hex`, and capture the final status lines
  before an AP_RESET event.

## 2026-08-20 P5.4 v1.0.5诊断

- 已确认当前现场波形的下降沿间隔与协议匹配；修复 DATA/XDATA 协议状态访问、主循环临界区、租约回绕和恢复宽限。
- 新增现场 ENTER 波形回归测试，并将短脉冲上限设为 150 ms。
- 版本为 1.0.5，日志打开；必须确认 Keil map 包含 maintenance_runtime.obj 后再烧录。
