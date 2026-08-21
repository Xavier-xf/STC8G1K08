#include "Config.h"
#include "heartbeat_monitor.h"
#include "reset_controller.h"
#include "maintenance_runtime.h"
#include "uart.h"

/*
 * STC8G1K08 正式外部复位固件
 *
 * P5.4/INT2 只接收 CPU_CHECK 下降沿。
 * P5.5 默认输入高阻，只有 RESET_ASSERT 状态才驱动 Q23/AP-RESET。
 * 当前 AP_RESET_ASSERT_LEVEL=0 为低有效假设，后续可只改配置宏。
 */

#define P5_CPU_CHECK_MASK 0x10
#define P5_AP_RESET_MASK 0x20
#define P3_UART_RX_MASK 0x01
#define P3_UART_TX_MASK 0x02

static volatile heartbeat_monitor_ms_t g_millisecond;
static heartbeat_monitor_t g_heartbeat_monitor;
static reset_controller_t g_reset_controller;
static maintenance_runtime_t g_maintenance_runtime;
static unsigned char g_reset_output_active;

static void timer0_init(void)
{
    TMOD &= (unsigned char)~0x03;
    AUXR |= 0x80;
    TH0 = (unsigned char)(TIMER0_RELOAD >> 8);
    TL0 = (unsigned char)TIMER0_RELOAD;
    TF0 = 0;
    ET0 = 1;
    TR0 = 1;
}


static void ap_reset_release(void)
{
    P5M1 |= P5_AP_RESET_MASK;
    P5M0 &= (unsigned char)~P5_AP_RESET_MASK;
    P5 |= P5_AP_RESET_MASK;
}

static void ap_reset_assert(void)
{
    /* 先回到高阻，再预置锁存值，最后切换推挽，避免切换瞬间毛刺。 */
    ap_reset_release();
    #if AP_RESET_ASSERT_LEVEL != 0U
        P5 |= P5_AP_RESET_MASK;
#else
        P5 &= (unsigned char)~P5_AP_RESET_MASK;
#endif
    P5M1 &= (unsigned char)~P5_AP_RESET_MASK;
    P5M0 |= P5_AP_RESET_MASK;
}

static void gpio_reset_init(void)
{
    P5M1 |= P5_CPU_CHECK_MASK;
    P5M0 &= (unsigned char)~P5_CPU_CHECK_MASK;
    P5 |= P5_CPU_CHECK_MASK;
    ap_reset_release();

    P3M1 |= P3_UART_RX_MASK;
    P3M0 &= (unsigned char)~P3_UART_RX_MASK;
    P3M1 &= (unsigned char)~P3_UART_TX_MASK;
    P3M0 |= P3_UART_TX_MASK;
    P3 |= P3_UART_TX_MASK;
}

static void wdt_init(void)
{
    WDT_CONTR = (unsigned char)(WDT_PRESCALE | WDT_ENABLE_MASK);
}

static void wdt_clear(void)
{
    WDT_CONTR |= WDT_CLEAR_MASK;
}


static heartbeat_monitor_ms_t millis_snapshot(void)
{
    heartbeat_monitor_ms_t value;
    unsigned char saved_ea = EA;

    EA = 0;
    value = g_millisecond;
    EA = saved_ea;
    return value;
}

static void heartbeat_snapshot(heartbeat_monitor_t *snapshot,
                               heartbeat_monitor_ms_t *now_ms)
{
    unsigned char saved_ea = EA;

    EA = 0;
    *now_ms = g_millisecond;
    *snapshot = g_heartbeat_monitor;
    EA = saved_ea;
}

static void maintenance_runtime_snapshot(
    maintenance_runtime_event_t *event,
    maintenance_runtime_mode_t *mode,
    heartbeat_monitor_ms_t now_ms)
{
    unsigned char saved_ea = EA;

    EA = 0;
    *event = maintenance_runtime_poll(&g_maintenance_runtime, now_ms);
    *mode = maintenance_runtime_mode(&g_maintenance_runtime);
    EA = saved_ea;
}

static void app_resume_normal(heartbeat_monitor_ms_t now_ms)
{
    unsigned char saved_ea = EA;

    EA = 0;
    heartbeat_monitor_init(&g_heartbeat_monitor, now_ms);
    reset_controller_init(&g_reset_controller, now_ms);
    g_reset_output_active = 0U;
    ap_reset_release();
    EA = saved_ea;
}


#if STC8G1K08_UART_LOG_ENABLE
static const char *heartbeat_status_name(heartbeat_monitor_status_t status)
{
    if (status == HEARTBEAT_MONITOR_STARTUP) return "startup";
    if (status == HEARTBEAT_MONITOR_HEALTHY) return "healthy";
    return "timeout";
}

static const char *reset_state_name(reset_controller_state_t state)
{
    if (state == RESET_CONTROLLER_STARTUP) return "startup";
    if (state == RESET_CONTROLLER_MONITORING) return "monitoring";
    if (state == RESET_CONTROLLER_ASSERT) return "assert";
    return "wait-recovery";
}

static void app_report(const heartbeat_monitor_t *heartbeat,
                       heartbeat_monitor_ms_t now_ms)
{
    heartbeat_monitor_status_t heartbeat_status;

    heartbeat_status = heartbeat_monitor_status(heartbeat, now_ms,
                                                 RESET_HEARTBEAT_GRACE_MS,
                                                 HEARTBEAT_MONITOR_TIMEOUT_MS);

    uart1_puts("reset heartbeat=");
    uart1_puts(heartbeat_status_name(heartbeat_status));
    uart1_puts(" state=");
    uart1_puts(reset_state_name(g_reset_controller.state));
    uart1_puts(" edges=");
    uart1_put_u32(heartbeat->edge_count);
    uart1_puts(" age_ms=");
    uart1_put_u32((unsigned long)(now_ms - heartbeat->last_edge_ms));
    uart1_puts(" seen=");
    uart1_putc(heartbeat->edge_seen != 0U ? '1' : '0');
    uart1_puts(" P54=");
    uart1_putc(CPU_CHECK ? '1' : '0');
    uart1_puts(" P55=");
    uart1_putc(AP_RESET_TEST ? '1' : '0');
    uart1_puts(" output=");
    uart1_puts(g_reset_output_active ? "assert" : "high-z");
    uart1_puts(" resets=");
    uart1_put_u32(g_reset_controller.reset_count);
    uart1_puts(" wdt=on\r\n");
}
#endif

static void apply_reset_output(unsigned char active)
{
    if (active == g_reset_output_active) return;
    if (active != 0U) {
        ap_reset_assert();
    } else {
        ap_reset_release();
    }
    g_reset_output_active = active;
#if STC8G1K08_UART_LOG_ENABLE
    uart1_puts(active != 0U ? "AP_RESET assert\r\n" :
                             "AP_RESET release\r\n");
#endif
}


void timer0_isr(void) interrupt 1
{
    TH0 = (unsigned char)(TIMER0_RELOAD >> 8);
    TL0 = (unsigned char)TIMER0_RELOAD;
    ++g_millisecond;
}

void int2_isr(void) interrupt 10
{
    heartbeat_monitor_ms_t now_ms = g_millisecond;

    /* 1. 先更新心跳监控（AP_RESET 决策的关键数据）*/
    heartbeat_monitor_on_falling_edge(&g_heartbeat_monitor, now_ms);

    /* 2. 再交给维护协议解码器（不影响心跳记录）*/
    maintenance_runtime_on_falling_edge(&g_maintenance_runtime, now_ms);
}
static void app_init(void)
{
    gpio_reset_init();
#if STC8G1K08_UART_LOG_ENABLE
    uart1_init();
#endif
    timer0_init();
    heartbeat_monitor_init(&g_heartbeat_monitor, 0UL);
    reset_controller_init(&g_reset_controller, 0UL);
    maintenance_runtime_init(&g_maintenance_runtime, 0UL);
    g_reset_output_active = 0U;
    INT_CLKO |= INT2_ENABLE_MASK;
    wdt_init();
    EA = 1;

#if STC8G1K08_UART_LOG_ENABLE
    uart1_puts("\r\nSTC8G1K08 reset firmware v" STC8G1K08_FIRMWARE_VERSION "\r\n");
    uart1_puts("P54=CPU_CHECK INT2 falling edge\r\n");
    uart1_puts("P55=AP-RESET controlled pulse, WDT=ON\r\n");
    uart1_puts("grace=30000ms timeout=5000ms pulse=200ms wdt=128x\r\n");
#endif
}


static void app_run(void)
{
    heartbeat_monitor_ms_t last_report;
    heartbeat_monitor_ms_t now_ms;
    heartbeat_monitor_t heartbeat;
    heartbeat_monitor_status_t heartbeat_status;
    maintenance_runtime_event_t mrt_event;
    maintenance_runtime_mode_t mrt_mode;

    last_report = millis_snapshot();
    for (;;) {
        heartbeat_snapshot(&heartbeat, &now_ms);

        /* 轮询维护运行时事件 */
        maintenance_runtime_snapshot(&mrt_event, &mrt_mode, now_ms);

        /* 处理维护模式事件 */
        if (mrt_event == MRT_EVENT_ENTER) {
#if STC8G1K08_UART_LOG_ENABLE
            uart1_puts("MNT ENTER\r\n");
#endif
        } else if (mrt_event == MRT_EVENT_EXIT) {
#if STC8G1K08_UART_LOG_ENABLE
            uart1_puts("MNT EXIT\r\n");
#endif
            /* 退出维护模式：原子重置心跳和复位状态，恢复启动宽限期 */
            app_resume_normal(now_ms);
        } else if (mrt_event == MRT_EVENT_EXPIRED) {
#if STC8G1K08_UART_LOG_ENABLE
            uart1_puts("MNT EXPIRED\r\n");
#endif
            /* 租约到期：原子重置心跳和复位状态，恢复启动宽限期 */
            app_resume_normal(now_ms);
        }

        if (mrt_event == MRT_EVENT_EXIT || mrt_event == MRT_EVENT_EXPIRED) {
            wdt_clear();
            continue;
        }

        /* 维护模式：AP_RESET 保持高阻，跳过复位逻辑 */
        if (mrt_mode == MRT_MODE_MAINTENANCE) {
            if (g_reset_output_active != 0U) {
                apply_reset_output(0U);
            }
            wdt_clear();
#if STC8G1K08_UART_LOG_ENABLE
            if ((heartbeat_monitor_ms_t)(now_ms - last_report) >= 1000UL) {
                last_report = now_ms;
                uart1_puts("MNT mode=maintenance edges=");
                uart1_put_u32(heartbeat.edge_count);
                uart1_puts("\r\n");
            }
#endif
            continue;
        }

        /* 正常监控模式：执行复位逻辑 */
        heartbeat_status = heartbeat_monitor_status(
            &heartbeat, now_ms, RESET_HEARTBEAT_GRACE_MS,
            HEARTBEAT_MONITOR_TIMEOUT_MS);
        reset_controller_update(&g_reset_controller, heartbeat_status,
                                now_ms, AP_RESET_PULSE_MS);
        apply_reset_output(
            reset_controller_output_active(&g_reset_controller));
        wdt_clear();
#if STC8G1K08_UART_LOG_ENABLE
        if ((heartbeat_monitor_ms_t)(now_ms - last_report) >= 1000UL) {
            last_report = now_ms;
            app_report(&heartbeat, now_ms);
        }
#endif
    }
}
void main(void)
{
    app_init();
    app_run();
}
