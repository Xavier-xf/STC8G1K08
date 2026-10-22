# STC8G1K08 看门狗固件

## 当前阶段

当前源码版本 1.1.0，仅保留一种短帧协议：3 字节请求、5 字节应答。用户 Keil Rebuild 已确认 `data=112.0 xdata=0 code=4283`，零错误零警告，已生成 HEX。配套 Linux `stc_watchdog/` 已改为相同短帧。**此前9/19字节协议的二进制不兼容；使用这次新生成的HEX和新编译的Linux应用配套上机。**

两份交付代码各自独立：STC 编译只需要 `STC8G1K08/`；Linux 模块只需要 `stc_watchdog/`。无 Shared 副本、同步脚本或跨工程 include。历史资料目录本轮不整理。

## 固件结构

Keil 工程仍为 `RVMDK/STC8G1K08-RESET.uvproj`，仅编译五个文件：

- `User/main.c`：初始化、命令执行、一次原子心跳快照及复位判断。
- `Driver/src/heartbeat_monitor.c`：原心跳与超时逻辑，保持 DATA 时基和快照路径。
- `Driver/src/reset_controller.c`：原复位状态机。
- `Driver/src/maintenance_runtime.c`：单线请求接收、CRC、应答发送。
- `Driver/src/uart.c`：原诊断串口。

保留 SMALL 内存模型，无新增 XDATA/IDATA 声明。通信模块常驻变量按 C51 类型合计 12 字节：5 字节复用缓冲区、3 个单字节状态、2 个 16 位计时变量；这是通信变量的静态大小，不是最终 DATA 总量。函数局部变量、参数和 overlay 仍以新的 M51 为准，不用搬内存区的方式绕开溢出。

## 当前唯一协议

| 方向 | 字节 0 | 字节 1 | 字节 2 | 字节 3 | 字节 4 |
|---|---|---|---|---|---|
| 请求 | 命令 | 非零序号 | CRC | — | — |
| 应答 | 命令 OR 0x80 | 原序号 | 结果 | 状态位 | CRC |

命令：START=1、STOP=2、READ=3。结果：成功=0、无效命令=1。CRC-8：poly=0x07、init=0、无反射、无末尾 XOR，覆盖 CRC 之前的所有字节。

状态位：bit0=监测启用，bit1=见过有效心跳，bit2..4=阶段（停止0、宽限1、监测2、复位3、等待恢复4），bit5..7 保留为0。固件版本、心跳年龄和复位次数继续看串口，不扩充应答帧。

START/STOP 执行后才应答；READ 从现有监测状态取值。重复 START 不重置运行中的宽限，重复 STOP 保持停止，不建立重复事务缓存。零命令、带应答位的请求、零序号、CRC 错误和半帧不执行。

## 时序与职责

- 心跳保持 100 ms 翻转、下降沿约每 200 ms 一次。
- 请求之前主机拉高至少20 ms。前导下降沿间隔依次400 ms、32 ms，然后逐位按8 ms（0）/14 ms（1）编码，每个字节高位先发，低脉宽2 ms，接收容差±2 ms。
- 收到完整请求后最多给主循环250 ms处理时间，兼容现有9600波特率日志；过期丢弃，不能无限等待后再迟发应答。
- 主机在请求结束后释放输出。STC 处理并准备结果后等待100 ms，以开漏方式发送同样前导和位编码的5字节应答，最后释放线路。
- 请求接收与应答复用同一个5字节缓冲区；一次只处理一条命令，不增加队列。
- Timer0 只做计时、接收超时和应答波形。INT2 只处理输入边沿。主循环在 EA 临界区交接帧和准备应答，不在中断中执行 START/STOP。
- 接收初始化没有跨中断调用，CRC 只在主循环调用，GPIO 应答辅助函数只在 Timer0 调用。
- 为区分请求首沿，后一正常下降沿确认前一心跳时刻；查询与 STC 自发应答不能推进保活。原超时和复位算法不变。

## Keil 验证

1. 将更新后的 `User/`、`Driver/` 和 `RVMDK/STC8G1K08-RESET.uvproj` 复制到 Windows。仓库中已移除本次引入的 `Shared/`、`sync_watchdog_protocol.py`、`Driver/inc/stc_watchdog_firmware.h` 和 `Driver/src/stc_watchdog_firmware.c`；Windows 若仍有旧副本，请一并移除。
2. 重新打开 `.uvproj`；确认 Memory Model=SMALL、C51 Define 中无 STC_WDT_PROTOCOL/STC_WDT_DEVICE_ONLY、仅五个正式源码。
3. 用户当前 Rebuild 已确认 data=112.0、xdata=0、code=4283，零错误零警告。后续修改仍需 Rebuild，不能沿用旧容量数值。
4. 若还有 L107 或 L15/L16，附上新 `RVMDK/list/stc8g1k08_reset.m51`，按具体段和调用路径处理，不屏蔽警告、不改 LARGE。
5. 检查 CODE 未超工程 IROM 0x0000..0x1FF8，DATA/IDATA 布局和栈有余量。原 HEX/M51 不是此次生成的结果。

Ubuntu 不安装 Keil。主机行为模拟只能检查软件行为，不能证明 C51 内存布局、ISR 用时或真实电气时序。

## 实板契约

Linux 短帧配套已完成，首轮命令和预期见 Linux 模块 `README.md`。保留原条件：24 MHz、P54RST=0、P3.1 9600 8N1；P5.5 始终推挽低通电、高复位，脉冲200 ms，启动宽限30秒、无保活超时5秒。冷启动默认停止，内部 WDT 复位且无 POR/LVD 标志时启用监测，内部 WDT 不随 STOP 关闭。

硬件按用户要求先假设可用，上机记录实际释放电平、应答、复位与长时间运行情况；假设不等于已验证。首轮仅验证 START/READ/STOP 基础应答，其他字段后续评估；通过后再验证重复 START、STOP 后暂停、无心跳约5秒复位和丢应答。
