# Maintenance Runtime Module - 实现总结

## 完成的工作 (2026-08-20)

### 1. 模块设计与实现

创建了精简的 `maintenance_runtime` 模块，将原来分离的 P5.4 解码器和维护控制器合并：

**文件清单：**
- `Driver/inc/maintenance_runtime.h` - 头文件定义
- `Driver/src/maintenance_runtime.c` - 实现
- `maintenance_runtime_test.c` - 主机测试套件
- `run_maintenance_runtime_test.sh` - 测试脚本
- `maintenance_runtime_integration_example.c` - 集成示例

**内存占用（DATA）：**
```
typedef struct {
    maintenance_runtime_ms_t last_edge_ms;   // 4 字节
    maintenance_runtime_ms_t lease_end_ms;   // 4 字节
    unsigned char state;                      // 1 字节
} maintenance_runtime_t;                      // 总计 9 字节
```

**状态字节位域布局：**
- bit 0-1: decoder_state (IDLE/SYNC/DELIMITER/PAYLOAD)
- bit 2: have_edge (是否已收到第一个边沿)
- bit 3-5: payload_count (载荷脉冲计数 0-7)
- bit 6: maintenance_active (维护模式标志)
- bit 7: reserved (保留)

### 2. 功能特性

**支持的事件：**
- `MRT_EVENT_ENTER` - 3 个载荷脉冲，进入/续约维护模式
- `MRT_EVENT_EXIT` - 4 个载荷脉冲，退出维护模式
- `MRT_EVENT_EXPIRED` - 租约到期，自动恢复正常监控

**协议时序（与原 cpu_check_command_decoder 完全兼容）：**
- BREAK: ≥750ms
- SYNC: 60-110ms
- DELIMITER: 240-360ms
- PAYLOAD: 60-110ms (短脉冲)
- TRAILER: ≥500ms
- 超时: 600ms

**租约管理：**
- 默认租约: 30 分钟 (1800000ms)
- 最小租约: 60 秒
- 最大租约: 1 小时
- RENEW 是隐式的（在维护模式下再次 ENTER）

### 3. 测试验证

所有 8 个主机测试通过：
- ✅ `test_init` - 初始化验证
- ✅ `test_enter_command` - ENTER 命令（3 个载荷脉冲）
- ✅ `test_exit_command` - EXIT 命令（4 个载荷脉冲）
- ✅ `test_lease_expiry` - 租约到期自动恢复
- ✅ `test_normal_heartbeat_ignored` - 普通心跳不触发命令
- ✅ `test_break_interrupted` - BREAK 被打断的处理
- ✅ `test_renew_extends_lease` - RENEW 延长租约
- ✅ `test_time_overflow` - 时间戳溢出处理

### 4. 与原实现的对比

| 特性 | 原实现 | 新实现 |
|------|--------|--------|
| 模块数量 | 2 个（decoder + controller）| 1 个（unified）|
| DATA 占用 | ~25 字节 | 9 字节 |
| 文本命令解析 | 支持（仅 HOST_TEST）| 不支持（二进制事件）|
| RENEW 命令 | 显式命令 | 隐式（重复 ENTER）|
| 协议时序 | 标准 | 完全兼容 |
| 状态管理 | 分离结构体 | 位域打包 |

### 5. 设计优势

1. **内存效率**：从 25B 降到 9B，节省 16B DATA
2. **简化接口**：单一结构体，3 个主要 API
3. **二进制事件**：不需要文本命令解析（那是调试用的）
4. **原子操作**：所有状态在一个字节中，便于快照
5. **零 XDATA**：完全在 DATA 中，符合 v1.0.3 要求

## 集成要求

### INT2 中断服务程序
```c
void int2_isr(void) __interrupt(INT2_VECTOR)
{
    heartbeat_monitor_ms_t now_ms = g_millisecond;
    
    // 1. 先更新心跳（AP_RESET 决策关键）
    heartbeat_monitor_on_falling_edge(&g_heartbeat_monitor, now_ms);
    
    // 2. 再交给维护解码器（不影响心跳）
    maintenance_runtime_on_falling_edge(&g_maintenance_runtime, now_ms);
    
    INT2_FLAG = 0;
}
```

### 主循环
```c
void app_run(void)
{
    while (1) {
        wdt_clear();
        
        // 获取快照
        heartbeat_snapshot(&hb_snapshot, &now_ms);
        
        // 检查维护事件
        mrt_event = maintenance_runtime_poll(&g_maintenance_runtime, now_ms);
        mrt_mode = maintenance_runtime_mode(&g_maintenance_runtime);
        
        // 处理 ENTER/EXIT/EXPIRED
        if (mrt_event == MRT_EVENT_EXIT || mrt_event == MRT_EVENT_EXPIRED) {
            reset_controller_init(&g_reset_controller, now_ms);
        }
        
        // 维护模式：AP_RESET 保持高阻，跳过复位逻辑
        if (mrt_mode == MRT_MODE_MAINTENANCE) {
            if (g_reset_output_active) {
                ap_reset_release();
                g_reset_output_active = 0U;
            }
            continue;
        }
        
        // 正常模式：执行复位逻辑
        // ... (v1.0.3 的复位决策代码)
    }
}
```

## 后续步骤

### Phase 2: 集成到 main.c
1. 添加 `#include "maintenance_runtime.h"`
2. 添加全局变量 `static maintenance_runtime_t g_maintenance_runtime;`
3. 在初始化中调用 `maintenance_runtime_init()`
4. 更新 INT2 ISR（先心跳后解码器）
5. 在主循环中轮询事件并处理

### Phase 3: 静态检查和编译
1. 运行 `reset_firmware_static_test.sh`
2. 检查 DATA 使用量（目标 <256B）
3. Windows Keil 重新构建
4. 验证 `0 Error(s), 0 Warning(s)`
5. 检查 `.map` 文件中的 DATA/XDATA 分配

### Phase 4: 板上验证
1. 烧录新固件
2. 测试 P5.4 ENTER 命令（V853 发送 3 个载荷脉冲）
3. 验证 AP_RESET 在维护模式下保持高阻
4. 测试 EXIT 命令（4 个载荷脉冲）
5. 验证租约到期自动恢复
6. 测试掉电/复位后的恢复行为

## 如果 DATA 超限的应对策略

按优先级排序：

1. **关闭周期 UART 状态打印**（如果启用）
2. **减少局部变量快照大小**（已经是标量快照）
3. **评估是否真的需要某些诊断字段**
4. **绝对不允许**：将 AP_RESET 决策数据移到 XDATA

## API 参考

### 初始化
```c
void maintenance_runtime_init(maintenance_runtime_t *mrt, 
                              maintenance_runtime_ms_t now_ms);
```

### 边沿处理（在 INT2 ISR 中调用）
```c
void maintenance_runtime_on_falling_edge(maintenance_runtime_t *mrt,
                                         maintenance_runtime_ms_t now_ms);
```

### 事件轮询（在主循环中调用）
```c
maintenance_runtime_event_t maintenance_runtime_poll(
    maintenance_runtime_t *mrt,
    maintenance_runtime_ms_t now_ms);
```

### 状态查询
```c
maintenance_runtime_mode_t maintenance_runtime_mode(
    const maintenance_runtime_t *mrt);

unsigned char maintenance_runtime_is_active(
    const maintenance_runtime_t *mrt);
```

## 关键约束

1. **INT2 边沿顺序强制**：先 heartbeat，后 decoder
2. **DATA 限制**：全部状态必须在 DATA 中
3. **维护模式行为**：AP_RESET 必须高阻
4. **租约管理**：RAM-only，掉电/复位后自动恢复正常
5. **WDT 服务**：维护模式下也必须喂狗
