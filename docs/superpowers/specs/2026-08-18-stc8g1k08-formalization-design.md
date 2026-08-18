# STC8G1K08 正式固件收敛设计

## 背景与目标

STC8G1K08 外部复位固件已经完成板上验证：V853 进程暂停后，STC 能检测心跳超时、输出 P5.5 复位脉冲，V853 重新启动并恢复心跳。因此下一阶段不是重新设计复位算法，而是把已验证实现整理成正式交付形态，并修复有明确证据支持的代码质量问题。

目标：

1. 正式工程目录只保留一个可交付的 Keil 工程：`STC8G1K08-RESET.uvproj`。
2. 生产入口统一命名为 `firmware/User/main.c`。
3. 保持当前心跳与复位契约：100 ms 翻转、1 s 超时、30 s 启动宽限、200 ms 复位脉冲、内部 WDT 约 2.1 s。
4. 修复 V853 显示模式函数的职责/空指针问题，以及心跳线程停止时的 join 失败清理问题。
5. 保留验证代码的可追溯性，但不让 SMOKE/MONITOR 成为正式构建目标。

非目标：

- 不修改 `sdk/V85X_Tina_V1.3/`。
- 不改变 V853 的 PH15、STC P5.4/P5.5 硬件连接和心跳时序。
- 不引入未经芯片资料证实的 INT2 配置写入。
- 不借此机会修改 SIP/DHCP 业务逻辑、LVGL 定时器风格或其他无关模块。

## 正式工程布局

保留并作为唯一正式工程：

```text
STC8G1K08/firmware/RVMDK/STC8G1K08-RESET.uvproj
STC8G1K08/firmware/User/main.c
STC8G1K08/firmware/User/Config.h
STC8G1K08/firmware/Driver/inc/heartbeat_monitor.h
STC8G1K08/firmware/Driver/inc/reset_controller.h
STC8G1K08/firmware/Driver/inc/stc8g1k08_regs.h
STC8G1K08/firmware/Driver/src/heartbeat_monitor.c
STC8G1K08/firmware/Driver/src/reset_controller.c
```

`STC8G1K08-RESET.uvproj` 的目标名继续明确为 RESET 固件。本次保持输出名 `stc8g1k08_reset.hex`，因为它已经对应现场烧录记录；用户允许后续修改输出名，但本次不把输出重命名和源文件重命名混在一起验证。

SMOKE/MONITOR 的工程、入口源文件和对应静态检查脚本移动到 `STC8G1K08/firmware/validation/` 作为历史验证材料，不加入正式工程。归档材料明确标注为验证快照，不作为日常编译目标。不会直接丢弃这些文件。

同时更新：

- 工程 XML 中的源文件路径：`Reset.c` -> `main.c`。
- `Config.h` 和寄存器头文件中仍写着“冒烟固件”的注释/Include guard。
- RESET 静态检查脚本、README、验证记录中的工程入口与输出路径。

## STC 固件代码调整

### 入口职责

将当前 `Reset.c` 重命名为 `main.c`，不改变中断向量或链接行为。把入口拆成三个小边界：

- `app_init()`：GPIO、UART、Timer0、心跳监控、复位控制器、WDT 和 INT2 使能。
- `app_report(const heartbeat_monitor_t *, heartbeat_monitor_ms_t)`：使用主循环已经取得的快照输出状态，避免再次读取共享状态。
- `app_run()`：快照、状态机更新、复位输出应用、WDT 清零和定时报告。

`main()` 只负责调用 `app_init()` 和 `app_run()`。UART 输出格式保持现有内容，避免为了减少几行拼接而引入 C51 可变参数格式化器和额外代码体积。

### 并发与寄存器边界

- 保留 `millis_snapshot()` 和 `heartbeat_snapshot()` 保存/恢复 `EA` 的实现。
- 不增加审计中猜测的 `IT2 = 1`。本地 STC 官方示例明确 INT2 为固定下降沿，当前 `INT_CLKO` bit4 使能方式与芯片资料一致。
- `int2_isr()` 继续记录下降沿；同优先级 8051 中断不会在 Timer0 ISR 中嵌套执行，当前时间戳读取在已验证硬件上保持不变。
- 给 `WDT_PRESCALE` 增加“6 对应 128 分频”的注释，定义 P3.0/P3.1 等端口掩码，减少魔法数字。

## V853 代码调整

### `video_mode_enable()`

只保留显示模式职责：

1. 检查 `lv_disp_get_default()` 和 `disp->driver`，失败时记录错误并返回。
2. 设置透明度、背景透明度和当前屏幕样式。
3. 开启视频模式时清空绘制缓冲、使当前屏幕失效并立即刷新。

移除其中的 `sensor_service_init()` 和 `front_white_light_init()`。这两个服务已经在 `app_run()` 初始化，并在退出路径关闭；视频模式切换不再重复改变它们的生命周期。

### `cpu_check_heartbeat_stop()`

保持现有“先停止线程、再释放 GPIO”的顺序，但把 join 结果作为清理前置条件：

1. 在锁内把 `running` 置零并复制线程句柄。
2. 解锁后 `pthread_join()`。
3. join 失败时记录错误并立即返回，保留 GPIO 和初始化状态，允许后续停止调用重试；绝不在线程可能仍访问时释放 GPIO。
4. join 成功后再拉低、释放 GPIO，并在锁内清理状态。

线程循环中的局部变量移到函数顶部，保持项目对旧 C 编译器风格的兼容性。

## 不采用的审计建议

- 不实现 `uart1_printf`：当前每秒一次诊断输出已验证，变参格式化会增加 8051 代码体积和新的格式错误面。
- 不给 UART 忙等加入任意计数超时：计数与波特率没有稳定的时间关系；当前 WDT 已能覆盖主循环卡死，若未来复现 TX 卡死再单独设计有时间基准的处理。
- 不把 `pulse_ms == 0` 简单改成 `return`：这会留下“已进入 ASSERT 但永不释放”的边界风险。若要防御，应先定义无效配置的安全策略并增加测试，本次正式化先保持配置常量非零。
- 不恢复 `app_prepare_sip_account()` 中被注释掉的 DHCP `return`：这是业务策略，不应依据静态审计猜测改变。
- 不为纯风格项重写 `page_timer_del_all()` 的 `goto`，避免扩大 LVGL 生命周期变更范围。

## 验证方案

### 静态/主机验证

- 更新并运行 STC RESET 静态检查，确认只引用 `main.c` 和正式工程。
- 运行 `heartbeat_monitor`、`reset_controller` 主机状态测试，保留超时、恢复、二次复位和时间回绕覆盖。
- 增加 `pulse_ms` 非零配置断言；不改变生产路径。
- 扩展 `system_static_test`：确认 `video_mode_enable()` 不含服务初始化，并包含显示对象判空。
- 扩展 `cpu_check_heartbeat_static_test` 或增加 focused test：确认 join 失败分支在 GPIO 释放之前返回。

### Keil 构建验收

Windows Keil 构建日志必须包含：

```text
compiling main.c...
compiling heartbeat_monitor.c...
compiling reset_controller.c...
0 Error(s), 0 Warning(s)
```

并确认生成单一正式 HEX，不能出现 unresolved external。

### 板上回归

1. 上电观察 STC 启动横幅和正常 `healthy/monitoring`。
2. 正常运行 10 分钟，确认无误复位。
3. 连续 3 次执行 `ps`、`kill -STOP <KCMLobbyPhone PID>`，确认每次约 1 s 超时后 V853 重启，STC 复位计数各从零开始。
4. V853 启动心跳延迟约 16 s 时不触发误复位，30 s 宽限保持有效。
5. V853 恢复后确认 P5.5 回到 `high-z`、心跳回到 `healthy/monitoring`。
6. 使用 `kill -CONT` 仅恢复被暂停的旧进程，不把它作为复位成功的判据。

## 交付与回滚

- 交付物是唯一正式 Keil 工程、其生成 HEX、更新后的 README/验证记录和主机/静态测试脚本。
- 归档验证材料保留在 `firmware/validation/`，便于对比，不参与正式构建。
- 若板上回归失败，优先回滚 V853 质量修复或入口重命名，复用本次变更前已验证的 `STC8G1K08-RESET.uvproj` 和入口源文件；不回滚纯逻辑测试文件。
