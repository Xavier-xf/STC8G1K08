# STC8G1K08 正式 RESET 固件

本目录的正式交付目标是外部复位固件。SMOKE/MONITOR 工程、入口和静态脚本已移到 `validation/`，仅作为历史验证快照。

## 正式工程

- Keil 工程：`RVMDK/STC8G1K08-RESET.uvproj`
- 生产入口：`User/main.c`
- 逻辑模块：`Driver/src/heartbeat_monitor.c`、`Driver/src/reset_controller.c`
- 维护与串口模块：`Driver/src/maintenance_controller.c`、`Driver/src/uart.c`
- 生成文件：`RVMDK/list/stc8g1k08_reset.hex`

本次保留 HEX 输出名，便于现场烧录记录和脚本继续使用。正式工程目录不再包含 SMOKE/MONITOR 工程。

## 固定契约

- P5.4/INT2 只接收 V853 CPU_CHECK 下降沿；INT2 使用芯片固定下降沿配置。
- P5.5/AP-RESET 正常状态为高阻，只有复位状态驱动 Q23 链路。
- V853 心跳每 100 ms 翻转一次，完整周期约 200 ms。
- RESET 超时为 1000 ms，启动宽限为 30000 ms，复位脉冲为 200 ms。
- STC 内部 WDT 使用 128 分频，24 MHz 下约 2.1 s；主循环持续清 WDT。
- 启动横幅包含固件版本 `STC8G1K08_FIRMWARE_VERSION "1.0.0"`。

## Keil 构建

在 Windows Keil C51 中打开 `RVMDK/STC8G1K08-RESET.uvproj`，确认目标器件为 **STC8G1K08 Series**，不要选择 `STC8G1K08T Series`。

构建前确认 `RVMDK/list/` 可写。验收日志应至少包含：

```text
compiling main.c...
compiling heartbeat_monitor.c...
compiling reset_controller.c...
compiling maintenance_controller.c...
compiling uart.c...
0 Error(s), 0 Warning(s)
```

STC8G1K08 内部 DATA RAM 只有 256 字节；UART1 的 64 字节接收环形缓冲使用芯片 XRAM（`xdata`），工程已配置 `XRAM(0-0x03FF)`。

生成的唯一正式烧录文件为 `RVMDK/list/stc8g1k08_reset.hex`。Linux 环境不能替代 Windows Keil 验收。

## ISP 配置

烧录前确认 `isp-config-bringup.txt` 中的现场选项仍有效：

- STC8G1K08-8PIN，P54RST=0。
- IRC/SYSCLK=24 MHz。
- 上电不自动启动硬件 WDT，LVD、复位延时和下载接口按现场记录。
- 串口诊断使用 P3.1、9600 8N1。

## DEBUG UART 维护命令

DEBUG_TX/DEBUG_RX 使用 PC USB-UART 连接，串口参数为 9600 8N1。固件正常和维护运行期间不再每秒输出完整状态；只有启动横幅、命令 ACK/NACK，以及显式状态查询会产生完整输出。

```text
MNT ENTER [seconds]
MNT RENEW [seconds]
MNT EXIT
MNT STATUS
```

`ENTER` 默认租约 1800 秒，允许范围为 60..3600 秒；维护期间 P5.5 保持高阻、忽略 CPU_CHECK 超时，但内部 WDT 继续运行。 `EXIT` 或租约到期后自动恢复正常监控并重新开始 30 秒启动宽限。维护状态只保存在 RAM，STC 复位或掉电后自动回到正常保护。

## 板上验收

1. 烧录 HEX 并上电，观察 `STC8G1K08 reset firmware v1.0.0` 启动横幅。
2. 通过 USB-UART 发送 `MNT ENTER 1800`，确认收到 `MNT ACK mode=maintenance` 后再停止 V853 应用或进行 NFS 升级。
3. 需要完整诊断时发送 `MNT STATUS`；升级完成、心跳恢复后发送 `MNT EXIT`，再用 `MNT STATUS` 确认 `healthy/monitoring`。
4. 正常运行至少 10 分钟，确认 `healthy/monitoring`、`output=high-z` 且无误复位。
5. 找到 `/home/application/KCMLobbyPhone` 的实际 PID，分别执行三次 `kill -STOP <PID>`；每次只允许产生一次复位并恢复心跳。
6. 确认 V853 约 16 s 的启动心跳延迟不会触发误复位，30 s 宽限有效。
7. 确认恢复后 STC 回到 `healthy/monitoring`，P5.5 回到高阻。
8. 仅用 `kill -CONT <PID>` 释放仍被暂停的旧进程，不把它作为复位成功判据。

本次现场记录跳过 P5.5/AP-RESET 示波器测量；`AP_RESET_ASSERT_LEVEL=0` 是当前低有效工程假设，后续若实测极性相反只修改该配置宏并重新烧录。

历史快照说明见 `validation/README.md`，原冒烟文档见 `validation/README-smoke.md`。
