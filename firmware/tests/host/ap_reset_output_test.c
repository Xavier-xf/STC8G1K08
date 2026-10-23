#include <assert.h>
#include <stdio.h>

#include "Config.h"
#include "heartbeat_monitor.h"
#include "reset_controller.h"
#include "uart.h"

/* Fake SFRs; functions below are extracted unchanged from User/main.c. */
static unsigned char P5, P5M1, P5M0, P3, P3M1, P3M0, EA;
static heartbeat_monitor_t g_heartbeat_monitor;
static reset_controller_t g_reset_controller;
static unsigned char g_reset_output_active;
static unsigned int uart_lines;
static unsigned int uart_chars;

/* P5.4/CPU_CHECK belongs to cpu_check_link; main.c must never touch it. */
#define CPU_CHECK_LINK_MASK 0x10U
#define CPU_CHECK (P5 & CPU_CHECK_LINK_MASK)
#define AP_RESET (P5 & 0x20U)

void uart1_puts(const char *text)
{
    while (*text++) ++uart_chars;
    ++uart_lines;
}

void uart1_putc(char value)
{
    (void)value;
    ++uart_chars;
}

void uart1_put_u32(unsigned long value)
{
    char digits[11];
    int length = snprintf(digits, sizeof(digits), "%lu", value);
    assert(length > 0 && length <= 10);
    uart_chars += (unsigned int)length;
}

#include "ap_reset_output_under_test.inc"

static void expect_output(unsigned char active)
{
    assert((P5 & P5_AP_RESET_MASK) == (active ? P5_AP_RESET_MASK : 0U));
    assert((P5M1 & P5_AP_RESET_MASK) == 0U);
    assert((P5M0 & P5_AP_RESET_MASK) != 0U);
    assert(g_reset_output_active == active);
}

static void update_output(heartbeat_monitor_status_t status,
                          heartbeat_monitor_ms_t now_ms)
{
    reset_controller_update(&g_reset_controller, status, now_ms,
                            AP_RESET_PULSE_MS, RESET_RECOVERY_WINDOW_MS);
    apply_reset_output(reset_controller_output_active(&g_reset_controller));
}

static void test_board_init(void)
{
    unsigned int mode;

    assert(AP_RESET_ASSERT_LEVEL == 1U);
    /* Initialize from every P5.5 mode, including POR/high-Z with latch high. */
    for (mode = 0U; mode < 4U; ++mode) {
        P5 = 0xEFU;
        P5M1 = (unsigned char)(0x85U | ((mode & 2U) ? P5_AP_RESET_MASK : 0U));
        P5M0 = (unsigned char)(0x4AU | ((mode & 1U) ? P5_AP_RESET_MASK : 0U));
        P3 = 0xFDU;
        P3M1 = 0x56U;
        P3M0 = 0xA9U;
        g_reset_output_active = 0U;
        gpio_board_init();
        expect_output(0U);
        /* Only P5.5 changes on port 5; P5.4 keeps whatever the link set. */
        assert((P5 & CPU_CHECK_LINK_MASK) == (0xEFU & CPU_CHECK_LINK_MASK));
        assert((P5M1 & CPU_CHECK_LINK_MASK) == (0x85U & CPU_CHECK_LINK_MASK));
        assert((P5M0 & CPU_CHECK_LINK_MASK) == (0x4AU & CPU_CHECK_LINK_MASK));
        assert(P5 == 0xCFU && P5M1 == 0x85U && P5M0 == 0x6AU);
        assert(P3 == 0xFFU && P3M1 == 0x55U && P3M0 == 0xAAU);
    }
}

static void test_reset_sequence(void)
{
    unsigned char saved_p5 = (unsigned char)(P5 & (unsigned char)~P5_AP_RESET_MASK);
    unsigned char saved_m1 = P5M1;
    unsigned char saved_m0 = P5M0;
    heartbeat_monitor_ms_t released;

    reset_controller_init(&g_reset_controller, 0UL);
    update_output(HEARTBEAT_MONITOR_STARTUP, 0UL);
    expect_output(0U);
    update_output(HEARTBEAT_MONITOR_HEALTHY, 1000UL);
    expect_output(0U);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, 6000UL);
    expect_output(1U);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, 6199UL);
    expect_output(1U);
    released = 6000UL + AP_RESET_PULSE_MS;
    update_output(HEARTBEAT_MONITOR_TIMEOUT, released);
    expect_output(0U);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, released + RESET_RECOVERY_WINDOW_MS - 1UL);
    expect_output(0U);
    assert(g_reset_controller.reset_count == 1UL);
    /* The host never recovered: the configured window repeats the reset. */
    update_output(HEARTBEAT_MONITOR_TIMEOUT, released + RESET_RECOVERY_WINDOW_MS);
    expect_output(1U);
    assert(g_reset_controller.reset_count == 2UL);
    released += RESET_RECOVERY_WINDOW_MS + AP_RESET_PULSE_MS;
    update_output(HEARTBEAT_MONITOR_TIMEOUT, released);
    expect_output(0U);
    update_output(HEARTBEAT_MONITOR_HEALTHY, released + 1UL);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, released + 5001UL);
    expect_output(1U);
    assert(g_reset_controller.reset_count == 3UL);

    /* STOP is the only runtime release outside the controller; it logs once. */
    uart_lines = 0U;
    apply_reset_output(0U);
    expect_output(0U);
    apply_reset_output(0U);
    assert(uart_lines == 1U);

    assert(P5M1 == saved_m1 && P5M0 == saved_m0);
    assert((P5 & (unsigned char)~P5_AP_RESET_MASK) == saved_p5);
}

static void test_restart_and_grace(void)
{
    heartbeat_monitor_ms_t start = 17002UL;
    heartbeat_monitor_ms_t wrapped;

    /* START from stopped: restore EA and restart the startup grace. */
    EA = 1U;
    monitor_restart(start);
    assert(EA == 1U && g_reset_controller.state == RESET_CONTROLLER_STARTUP);
    assert(g_heartbeat_monitor.started_at_ms == start && !g_heartbeat_monitor.edge_seen);
    EA = 0U;
    monitor_restart(start);
    assert(EA == 0U);
    assert(reset_grace_ms() == RESET_HEARTBEAT_GRACE_MS);
    assert(heartbeat_monitor_status(&g_heartbeat_monitor, start + 1000UL, reset_grace_ms(),
                                    HEARTBEAT_MONITOR_TIMEOUT_MS) == HEARTBEAT_MONITOR_STARTUP);

    /* About 49.7 days later the 32-bit START age wraps back below the grace. */
    g_reset_controller.state = RESET_CONTROLLER_MONITORING;
    wrapped = start + 10000UL;
    heartbeat_monitor_on_falling_edge(&g_heartbeat_monitor, wrapped - 6000UL);
    assert(reset_grace_ms() == 0UL);
    assert(heartbeat_monitor_status(&g_heartbeat_monitor, wrapped, reset_grace_ms(),
                                    HEARTBEAT_MONITOR_TIMEOUT_MS) == HEARTBEAT_MONITOR_TIMEOUT);
    /* The constant grace would have hidden that dead host for up to 30 s. */
    assert(heartbeat_monitor_status(&g_heartbeat_monitor, wrapped, RESET_HEARTBEAT_GRACE_MS,
                                    HEARTBEAT_MONITOR_TIMEOUT_MS) == HEARTBEAT_MONITOR_STARTUP);
    g_reset_controller.state = RESET_CONTROLLER_WAIT_RECOVERY;
    assert(reset_grace_ms() == 0UL);
}

static void test_uart_report_budget(void)
{
    heartbeat_monitor_t heartbeat;
    unsigned int phase, status, longest = 0U;

    heartbeat_monitor_init(&heartbeat, 0UL);
    heartbeat.last_edge_ms = 0UL;
    heartbeat.edge_count = 0xffffffffUL;
    heartbeat.edge_seen = 1U;
    g_reset_controller.reset_count = 0xffffffffUL;
    for (phase = RESET_CONTROLLER_STARTUP; phase <= RESET_CONTROLLER_WAIT_RECOVERY; ++phase) {
        g_reset_controller.state = (reset_controller_state_t)phase;
        for (status = HEARTBEAT_MONITOR_STARTUP; status <= HEARTBEAT_MONITOR_TIMEOUT; ++status) {
            uart_chars = 0U;
            g_reset_output_active = 0U;
            app_report(&heartbeat, (heartbeat_monitor_status_t)status, 0xffffffffUL);
            if (uart_chars > longest) longest = uart_chars;
        }
    }
    assert(longest == 140U);
    /* 140 chars + 18-char release log: <165 ms on the wire at 9600/8N1.
     * This excludes arithmetic and interrupt execution time, which need board checks. */
    assert((longest + 18U) * 10UL * 1000UL < 165UL * UART_BAUDRATE);
    printf("OK: actual UART report <=%u chars; report + release wire time <165 ms\n", longest);
}

int main(void)
{
    test_board_init();
    test_reset_sequence();
    test_restart_and_grace();
    test_uart_report_budget();
    puts("OK: AP-RESET GPIO, repeat reset, single P5.5 owner and STARTUP-only grace test passed");
    return 0;
}
