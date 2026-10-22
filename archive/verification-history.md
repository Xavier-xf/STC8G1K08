# STC8G1K08 正式 RESET 固件验证记录

## v1.1.0 短帧实现阶段（2026-10-21）

按确认的收缩方案，STC 与 Linux 各自独立实现同一短帧协议，不共用源码。STC 3 字节请求、5 字节应答已获用户 Keil 验证；Linux 独立模块已接入同样短帧并交叉编译，基础应答仍待实板验证。未打包、未烧录；旧9/19字节协议二进制不属于这次交付。

| 项目 | 本阶段结果 |
|---|---|
| STC 单独交付 | 工程仅包含 main、heartbeat_monitor、reset_controller、maintenance_runtime、uart 五个正式源码 |
| 清除跨工程依赖 | 已移除本次 Shared、副本同步脚本和 stc_watchdog_firmware；工程不引用 Linux 文件 |
| 当前协议 | 单一短帧，无旧/新协议选择分支；通信常驻状态按 C51 类型计算为 12 字节 |
| 内存策略 | 保留 SMALL、原 DATA 时基及一次原子心跳快照；无新增 XDATA/IDATA 声明 |
| 短帧行为验证 | 通过：3 字节请求、5 字节应答、CRC、拒绝响应、16 位回绕、250 ms 有限交接窗口、半帧恢复、查询和自身应答不喂狗 |
| 原心跳逻辑回归 | 通过 |
| 原复位状态机回归 | 通过 |
| 实际 P5.5 输出函数寄存器模拟 | 通过：低通电/高复位、200 ms 脉冲、推挽保持 |
| Keil Rebuild / DATA / CODE | 用户日志确认：data=112.0 xdata=0 code=4283，生成HEX，0 Error(s), 0 Warning(s)；栈余量仍需结合新M51核对 |
| Linux 短帧模块 | 已完成独立的接口/线程、GPIO、协议三组文件；应用与CLI交叉编译通过，协议及线程模拟应答验证通过 |
| 首轮实板命令（2026-10-21） | Linux START/READ/STOP均报TIMEOUT；STC出现stopped/startup切换，普通心跳恢复后约每秒增加5计数。请求至少部分执行，返回应答仍未确认，两端日志不按不同钟表直接对时 |
| Linux 超时诊断第二轮 | 用户日志：启动START(seq=138)、READ(seq=139)、START(seq=140)均tx_edges=27、tx_complete=1、tx_late=0，rx_edges/rx_bits/dropped/bad_frames/clock_changed/io_errno全为0。本端完整发送，无用户态回传事件；不是已有帧被CRC或时钟过滤。SDK只读核对PH15支持EINT、事件使用墙上时钟；下一步测P5.4与PH15应答窗口的实际电平/边沿，不改协议或已通过STC复位逻辑 |
| 电气、ISR 时长、真实应答及复位、长时间运行 | 仍待确认；先排查返回通路，不重复已经有证据的普通心跳链路 |

此前通用 9/19 字节方案被取消，其 Keil 失败日志为 data=205.0、xdata=94、code=9047，包含 L107、L15/L16。它不代表当前短帧实现的容量；本阶段不用 LARGE 或搬到 XDATA 规避问题。

详细编译步骤见 README.md。历史章节只说明当时的结果，不适用于当前短帧协议。

## v1.0.7 新电路验收（2026-10-07）

本节为当前版本要求；下方 v1.0.6 记录仅供历史参考，不适用于新电路。

- 工程仍为 `RVMDK/STC8G1K08-RESET.uvproj`，入口 `User/main.c`。
- 新电路 P5.5 **低电平通电、高电平复位**，始终推挽输出，不再高阻释放。
- GPIO 初始化先写低电平锁存值，再切换推挽；正常、宽限、停止监测、
  复位恢复阶段均保持低电平。
- 脉宽 200 ms、启动宽限 30 s、无心跳超时 5 s、WDT 和 P5.4 协议不变。
- 当前协议是 3 个 payload 脉冲 START、4 个 STOP；STOP 无租约到期。
  冷启动默认停止监测，START 重新开始宽限；仅 WDT 复位且无 POR/LVD
  标志时自动启用监测。不能使用下方旧 ENTER/EXIT 租约流程验收当前固件。

| 检查项 | 新电路预期 | 现场结果 |
|---|---|---|
| Keil Rebuild | 0 Error(s), 0 Warning(s)，重新记录 DATA/CODE 大小 | 待测 |
| 烧录版本 | 新生成的 `RVMDK/list/stc8g1k08_reset.hex`；串口横幅 v1.0.7 | 待测 |
| 冷启动 | GPIO 初始化后 P5.5 拉低，目标设备能通电，监测仍为 stopped | 待测 |
| 正常运行 | START 后心跳正常，P5.5 持续为低，无误复位 | 待测 |
| 无心跳 | 监测启用且满足超时条件后，P5.5 拉高约 200 ms，再拉低 | 待测 |
| 复位恢复 | 目标电源实际恢复，心跳恢复前不重复输出脉冲 | 待测 |
| STOP | P5.5 持续低电平通电，停应用超过 5 s 不复位 | 待测 |
| START | 保持低电平，重新开始 30 s 启动宽限 | 待测 |
| STC 复位/下载 | 记录默认电平、目标电源变化及重新初始化拉低的时刻 | 待测 |

主机 GPIO 测试直接抽取 `main.c` 的真实输出函数，用模拟寄存器检查
初始化、推挽保持、200 ms 状态转换、恢复通电及 P5.4 输入保留；
不能代替实际电气波形、负载供电及 C51 构建验收。200 ms 沿用旧版，
需要确认新电路在此时间内确实完成复位/断电。

## 旧版 v1.0.6 记录（历史参考）

## 版本与交付物

- 固件版本：`1.0.6`
- 工程：`RVMDK/STC8G1K08-RESET.uvproj`
- 入口：`User/main.c`
- 烧录文件：`RVMDK/list/stc8g1k08_reset.hex`
- 正式维护模块：`Driver/src/maintenance_runtime.c`（P5.4 ENTER/RENEW/EXIT 已启用）
- CPU_CHECK 无心跳超时：`5000 ms`
- 启动宽限：`30000 ms`
- AP-RESET 脉冲：`200 ms`
- P5.4 载荷：ENTER/RENEW 为 3 个脉冲，EXIT 为 4 个脉冲
- 维护期间 P5.5：保持高阻
- EXIT 后启动宽限：`30000 ms`
- 测试日期：待填写
- 操作人员：待填写

## 现场验收

| 检查项 | 预期 | 现场结果 |
|---|---|---|
| Keil 构建 | `data=112.0 xdata=0 code=4725`，`0 Error(s), 0 Warning(s)` | 待填写 |
| 正常 CPU_CHECK | PH15/P5.4 每 100 ms 翻转，完整周期约 200 ms | 待填写 |
| 正常保护 | P5.5 保持高阻，V853 连续运行不复位 | 待填写 |
| 心跳停止 | 约 5 s 后输出一次 200 ms AP-RESET 脉冲 | 待填写 |
| 维护 ENTER | 3 个短脉冲后进入维护，P5.5 保持高阻 | 待填写 |
| 维护期间停程序 | 停止 V853 应用超过 5 s 不触发 AP-RESET | 待填写 |
| 维护 EXIT | 4 个短脉冲后恢复正常监控并重新开始 30 s 宽限 | 待填写 |
| 租约到期 | 默认 1800 s 租约到期后自动恢复保护 | 待填写 |
| STC 复位/掉电 | 维护状态丢失，恢复正常保护 | 待填写 |

## P5.4 维护操作顺序

`cpu_check_ctl` 由正常 app 构建安装到应用分区，现场先确认实际路径，例如：

```sh
ls -l /home/application/cpu_check_ctl /home/application/KCMLobbyPhone
```

1. **进入维护（V853 程序仍在运行）**

   ```sh
   /home/application/cpu_check_ctl enter
   ```

   命令通过运行中的心跳线程串行发送 3 脉冲 ENTER 帧，耗时约 2 秒；返回 `sent` 后再停止程序。

2. **停止程序并进行升级/NFS 操作**

   ```sh
   kill -TERM "$(pidof KCMLobbyPhone)"
   # 必要时才使用 kill -KILL；不要在 ENTER 前直接杀程序
   mount -t nfs -o nolock 192.168.0.156:/home/xiaoxiao/workspace/Kocom_Lobbyphone /tmp/nfs
   ```

   维护状态只在 STC RAM 中保存，期间 WDT 仍运行，P5.5 保持高阻释放。

3. **长时间维护续租**

   默认租约为 1800 秒；若维护可能超过 30 分钟，在程序停止期间执行：

   ```sh
   /home/application/cpu_check_ctl renew
   ```

   建议每 10..20 分钟续租一次。`renew`/`exit` 会直接申请 PH15，因此必须在 `KCMLobbyPhone` 停止后执行。

4. **恢复保护并启动程序**

   ```sh
   /home/application/cpu_check_ctl exit
   /home/application/KCMLobbyPhone &
   ```

   必须先发送 `exit` 再启动程序；程序启动后会占用 PH15，之后再发 `exit` 可能因 GPIO 忙而失败。EXIT 帧完成后 STC 重新进入 30 秒启动宽限，随后恢复 5 秒无心跳复位保护。

## 电气测量

| 信号 | 预期 | 测量文件/照片 |
|---|---|---|
| P5.4/CPU_CHECK 正常心跳 | 0..3.3 V，100 ms 翻转 | 待填写 |
| P5.4 ENTER/RENEW 帧 | break 约 900 ms，3 个 payload 短脉冲 | 待填写 |
| P5.4 EXIT 帧 | break 约 900 ms，4 个 payload 短脉冲 | 待填写 |
| P5.5/AP-RESET 空闲 | 高阻释放 | 待填写 |
| P5.5/AP-RESET 复位 | 按配置电平保持约 200 ms | 待填写 |

`AP_RESET_ASSERT_LEVEL=0` 仍是当前低有效工程假设；若实测极性不同，只修改 `Config.h` 后重新构建烧录。真实板上波形和租约/掉电验证完成前，不把 Linux 主机测试当作板级验收。
