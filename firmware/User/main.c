#include "Config.h"
#include "heartbeat_monitor.h"
#include "reset_controller.h"
#include "maintenance_controller.h"
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
static unsigned char g_reset_output_active;
static maintenance_controller_t g_maintenance_controller;
#define COMMAND_LINE_BUFFER_SIZE 16U
static char g_command_line[COMMAND_LINE_BUFFER_SIZE];
static unsigned char g_command_length;
static unsigned char g_command_overflow;

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

static heartbeat_monitor_status_t heartbeat_status_snapshot(
    heartbeat_monitor_ms_t now_ms)
{
    heartbeat_monitor_status_t status;
    unsigned char saved_ea = EA;

    EA = 0;
    status = heartbeat_monitor_status(&g_heartbeat_monitor, now_ms,
                                      RESET_HEARTBEAT_GRACE_MS,
                                      HEARTBEAT_MONITOR_TIMEOUT_MS);
    EA = saved_ea;
    return status;
}

static unsigned long heartbeat_edge_count_snapshot(void)
{
    unsigned long edge_count;
    unsigned char saved_ea = EA;

    EA = 0;
    edge_count = heartbeat_monitor_edge_count(&g_heartbeat_monitor);
    EA = saved_ea;
    return edge_count;
}

static heartbeat_monitor_ms_t heartbeat_edge_age_snapshot(
    heartbeat_monitor_ms_t now_ms)
{
    heartbeat_monitor_ms_t edge_age_ms;
    unsigned char saved_ea = EA;

    EA = 0;
    edge_age_ms = heartbeat_monitor_edge_age_ms(&g_heartbeat_monitor, now_ms);
    EA = saved_ea;
    return edge_age_ms;
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


static void app_resume_normal(heartbeat_monitor_ms_t now_ms)
{
    heartbeat_monitor_init(&g_heartbeat_monitor, now_ms);
    reset_controller_init(&g_reset_controller, now_ms);
    g_reset_output_active = 0U;
    ap_reset_release();
}

static const char *maintenance_parse_error_name(maintenance_parse_result_t result)
{
    if (result == MAINTENANCE_PARSE_INVALID_PREFIX) return "prefix";
    if (result == MAINTENANCE_PARSE_UNKNOWN_COMMAND) return "command";
    if (result == MAINTENANCE_PARSE_INVALID_ARGUMENT) return "argument";
    return "empty";
}

static const char *maintenance_controller_error_name(
    maintenance_controller_error_t error)
{
    if (error == MAINTENANCE_ERROR_LEASE_RANGE) return "lease-range";
    if (error == MAINTENANCE_ERROR_NOT_ACTIVE) return "not-active";
    return "command";
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

static void app_report(heartbeat_monitor_ms_t now_ms)
{
    uart1_puts("MNT STATUS mode=");
    if (maintenance_controller_mode(&g_maintenance_controller) ==
        MAINTENANCE_MODE_MAINTENANCE) {
        uart1_puts("maintenance lease_ms=");
        uart1_put_u32(maintenance_controller_remaining_ms(
            &g_maintenance_controller, now_ms));
        uart1_puts(" heartbeat=paused state=maintenance");
    } else {
        uart1_puts("normal lease_ms=0 heartbeat=");
        uart1_puts(heartbeat_status_name(heartbeat_status_snapshot(now_ms)));
        uart1_puts(" state=");
        uart1_puts(reset_state_name(reset_controller_state(
            &g_reset_controller)));
    }
    uart1_puts(" edges=");
    uart1_put_u32(heartbeat_edge_count_snapshot());
    uart1_puts(" age_ms=");
    uart1_put_u32(heartbeat_edge_age_snapshot(now_ms));
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

static void app_process_command(const char *line)
{
    maintenance_command_t command;
    maintenance_controller_error_t error;
    maintenance_parse_result_t parse_result;
    maintenance_controller_action_t action;
    heartbeat_monitor_ms_t now_ms;

    parse_result = maintenance_controller_parse_line(line, &command);
    if (parse_result != MAINTENANCE_PARSE_OK) {
        uart1_puts("MNT NACK reason=");
        uart1_puts(maintenance_parse_error_name(parse_result));
        uart1_puts("\r\n");
        return;
    }
    now_ms = millis_snapshot();
    action = maintenance_controller_execute(
        &g_maintenance_controller, &command, now_ms, &error);
    if (action == MAINTENANCE_ACTION_REJECTED) {
        uart1_puts("MNT NACK reason=");
        uart1_puts(maintenance_controller_error_name(error));
        uart1_puts("\r\n");
        return;
    }
    if (action == MAINTENANCE_ACTION_STATUS) {
        app_report(now_ms);
        return;
    }
    if (action == MAINTENANCE_ACTION_ENTERED ||
        action == MAINTENANCE_ACTION_RENEWED) {
        apply_reset_output(0U);
        uart1_puts("MNT ACK mode=maintenance lease_s=");
        uart1_put_u32(maintenance_controller_remaining_ms(
            &g_maintenance_controller, now_ms) / 1000UL);
        uart1_puts("\r\n");
        return;
    }
    if (action == MAINTENANCE_ACTION_RESUMED) {
        app_resume_normal(now_ms);
        uart1_puts("MNT ACK mode=resume\r\n");
    }
}

static void app_poll_commands(void)
{
    unsigned char value;

    if (uart1_rx_overflow_take() != 0U) {
        g_command_length = 0U;
        g_command_overflow = 0U;
        uart1_puts("MNT NACK reason=rx-overflow\r\n");
    }
    while (uart1_read_byte(&value) != 0U) {
        if (value == '\r' || value == '\n') {
            if (g_command_overflow != 0U) {
                uart1_puts("MNT NACK reason=line-too-long\r\n");
            } else if (g_command_length != 0U) {
                g_command_line[g_command_length] = '\0';
                app_process_command(g_command_line);
            }
            g_command_length = 0U;
            g_command_overflow = 0U;
        } else if (g_command_overflow == 0U) {
            if (g_command_length < (COMMAND_LINE_BUFFER_SIZE - 1U)) {
                g_command_line[g_command_length++] = (char)value;
            } else {
                g_command_overflow = 1U;
            }
        }
    }
}

static void app_init(void)
{
    gpio_reset_init();
    uart1_init();
    timer0_init();
    heartbeat_monitor_init(&g_heartbeat_monitor, 0UL);
    reset_controller_init(&g_reset_controller, 0UL);
    maintenance_controller_init(&g_maintenance_controller, 0UL);
    g_reset_output_active = 0U;
    g_command_length = 0U;
    g_command_overflow = 0U;
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
    heartbeat_monitor_ms_t now_ms;
    heartbeat_monitor_status_t heartbeat_status;

    for (;;) {
        now_ms = millis_snapshot();
        if (maintenance_controller_update(&g_maintenance_controller, now_ms) != 0U) {
            app_resume_normal(now_ms);
            uart1_puts("MNT ACK mode=resume reason=lease-expired\r\n");
        }
        app_poll_commands();
        now_ms = millis_snapshot();
        if (maintenance_controller_mode(&g_maintenance_controller) ==
            MAINTENANCE_MODE_MAINTENANCE) {
            apply_reset_output(0U);
        } else {
            heartbeat_status = heartbeat_status_snapshot(now_ms);
            reset_controller_update(&g_reset_controller, heartbeat_status,
                                    now_ms, AP_RESET_PULSE_MS);
            apply_reset_output(
                reset_controller_output_active(&g_reset_controller));
        }
        wdt_clear();
    }
}
void main(void)
{
    app_init();
    app_run();
}
