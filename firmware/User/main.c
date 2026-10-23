#include "Config.h"
#include "heartbeat_monitor.h"
#include "reset_controller.h"
#include "cpu_check_link.h"
#include "uart.h"

/* P5.4/CPU_CHECK 由 cpu_check_link 独占；本文件只管理 P5.5 和 UART 引脚。 */
#define P5_AP_RESET_MASK 0x20
#define P3_UART_RX_MASK 0x01
#define P3_UART_TX_MASK 0x02

/*
 * 中断共享数据。Timer0 与 INT2 保持同一默认优先级、互不嵌套，因此 INT2
 * 可直接读取 32 位毫秒计数；主循环读取时关总中断取快照。
 */
static volatile heartbeat_monitor_ms_t g_millisecond;
static heartbeat_monitor_t g_heartbeat_monitor;

/* 仅主循环访问。 */
static reset_controller_t g_reset_controller;
static unsigned char g_reset_output_active;
static unsigned char g_monitor_enabled;

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
#if AP_RESET_ASSERT_LEVEL != 0U
    P5 &= (unsigned char)~P5_AP_RESET_MASK;
#else
    P5 |= P5_AP_RESET_MASK;
#endif
}

static void ap_reset_assert(void)
{
    /* 始终保持推挽模式，只切换输出电平。 */
#if AP_RESET_ASSERT_LEVEL != 0U
    P5 |= P5_AP_RESET_MASK;
#else
    P5 &= (unsigned char)~P5_AP_RESET_MASK;
#endif
}

static void gpio_board_init(void)
{
    /* 先预置通电电平，再切换推挽；运行中不再释放为高阻。 */
    ap_reset_release();
    P5M0 |= P5_AP_RESET_MASK;
    P5M1 &= (unsigned char)~P5_AP_RESET_MASK;

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

/* 从停止进入监测：清空本轮心跳并重新开始宽限。此时复位输出必为释放。 */
static void monitor_restart(heartbeat_monitor_ms_t now_ms)
{
    unsigned char saved_ea = EA;

    EA = 0;
    heartbeat_monitor_init(&g_heartbeat_monitor, now_ms);
    reset_controller_init(&g_reset_controller, now_ms);
    EA = saved_ea;
}

/*
 * 开机宽限只属于 STARTUP 阶段。其余阶段传 0，避免运行约 49.7 天后
 * 32 位差值回绕而再次报告 STARTUP，导致 30 s 内主机死机不被复位。
 */
static heartbeat_monitor_ms_t reset_grace_ms(void)
{
    return g_reset_controller.state == RESET_CONTROLLER_STARTUP ?
           RESET_HEARTBEAT_GRACE_MS : 0UL;
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

/* 最长约 140 字符；改动字段须同步复核 cpu_check_link.c 的 LINK_HANDOFF_MS 预算。 */
static void app_report(const heartbeat_monitor_t *heartbeat,
                       heartbeat_monitor_status_t heartbeat_status,
                       heartbeat_monitor_ms_t now_ms)
{
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
    uart1_putc(AP_RESET ? '1' : '0');
    uart1_puts(" output=");
    uart1_puts(g_reset_output_active ? "assert" : "power-on");
    uart1_puts(" resets=");
    uart1_put_u32(g_reset_controller.reset_count);
    uart1_puts(" wdt=on\r\n");
}
#endif

/* 运行期 P5.5 的唯一改动入口。 */
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
    cpu_check_link_tick();
}

void int2_isr(void) interrupt 10
{
    heartbeat_monitor_ms_t now_ms = g_millisecond;
    unsigned int age_ms = cpu_check_link_on_falling_edge((unsigned int)now_ms);

    /* 只记录已确认的普通心跳；协议边沿和自身应答返回 0。 */
    if (age_ms)
        heartbeat_monitor_on_falling_edge(&g_heartbeat_monitor, now_ms - age_ms);
}

static void app_init(void)
{
    unsigned char reset_cause;
    unsigned char wdt_reset;
    unsigned char power_or_lvd_reset;

    /* 必须在 wdt_init() 改写 WDT_CONTR 之前取样复位原因。 */
    reset_cause = (unsigned char)((PCON & (PCON_POF_MASK | PCON_LVDF_MASK)) |
                                  (WDT_CONTR & WDT_FLAG_MASK));
    PCON &= (unsigned char)~(PCON_POF_MASK | PCON_LVDF_MASK);
    wdt_reset = (unsigned char)((reset_cause & WDT_FLAG_MASK) != 0U);
    power_or_lvd_reset = (unsigned char)((reset_cause &
                                          (PCON_POF_MASK | PCON_LVDF_MASK)) != 0U);

    gpio_board_init();
    cpu_check_link_init();
#if STC8G1K08_UART_LOG_ENABLE
    uart1_init();
#endif
    timer0_init();
    heartbeat_monitor_init(&g_heartbeat_monitor, 0UL);
    reset_controller_init(&g_reset_controller, 0UL);
    /* 仅内部看门狗复位自动恢复监测；冷上电/低压复位等待主机 START。 */
    g_monitor_enabled = (unsigned char)(wdt_reset != 0U && power_or_lvd_reset == 0U);
    g_reset_output_active = 0U;
    INT_CLKO |= INT2_ENABLE_MASK;
    wdt_init();
    EA = 1;

#if STC8G1K08_UART_LOG_ENABLE
    /* 约 220 ms；上电后最早的完整请求帧也需 600 ms 以上，不会与交接期限冲突。 */
    uart1_puts("\r\nSTC8G1K08 reset firmware v" STC8G1K08_FIRMWARE_VERSION "\r\n");
    uart1_puts("protocol=short-frame 3/5 bytes\r\n");
    uart1_puts("P54=CPU_CHECK INT2 falling edge\r\n");
#if AP_RESET_ASSERT_LEVEL != 0U
    uart1_puts("P55=LOW power-on, HIGH reset, WDT=ON\r\n");
#else
    uart1_puts("P55=HIGH power-on, LOW reset, WDT=ON\r\n");
#endif
    uart1_puts("grace=");
    uart1_put_u32(RESET_HEARTBEAT_GRACE_MS);
    uart1_puts("ms timeout=");
    uart1_put_u32(HEARTBEAT_MONITOR_TIMEOUT_MS);
    uart1_puts("ms pulse=");
    uart1_put_u32(AP_RESET_PULSE_MS);
    uart1_puts("ms recovery=");
    uart1_put_u32(RESET_RECOVERY_WINDOW_MS);
    uart1_puts("ms wdt=");
    uart1_put_u32(WDT_DIVIDER);
    uart1_puts("x\r\n");
#endif
}

static void app_run(void)
{
    heartbeat_monitor_ms_t last_report;
    heartbeat_monitor_ms_t now_ms;
    heartbeat_monitor_t heartbeat;
    heartbeat_monitor_status_t heartbeat_status;
    unsigned char command;
    unsigned char reply_result;
    unsigned char reply_status;

    last_report = millis_snapshot();
    for (;;) {
        heartbeat_snapshot(&heartbeat, &now_ms);

        EA = 0;
        command = cpu_check_link_poll();
        EA = 1;
        if (command) {
            reply_result = LINK_RESULT_OK;
            if (command == LINK_COMMAND_START) {
                /* 已启用时重复 START 不重置宽限和状态。 */
                if (!g_monitor_enabled) {
                    g_monitor_enabled = 1U;
                    monitor_restart(now_ms);
                    heartbeat_snapshot(&heartbeat, &now_ms);
                }
            } else if (command == LINK_COMMAND_STOP) {
                /* 先释放复位输出，使应答状态与引脚一致。 */
                g_monitor_enabled = 0U;
                apply_reset_output(0U);
            } else if (command != LINK_COMMAND_READ) {
                reply_result = LINK_RESULT_INVALID;
            }
            reply_status = g_monitor_enabled ? LINK_STATUS_ENABLED : 0U;
            if (heartbeat.edge_seen) reply_status |= LINK_STATUS_HEARTBEAT;
            if (g_monitor_enabled)
                reply_status |= (unsigned char)((g_reset_controller.state + 1U) <<
                                                LINK_STATUS_PHASE_SHIFT);
            EA = 0;
            cpu_check_link_reply(reply_result, reply_status);
            EA = 1;
        }

        /* 监测停止：复位输出已在 STOP 时释放，内部 WDT 继续运行。 */
        if (!g_monitor_enabled) {
            wdt_clear();
#if STC8G1K08_UART_LOG_ENABLE
            if ((heartbeat_monitor_ms_t)(now_ms - last_report) >= 1000UL) {
                last_report = now_ms;
                uart1_puts("MNT mode=stopped edges=");
                uart1_put_u32(heartbeat.edge_count);
                uart1_puts("\r\n");
            }
#endif
            continue;
        }

        /* 监测已启动：执行心跳超时和 AP_RESET 逻辑。 */
        heartbeat_status = heartbeat_monitor_status(
            &heartbeat, now_ms, reset_grace_ms(), HEARTBEAT_MONITOR_TIMEOUT_MS);
        reset_controller_update(&g_reset_controller, heartbeat_status, now_ms,
                                AP_RESET_PULSE_MS, RESET_RECOVERY_WINDOW_MS);
        apply_reset_output(reset_controller_output_active(&g_reset_controller));
        wdt_clear();
#if STC8G1K08_UART_LOG_ENABLE
        if ((heartbeat_monitor_ms_t)(now_ms - last_report) >= 1000UL) {
            last_report = now_ms;
            app_report(&heartbeat, heartbeat_status, now_ms);
        }
#endif
    }
}

void main(void)
{
    app_init();
    app_run();
}
