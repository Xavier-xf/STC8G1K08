# STC8G1K08 看门狗固件

独立的 STC8G1K08 外部看门狗固件。当前源码版本为 `1.1.0`，版本定义位于 `User/Config.h`。仅支持一种单线短帧协议：3 字节请求、5 字节应答。

复制整个 `firmware/` 即可取得全部工程源码、寄存器定义、测试及说明，不引用父目录、Linux 源码、Shared 副本或同步脚本。接收方另行准备 Keil C51、适配的 STC 器件环境和 STC-ISP。Linux 端单独交付 `stc_watchdog/`；本产品的 CPU_CHECK socket、CLI、UI 与升级业务不属于固件依赖。

## 目录与职责

```text
firmware/
├── README.md
├── User/
│   ├── main.c
│   └── Config.h
├── Driver/
│   ├── inc/
│   └── src/
├── RVMDK/
│   └── STC8G1K08-RESET.uvproj
├── tests/
│   ├── host/
│   └── static/
└── docs/
    ├── flashing.md
    └── verification.md
```

Keil 只编译五个正式 C 文件：

| 文件 | 职责 |
|---|---|
| `User/main.c` | 板级初始化、中断入口、原子心跳快照、命令执行及复位输出编排 |
| `Driver/src/heartbeat_monitor.c` | 心跳记录与宽限/超时判断，不直接访问寄存器 |
| `Driver/src/reset_controller.c` | 复位状态机，不直接访问寄存器 |
| `Driver/src/maintenance_runtime.c` | 单线收发、CRC、有限帧交接及协议边沿分类 |
| `Driver/src/uart.c` | TX-only 串口诊断 |

`Driver/inc/stc8g1k08_regs.h` 是本工程所需的最小寄存器定义。逻辑模块放在现有 Driver 目录中，不意味着它们直接依赖硬件；以职责和接口区分，不再为小型固件增加目录层次。

## 运行契约

- P5.4：CPU_CHECK 通信与心跳；低电平主动下拉，高电平高阻释放，需要兼容两端电压的外部上拉。Linux 配套模块也使用开漏输出。
- P5.5：独立推挽复位输出，正常/停止/宽限/恢复均为低，复位为高，脉冲约200 ms。它不属于双端开漏通信线。
- 冷启动默认停止外部监测。START 启用并开始30秒宽限；已启用时重复START不重置宽限、心跳或复位状态。
- STOP 停用外部监测并恢复正常复位输出，但仍接收心跳；不停止 STC 内部 WDT。停止后无需续租。
- 仅内部 WDT 复位且没有 POR/LVD 标志时，初始化自动启用外部监测；其他情况默认停止。
- 越过宽限后，无有效心跳达到5秒时断言一次复位；脉冲结束进入等待恢复，不在无心跳时连续重复复位。有效心跳恢复后回到监测。
- `heartbeat_seen` 表示本轮见过有效心跳，不表示此刻心跳一定正常；应结合阶段理解。
- READ 不改变监测状态。查询和 STC 自发应答不能用于刷新保活。固件不会因为收到READ就重新开始宽限。
- 主机初始化心跳不等于启用外部监测，主机须提交START并检查应答；退出或升级前必须确认STOP。无应答不能推断命令未执行。

## 协议

字节序按下面的字段顺序，各字节最高位先发：

| 方向 | 字节0 | 字节1 | 字节2 | 字节3 | 字节4 |
|---|---|---|---|---|---|
| 请求 | 命令 | 非零序号 | CRC | — | — |
| 应答 | 命令 OR 0x80 | 原序号 | 结果 | 状态 | CRC |

START=1、STOP=2、READ=3；执行结果成功0、无效命令1。CRC-8：多项式0x07、初值0、不反射、末尾不异或，覆盖CRC之前所有字节。CRC错误、半帧、零命令、带应答位的请求或零序号不执行。有效未知命令返回无效命令结果。

状态 bit0=监测启用，bit1=见过有效心跳，bit2..4=阶段，bit5..7=0。阶段：停止0、启动宽限1、监测2、复位3、等待恢复4。状态在命令执行后、准备应答时取值，并非收到应答时的连续实时状态。固件版本、心跳年龄和复位次数不占用应答字节，只从串口观察。

- 普通心跳：主机每100 ms翻转，下降沿约每200 ms一次。
- 帧前主机释放至少20 ms；前导的三个下降沿间隔依次400 ms和32 ms。
- 数据0/1由相邻下降沿间隔8/14 ms编码，低脉宽2 ms，接收间隔容差±2 ms。
- 完整请求交给主循环的窗口最多250 ms，超时丢弃；兼容现有9600波特率日志输出，不无限迟发应答。
- 准备好结果后STC等待100 ms，发送同样前导和5字节应答，结束释放。请求和应答复用一个5字节缓冲区，一次处理一条命令，无队列。
- INT2分类输入边沿；Timer0计时、处理接收超时并发送应答；START/STOP执行、CRC计算及应答准备在主循环完成。
- 主循环交接帧和准备结果必须在中断关闭的临界区内。应答GPIO辅助函数只从Timer0调用，避免跨主循环/ISR的C51局部变量overlay共享。
- 为区分请求首沿，普通后一下降沿确认前一心跳时刻，日志中的心跳年龄会包含确认延迟。通信期间不刷新心跳，约1至2秒的年龄增长可见于正常命令事务。
- 配套Linux应答接收上限1700 ms、整笔事务上限2600 ms，有效帧后等100 ms再恢复心跳，并保留400 ms正常心跳窗口。序号只区分有限事务窗口，不是跨重启防重放机制；没有重复事务缓存或兼容模式。

## 编译与烧录

1. 用 Keil 打开 `RVMDK/STC8G1K08-RESET.uvproj`，安装相应的器件支持；工程只引用 `../User`、`../Driver/inc` 和五个目录内源码。标准 STARTUP.A51 和 C51 库由工具链提供。
2. 保持 Memory Model=SMALL，执行 Rebuild，要求零错误、零警告。不要增加 LARGE、XDATA/IDATA迁移或屏蔽overlay警告来绕过容量问题。
3. 输出为 `RVMDK/list/stc8g1k08_reset.hex`、同名M51及构建日志。list目录由Keil生成，不包含在当前干净源码交付中；仓库归档中的旧HEX不能替代本次Rebuild。
4. 用户此前实际验证的容量基线为 `data=112.0 xdata=0 code=4283`。本次只整理测试和文档，生产源码及uvproj校验未变；未在此环境重新运行Keil，不将基线声称为新的构建结果。
5. 检查M51中的DATA/IDATA布局、overlay、调用关系及栈余量，并检查CODE不超过工程IROM 0x0000..0x1FF8。DATA=112不能直接推出栈只剩16字节。

[烧录说明](docs/flashing.md) 包含24 MHz、P54RST及电气要求；[验证记录](docs/verification.md) 区分实际通过、主机模拟和未測项。接收方Rebuild并核对产物后再烧录；仅本次目录/测试/文档整理无需让现有测试板更换已验证HEX。

## 主机回归测试

从复制后的 `firmware/` 根目录执行：

```sh
sh tests/static/reset_firmware_static_test.sh
sh tests/host/run_heartbeat_monitor_test.sh
sh tests/host/run_reset_controller_test.sh
sh tests/host/run_ap_reset_output_test.sh
sh tests/host/run_maintenance_runtime_test.sh
```

依赖POSIX shell、GCC和Python3.9或更新版本，不需要Linux应用或SDK。测试输出在临时目录，不写入生产工程。HOST_TEST宏仅用于模拟C51类型或寄存器，不是旧/新协议开关，不影响Keil生产构建。

模拟能验证心跳、复位逻辑、实际输出函数和协议边界；不能证明Keil内存布局、ISR执行时间、真实电压或波形。不得把静态检查中不存在XDATA声明等同于内存布局已经审计。

## 移植边界与交付

优先复用 `heartbeat_monitor` 和 `reset_controller`；`main.c`、`Config.h`、寄存器头、UART，以及通信模块中的GPIO/中断/时基相关部分按新平台适配。更换MCU不承诺零修改编译。保留32位毫秒判断与16位短帧计时的回绕语义，核对新的编译器类型宽度、中断临界区和调用约束。

模块化不要求新增运行时对象、回调表、动态分配、消息队列或大型平台框架。本交付保持单实例、一个收发复用缓冲区和现有DATA决策路径，不改已验证的固件算法。

交付源码时复制整个本目录，排除后续生成的 `RVMDK/list/` 和个人IDE状态。若交付可烧录产物，应另附同次Rebuild的HEX、M51、工具版本、源码版本和SHA-256；不要包含许可证或个人IDE信息。历史文件不参与生产编译，也不需要随firmware发送。
