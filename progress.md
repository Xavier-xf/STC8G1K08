# Progress Log

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
