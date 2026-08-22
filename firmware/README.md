# STC8G1K08 正式 RESET 固件

当前正式固件版本：`1.0.6`。

本目录的正式交付目标是外部复位固件。生产源码保留在 `User/`、`Driver/`，活跃测试位于 `tests/host/` 和 `tests/static/`；旧实现、调试程序和历史验证材料保留在 `validation/`，不参与正式 Keil 工程。

## 正式工程

- Keil 工程：`RVMDK/STC8G1K08-RESET.uvproj`
- 生产入口：`User/main.c`
- 逻辑模块：`Driver/src/heartbeat_monitor.c`、`Driver/src/reset_controller.c`、`Driver/src/maintenance_runtime.c`
- 串口模块：`Driver/src/uart.c`。P5.4 维护协议由 `maintenance_runtime.c` 正式编译并启用。
- 生成文件：`RVMDK/list/stc8g1k08_reset.hex`

本次保留 HEX 输出名，便于现场烧录记录和脚本继续使用。正式工程目录不再包含 SMOKE/MONITOR 工程。

## 固定契约

- P5.4/INT2 只接收 V853 CPU_CHECK 下降沿；INT2 使用芯片固定下降沿配置。
- P5.5/AP-RESET 正常状态为高阻，只有复位状态驱动 Q23 链路。
- V853 心跳每 100 ms 翻转一次，完整周期约 200 ms。
- RESET 超时为 5000 ms，启动宽限为 30000 ms，复位脉冲为 200 ms。
- STC 内部 WDT 使用 128 分频，24 MHz 下约 2.1 s；主循环持续清 WDT。
- 启动横幅包含固件版本 `STC8G1K08_FIRMWARE_VERSION "1.0.6"`。

## Keil 构建

在 Windows Keil C51 中打开 `RVMDK/STC8G1K08-RESET.uvproj`，确认目标器件为 **STC8G1K08 Series**，不要选择 `STC8G1K08T Series`。

构建前确认 `RVMDK/list/` 可写。验收日志应至少包含：

```text
compiling main.c...
compiling heartbeat_monitor.c...
compiling reset_controller.c...
compiling maintenance_runtime.c...
compiling uart.c...
Program Size: data=112.0 xdata=0 code=4725
0 Error(s), 0 Warning(s)
```

当前正式构建结果为 `data=112.0 xdata=0 code=4725`，未使用 XDATA；心跳、维护协议和复位判断状态均保留在 DATA/局部快照路径。

生成的唯一正式烧录文件为 `RVMDK/list/stc8g1k08_reset.hex`。Linux 环境不能替代 Windows Keil 验收。

## ISP 配置

烧录前确认 `isp-config-bringup.txt` 中的现场选项仍有效：

- STC8G1K08-8PIN，P54RST=0。
- IRC/SYSCLK=24 MHz。
- 上电不自动启动硬件 WDT，LVD、复位延时和下载接口按现场记录。
- 串口诊断使用 P3.1、9600 8N1。

## P5.4 维护协议基线

P5.4/INT2 已启用 ENTER、RENEW、EXIT 维护帧。ENTER 和 RENEW 使用 3 个载荷脉冲，EXIT 使用 4 个载荷脉冲；维护状态只保存在 STC RAM 中。维护期间 P5.5/AP-RESET 保持高阻，EXIT 后重新进入 30 秒启动宽限，再恢复 5 秒无心跳保护。

维护操作顺序：

1. /home/application/cpu_check_ctl enter
2. stop or replace KCMLobbyPhone while STC remains in maintenance mode
3. use /home/application/cpu_check_ctl renew every 10..20 minutes for long maintenance
4. send /home/application/cpu_check_ctl exit before restarting KCMLobbyPhone

## 板上验收

1. 烧录 HEX 并上电，观察 `STC8G1K08 reset firmware v1.0.6` 启动横幅。
2. 保持 V853 正常运行至少 30 分钟，确认每秒日志持续为 `healthy`、`monitoring`、`output=high-z`，且 `resets` 不增加。
3. 执行 `cpu_check_ctl enter` 后停止或替换 KCMLobbyPhone，长时间维护按需执行 `renew`。
4. 维护完成后先执行 `cpu_check_ctl exit`，再启动 KCMLobbyPhone，确认 30 秒宽限和后续 5 秒超时复位。

本次现场记录跳过 P5.5/AP-RESET 示波器测量；`AP_RESET_ASSERT_LEVEL=0` 是当前低有效工程假设，后续若实测极性相反只修改该配置宏并重新烧录。

历史快照说明见 `validation/README.md`，原冒烟文档见 `validation/README-smoke.md`。
