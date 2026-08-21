# STC8G1K08 维护模式问题诊断与解决方案

## 问题现状

1. ✅ V853成功发送ENTER命令（日志确认）
2. ✅ V853心跳恢复后不再重启
3. ❌ **STC未进入维护模式**（kill V853进程后仍触发reset）

## 根本原因分析

### 可能原因1：STC固件版本问题
- 虽然v1.0.4编译成功（DATA=122/256），但板子上可能还是v1.0.3
- **验证方法**：v1.0.3有UART日志，v1.0.4禁用了UART
- **你的反馈**："已经不打印了" → 说明是v1.0.4

### 可能原因2：maintenance_runtime模块未正确工作
检查点：
1. INT2 ISR是否调用 `maintenance_runtime_on_falling_edge()`
2. 主循环是否调用 `maintenance_runtime_poll()`
3. 维护模式判断逻辑是否正确

### 可能原因3：内存问题导致运行时错误
- DATA使用率47.7%（122/256），接近一半
- 可能存在栈溢出或数据损坏
- 8051架构的DATA区域非常有限

## 诊断方法

### 方法1：确认固件版本
用STC-ISP读取芯片，检查：
- CODE SIZE（v1.0.4应该比v1.0.3大）
- 比对hex文件内容

### 方法2：添加GPIO指示灯
修改STC固件，添加维护模式LED指示：
```c
// 在进入维护模式时点亮LED
if (mrt_event == MRT_EVENT_ENTER) {
    P55 = 1;  // 点亮LED表示进入维护模式
}
```

### 方法3：降低内存使用
当前DATA使用122/256，可以优化：

#### 3.1 减少全局变量
检查是否有不必要的全局变量

#### 3.2 使用XDATA（间接寻址）
对于不频繁访问的数据，使用XDATA：
```c
// 将不频繁访问的结构体放到XDATA
maintenance_runtime_t xdata g_maintenance_runtime;
```

#### 3.3 简化维护协议时序
当前的维护协议decoder较复杂，可以简化

## 解决方案

### 方案1：重新烧录确认（推荐立即执行）

**步骤：**
1. 确认hex文件
   ```bash
   ls -lh /home/xiaoxiao/workspace/Kocom_Lobbyphone/STC8G1K08/firmware/RVMDK/list/stc8g1k08_reset.hex
   ```
   文件大小应该约8.4KB

2. 用STC-ISP烧录
   - 选择正确的hex文件
   - 确认烧录成功
   - 读回验证

3. 测试ENTER命令
   ```bash
   /home/application/cpu_check_ctl enter
   ```

### 方案2：优化内存使用（如果方案1失败）

#### 2.1 将maintenance_runtime移到XDATA
修改 `firmware/User/main.c`:
```c
// 将 g_maintenance_runtime 移到XDATA
heartbeat_monitor_t g_heartbeat_monitor;
reset_controller_t g_reset_controller;
maintenance_runtime_t xdata g_maintenance_runtime;  // 添加xdata关键字
```

#### 2.2 简化解码器状态
减少maintenance_runtime_t结构体大小

#### 2.3 重新编译并烧录

### 方案3：备选方案 - 简化维护协议

如果内存确实不够，可以简化协议：

#### 3.1 使用更简单的ENTER信号
- 当前：BREAK(900ms) + SYNC + DELIMITER + 3个脉冲
- 简化：固定间隔的5个脉冲（例如每个200ms）

#### 3.2 修改V853和STC的协议实现

## 立即执行的行动

### 第一步：验证固件
```bash
# 1. 检查hex文件
ls -lh /home/xiaoxiao/workspace/Kocom_Lobbyphone/STC8G1K08/firmware/RVMDK/list/stc8g1k08_reset.hex

# 2. 计算hex文件MD5
md5sum /home/xiaoxiao/workspace/Kocom_Lobbyphone/STC8G1K08/firmware/RVMDK/list/stc8g1k08_reset.hex
```

### 第二步：添加调试指示
如果有空余的GPIO，可以添加LED指示维护模式状态

### 第三步：内存优化
如果确认是内存问题，按方案2执行

## 预期结果

成功后的表现：
1. 发送 `cpu_check_ctl enter` 后，V853不会重启
2. kill V853进程后，STC进入维护模式，**不触发reset**
3. 30分钟后自动退出维护模式，恢复正常监控

## 下一步建议

1. **立即**：用STC-ISP重新烧录v1.0.4固件，确认版本
2. **如果仍失败**：尝试方案2（XDATA优化）
3. **如果还失败**：考虑方案3（简化协议）

## 技术细节备查

### 当前内存使用
- DATA: 122/256 (47.7%)
- CODE: 符合STC8G1K08的8KB限制
- XDATA: 未使用（可用于优化）

### 关键代码位置
- INT2 ISR: `firmware/User/main.c:176-185`
- 主循环维护模式处理: `firmware/User/main.c:247-263`
- 维护协议解码器: `firmware/Driver/src/maintenance_runtime.c`

### 时序参数
- V853发送: BREAK=900ms, SYNC=80ms, DELIMITER=300ms
- STC期望: BREAK≥750ms, SHORT=60-110ms, DELIMITER=240-360ms
- **时序匹配，不是问题根源**
