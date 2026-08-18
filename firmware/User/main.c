#include "Config.h"
#include "heartbeat_monitor.h"
#include "reset_controller.h"

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

static void uart1_init(void)
{
    unsigned long reload;

    P_SW1 &= (unsigned char)~0xC0;
    SCON = 0x40;
    TI = 0;
    reload = UART1_RELOAD;

    TR1 = 0;
    AUXR &= (unsigned char)~0x01;
    TMOD &= (unsigned char)~0x40;
    TMOD &= (unsigned char)~0x30;
    AUXR |= 0x40;
    TH1 = (unsigned char)(reload >> 8);
    TL1 = (unsigned char)reload;
    ET1 = 0;
    INT_CLKO &= (unsigned char)~0x02;
    TR1 = 1;
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

static void uart1_putc(char value)
{
    SBUF = value;
    while (!TI) {
    }
    TI = 0;
}

static void uart1_puts(const char *text)
{
    while (*text != '\0') {
        uart1_putc(*text++);
    }
}

static void uart1_put_u32(heartbeat_monitor_ms_t value)
{
    char digits[10];
    unsigned char count = 0;

    if (value == 0UL) {
        uart1_putc('0');
        return;
    }
    while (value != 0UL && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    }
    while (count != 0U) {
        uart1_putc(digits[--count]);
    }
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

static void apply_reset_output(unsigned char active)
{
    if (active == g_reset_output_active) return;
    if (active != 0U) {
        ap_reset_assert();
    } else {
        ap_reset_release();
    }
    g_reset_output_active = active;
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
    uart1_puts(reset_state_name(reset_controller_state(&g_reset_controller)));
    uart1_puts(" edges=");
    uart1_put_u32(heartbeat_monitor_edge_count(heartbeat));
    uart1_puts(" age_ms=");
    uart1_put_u32(heartbeat_monitor_edge_age_ms(heartbeat, now_ms));
    uart1_puts(" P54=");
    uart1_putc(CPU_CHECK ? '1' : '0');
    uart1_puts(" P55=");
    uart1_putc(AP_RESET_TEST ? '1' : '0');
    uart1_puts(" output=");
    uart1_puts(g_reset_output_active ? "assert" : "high-z");
    uart1_puts(" resets=");
    uart1_put_u32(reset_controller_reset_count(&g_reset_controller));
    uart1_puts(" wdt=on\r\n");
}

void timer0_isr(void) interrupt 1
{
    TH0 = (unsigned char)(TIMER0_RELOAD >> 8);
    TL0 = (unsigned char)TIMER0_RELOAD;
    ++g_millisecond;
}

void int2_isr(void) interrupt 10
{
    heartbeat_monitor_on_falling_edge(&g_heartbeat_monitor, g_millisecond);
}

static void app_init(void)
{
    gpio_reset_init();
    uart1_init();
    timer0_init();
    heartbeat_monitor_init(&g_heartbeat_monitor, 0UL);
    reset_controller_init(&g_reset_controller, 0UL);
    g_reset_output_active = 0U;
    INT_CLKO |= INT2_ENABLE_MASK;
    EA = 1;
    wdt_init();

    uart1_puts("\r\nSTC8G1K08 reset firmware v" STC8G1K08_FIRMWARE_VERSION "\r\n");
    uart1_puts("P54=CPU_CHECK INT2 falling edge\r\n");
    uart1_puts("P55=AP-RESET controlled pulse, WDT=ON\r\n");
    uart1_puts("grace=30000ms timeout=1000ms pulse=200ms wdt=128x\r\n");
}

static void app_run(void)
{
    heartbeat_monitor_ms_t last_report;
    heartbeat_monitor_ms_t now_ms;
    heartbeat_monitor_t heartbeat;
    heartbeat_monitor_status_t heartbeat_status;

    last_report = millis_snapshot();
    for (;;) {
        heartbeat_snapshot(&heartbeat, &now_ms);
        heartbeat_status = heartbeat_monitor_status(&heartbeat, now_ms,
                                                     RESET_HEARTBEAT_GRACE_MS,
                                                     HEARTBEAT_MONITOR_TIMEOUT_MS);
        reset_controller_update(&g_reset_controller, heartbeat_status, now_ms,
                                AP_RESET_PULSE_MS);
        apply_reset_output(reset_controller_output_active(&g_reset_controller));
        wdt_clear();

        if ((heartbeat_monitor_ms_t)(now_ms - last_report) >= 1000UL) {
            last_report = now_ms;
            app_report(&heartbeat, now_ms);
        }
    }
}

void main(void)
{
    app_init();
    app_run();
}
