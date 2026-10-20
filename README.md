# STC8G1K08

## 当前固件：v1.0.7（新电路）

P5.5/AP-RESET 改为持续推挽输出：

| 状态 | P5.5 电平 | 作用 |
|---|---|---|
| 上电初始化后、正常运行、启动宽限 | 低 | 保持通电 |
| 停止监测（STOP） | 低 | 只停止监测，不断电 |
| 触发复位 | 高，持续约 200 ms | 复位/断电 |
| 复位脉冲结束、等待心跳恢复 | 低 | 恢复通电 |

`firmware/User/Config.h` 中 `AP_RESET_ASSERT_LEVEL=1U`。不再使用旧版
高阻释放方式；本版仅适用于用户说明的“低电平通电、高电平复位”电路。

P5.4 心跳协议、30 s 启动宽限、5 s 无心跳超时及内部 WDT 不变。
**冷启动仍默认停止监测**，收到 START 帧后启用；本次没有改变监测启停规则。

## 构建与验证

- Keil 工程：`firmware/RVMDK/STC8G1K08-RESET.uvproj`
- 固件入口：`firmware/User/main.c`
- Rebuild 后烧录：`firmware/RVMDK/list/stc8g1k08_reset.hex`
- STC-ISP 时钟：24 MHz，与 `MAIN_FOSC` 一致。

主机回归（不等同于 Keil 编译或板级验证）：

```sh
sh firmware/tests/static/reset_firmware_static_test.sh
sh firmware/tests/host/run_ap_reset_output_test.sh
sh firmware/tests/host/run_heartbeat_monitor_test.sh
sh firmware/tests/host/run_reset_controller_test.sh
sh firmware/tests/host/run_maintenance_runtime_test.sh
```

新电路修改尚需 Keil Rebuild 和重新烧录，不要直接使用仓库中已有的旧 HEX。
板级验收见 [固件验证记录](firmware/验证记录.md)。固件只能在执行 GPIO
初始化后拉低，STC 复位/下载期间的默认电平仍由芯片和外部电路决定。
