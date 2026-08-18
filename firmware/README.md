# STC8G1K08 正式 RESET 固件

本目录的正式交付目标是外部复位固件。SMOKE/MONITOR 工程、入口和静态脚本已移到 `validation/`，仅作为历史验证快照。

## 正式工程

- Keil 工程：`RVMDK/STC8G1K08-RESET.uvproj`
- 生产入口：`User/main.c`
- 逻辑模块：`Driver/src/heartbeat_monitor.c`、`Driver/src/reset_controller.c`
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
0 Error(s), 0 Warning(s)
```

生成的唯一正式烧录文件为 `RVMDK/list/stc8g1k08_reset.hex`。Linux 环境不能替代 Windows Keil 验收。

## ISP 配置

烧录前确认 `isp-config-bringup.txt` 中的现场选项仍有效：

- STC8G1K08-8PIN，P54RST=0。
- IRC/SYSCLK=24 MHz。
- 上电不自动启动硬件 WDT，LVD、复位延时和下载接口按现场记录。
- 串口诊断使用 P3.1、9600 8N1。

## 板上验收

1. 烧录 HEX 并上电，观察 `STC8G1K08 reset firmware v1.0.0` 启动横幅。
2. 正常运行至少 10 分钟，确认 `healthy/monitoring`、`output=high-z` 且无误复位。
3. 找到 `/home/application/KCMLobbyPhone` 的实际 PID，分别执行三次 `kill -STOP <PID>`；每次只允许产生一次复位并恢复心跳。
4. 确认 V853 约 16 s 的启动心跳延迟不会触发误复位，30 s 宽限有效。
5. 确认恢复后 STC 回到 `healthy/monitoring`，P5.5 回到高阻。
6. 仅用 `kill -CONT <PID>` 释放仍被暂停的旧进程，不把它作为复位成功判据。

本次现场记录跳过 P5.5/AP-RESET 示波器测量；`AP_RESET_ASSERT_LEVEL=0` 是当前低有效工程假设，后续若实测极性相反只修改该配置宏并重新烧录。

历史快照说明见 `validation/README.md`，原冒烟文档见 `validation/README-smoke.md`。
