#include "Config.h"
#include "heartbeat_monitor.h"

/*
 * STC8G1K08 CPU_CHECK 监控固件
 *
 * Pin1/P5.4 通过 INT2 记录 CPU_CHECK 的下降沿。
 * Pin3/P5.5 始终保持输入高阻；本固件不驱动 Q23、AP-RESET 或内部 WDT。
 */

static volatile heartbeat_monitor_ms_t g_millisecond;
static heartbeat_monitor_t g_heartbeat_monitor;

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

static void gpio_monitor_init(void)
{
    /* P5.4/CPU_CHECK 和 P5.5/AP-RESET 均为输入高阻。 */
    P5M1 |= 0x30;
    P5M0 &= (unsigned char)~0x30;
    P5 |= 0x30;

    P3M1 |= 0x01;
    P3M0 &= (unsigned char)~0x01;
    P3M1 &= (unsigned char)~0x02;
    P3M0 |= 0x02;
    P3 |= 0x02;
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

static void report_status(void)
{
    heartbeat_monitor_t snapshot;
    heartbeat_monitor_ms_t now_ms;
    heartbeat_monitor_status_t status;

    heartbeat_snapshot(&snapshot, &now_ms);
    status = heartbeat_monitor_status(&snapshot, now_ms,
                                      HEARTBEAT_MONITOR_GRACE_MS,
                                      HEARTBEAT_MONITOR_TIMEOUT_MS);
    uart1_puts("monitor state=");
    uart1_puts(heartbeat_status_name(status));
    uart1_puts(" edges=");
    uart1_put_u32(heartbeat_monitor_edge_count(&snapshot));
    uart1_puts(" age_ms=");
    uart1_put_u32(heartbeat_monitor_edge_age_ms(&snapshot, now_ms));
    uart1_puts(" P54=");
    uart1_putc(CPU_CHECK ? '1' : '0');
    uart1_puts(" P55=");
    uart1_putc(AP_RESET_TEST ? '1' : '0');
    uart1_puts(" output=high-z\r\n");
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

void main(void)
{
    heartbeat_monitor_ms_t last_report;
    heartbeat_monitor_ms_t now_ms;

    gpio_monitor_init();
    uart1_init();
    timer0_init();
    heartbeat_monitor_init(&g_heartbeat_monitor, 0UL);
    INT_CLKO |= INT2_ENABLE_MASK;
    EA = 1;

    uart1_puts("\r\nSTC8G1K08 monitor firmware\r\n");
    uart1_puts("P54=CPU_CHECK INT2 falling edge\r\n");
    uart1_puts("P55=AP-RESET high-z, WDT=OFF, output-drive=OFF\r\n");
    uart1_puts("grace=10000ms timeout=1000ms\r\n");

    last_report = millis_snapshot();
    for (;;) {
        now_ms = millis_snapshot();

        if ((heartbeat_monitor_ms_t)(now_ms - last_report) >= 1000UL) {
            last_report = now_ms;
            report_status();
        }
    }
}
