/*
 * maintenance_runtime 集成示例
 *
 * 展示如何在 main.c 中集成新的精简维护模块，同时保持 v1.0.3 的核心不变。
 */

#include "Config.h"
#include "heartbeat_monitor.h"
#include "reset_controller.h"
#include "maintenance_runtime.h"

/* 全局状态 - 全部保留在 DATA */
static volatile heartbeat_monitor_ms_t g_millisecond;
static heartbeat_monitor_t g_heartbeat_monitor;
static reset_controller_t g_reset_controller;
static maintenance_runtime_t g_maintenance_runtime;
static unsigned char g_reset_output_active;

/* 初始化函数中添加 */
void init_all(void)
{
    heartbeat_monitor_ms_t now_ms;

    /* 其他初始化... */
    timer0_init();
    gpio_reset_init();
    wdt_init();

    /* 初始化状态 */
    now_ms = millis_snapshot();
    heartbeat_monitor_init(&g_heartbeat_monitor, now_ms);
    reset_controller_init(&g_reset_controller, now_ms);
    maintenance_runtime_init(&g_maintenance_runtime, now_ms);

    g_reset_output_active = 0U;
}

/* INT2 中断服务程序 - 关键：先更新心跳，再交给解码器 */
void int2_isr(void) __interrupt(INT2_VECTOR)
{
    heartbeat_monitor_ms_t now_ms;

    /* 1. 记录时间戳 */
    now_ms = g_millisecond;

    /* 2. 更新心跳监控（这是 AP_RESET 决策的关键）*/
    heartbeat_monitor_on_falling_edge(&g_heartbeat_monitor, now_ms);

    /* 3. 交给维护协议解码器（不影响心跳记录）*/
    maintenance_runtime_on_falling_edge(&g_maintenance_runtime, now_ms);

    /* 清除中断标志 */
    INT2_FLAG = 0;
}

/* 主循环 */
void app_run(void)
{
    heartbeat_monitor_ms_t now_ms;
    heartbeat_monitor_t hb_snapshot;
    heartbeat_monitor_status_t hb_status;
    maintenance_runtime_event_t mrt_event;
    maintenance_runtime_mode_t mrt_mode;
    reset_controller_event_t reset_event;
    unsigned char reset_expired;

    while (1) {
        wdt_clear();

        /* 1. 获取当前时间和心跳快照（v1.0.3 原子快照方式）*/
        heartbeat_snapshot(&hb_snapshot, &now_ms);

        /* 2. 检查维护模式事件 */
        mrt_event = maintenance_runtime_poll(&g_maintenance_runtime, now_ms);
        mrt_mode = maintenance_runtime_mode(&g_maintenance_runtime);

        if (mrt_event == MRT_EVENT_ENTER) {
            /* 进入或续约维护模式 */
            /* 可选：UART 日志 "MNT ENTER\r\n" */
        } else if (mrt_event == MRT_EVENT_EXIT) {
            /* 退出维护模式 */
            /* 可选：UART 日志 "MNT EXIT\r\n" */
            /* 重新初始化复位控制器，恢复启动宽限期 */
            reset_controller_init(&g_reset_controller, now_ms);
        } else if (mrt_event == MRT_EVENT_EXPIRED) {
            /* 租约到期，自动恢复监控 */
            /* 可选：UART 日志 "MNT EXPIRED\r\n" */
            reset_controller_init(&g_reset_controller, now_ms);
        }

        /* 3. 如果在维护模式，AP_RESET 保持高阻 */
        if (mrt_mode == MRT_MODE_MAINTENANCE) {
            if (g_reset_output_active != 0U) {
                ap_reset_release();
                g_reset_output_active = 0U;
            }
            continue;  /* 跳过复位逻辑 */
        }

        /* 4. 正常监控模式：更新复位控制器 */
        hb_status = heartbeat_monitor_status(
            &hb_snapshot, now_ms,
            HEARTBEAT_STARTUP_GRACE_MS,
            HEARTBEAT_MONITOR_TIMEOUT_MS);

        reset_event = reset_controller_update(
            &g_reset_controller, hb_status, now_ms);

        /* 5. 处理复位事件 */
        if (reset_event == RESET_CONTROLLER_ASSERT) {
            if (g_reset_output_active == 0U) {
                ap_reset_assert();
                g_reset_output_active = 1U;
            }
        }

        reset_expired = reset_controller_pulse_expired(
            &g_reset_controller, now_ms);

        if (reset_expired != 0U) {
            if (g_reset_output_active != 0U) {
                ap_reset_release();
                g_reset_output_active = 0U;
            }
        }

        /* 6. 可选：周期状态打印（如果 DATA 充足）*/
        /* 如果 DATA 超限，关闭此打印 */
    }
}

/*
 * 关键设计要点：
 *
 * 1. DATA 使用：
 *    - g_millisecond: 4 字节
 *    - g_heartbeat_monitor: 13 字节
 *    - g_reset_controller: ~8 字节
 *    - g_maintenance_runtime: 9 字节
 *    - 局部变量工作区: ~20 字节
 *    总计约 54 字节，远低于 256B 限制
 *
 * 2. 心跳边沿顺序：
 *    INT2 ISR 先调用 heartbeat_monitor_on_falling_edge()
 *    再调用 maintenance_runtime_on_falling_edge()
 *    确保协议解析不影响 AP_RESET 决策
 *
 * 3. 维护模式处理：
 *    - ENTER/RENEW 事件：保持 AP_RESET 高阻
 *    - EXIT/EXPIRED 事件：重新初始化复位控制器
 *    - 维护期间跳过所有复位逻辑
 *
 * 4. 无 XDATA 依赖：
 *    所有 AP_RESET 决策相关数据保留在 DATA
 *    单次原子快照获取时间和心跳状态
 *
 * 5. 如果 DATA 超限：
 *    优先关闭周期 UART 状态打印
 *    不能将复位决策数据移到 XDATA
 */
