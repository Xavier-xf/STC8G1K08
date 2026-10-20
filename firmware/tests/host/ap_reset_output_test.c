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

void uart1_puts(const char *text)
{
    (void)text;
}

#include "ap_reset_output_under_test.inc"

static void expect_output(unsigned char active)
{
    assert((P5 & P5_AP_RESET_MASK) == (active ? P5_AP_RESET_MASK : 0U));
    assert((P5M1 & P5_AP_RESET_MASK) == 0U);
    assert((P5M0 & P5_AP_RESET_MASK) != 0U);
    assert(g_reset_output_active == active);
    /* CPU_CHECK must remain an input throughout. */
    assert((P5M1 & P5_CPU_CHECK_MASK) != 0U);
    assert((P5M0 & P5_CPU_CHECK_MASK) == 0U);
}

static void update_output(heartbeat_monitor_status_t status,
                          heartbeat_monitor_ms_t now_ms)
{
    reset_controller_update(&g_reset_controller, status, now_ms, AP_RESET_PULSE_MS);
    apply_reset_output(reset_controller_output_active(&g_reset_controller));
}

int main(void)
{
    unsigned int mode;
    unsigned char saved_p5, saved_m1, saved_m0;

    assert(AP_RESET_ASSERT_LEVEL == 1U);
    /* Initialize from every P5.5 mode, including POR/high-Z with latch high. */
    for (mode = 0U; mode < 4U; ++mode) {
        P5 = 0xEFU;
        P5M1 = (unsigned char)(0x85U | ((mode & 2U) ? P5_AP_RESET_MASK : 0U));
        P5M0 = (unsigned char)(0x4AU | ((mode & 1U) ? P5_AP_RESET_MASK : 0U));
        P3 = 0xFDU;
        P3M1 = 0x56U;
        P3M0 = 0xA9U;
        gpio_reset_init();
        expect_output(0U);
        assert(P5 == 0xDFU && P5M1 == 0x95U && P5M0 == 0x6AU);
        assert(P3 == 0xFFU && P3M1 == 0x55U && P3M0 == 0xAAU);
    }

    saved_p5 = (unsigned char)(P5 & (unsigned char)~P5_AP_RESET_MASK);
    saved_m1 = P5M1;
    saved_m0 = P5M0;
    reset_controller_init(&g_reset_controller, 0UL);
    update_output(HEARTBEAT_MONITOR_STARTUP, 0UL);
    expect_output(0U);
    update_output(HEARTBEAT_MONITOR_HEALTHY, 1000UL);
    expect_output(0U);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, 6000UL);
    expect_output(1U);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, 6199UL);
    expect_output(1U);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, 6200UL);
    expect_output(0U);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, 12000UL);
    expect_output(0U);
    assert(g_reset_controller.reset_count == 1UL);
    update_output(HEARTBEAT_MONITOR_HEALTHY, 12001UL);
    update_output(HEARTBEAT_MONITOR_TIMEOUT, 17001UL);
    expect_output(1U);
    assert(g_reset_controller.reset_count == 2UL);

    /* START/STOP use this helper: cancel a reset and keep power enabled. */
    EA = 1U;
    app_resume_normal(17002UL);
    expect_output(0U);
    assert(EA == 1U);
    assert(g_reset_controller.state == RESET_CONTROLLER_STARTUP);
    update_output(HEARTBEAT_MONITOR_STARTUP, 17003UL);
    expect_output(0U);
    apply_reset_output(1U);
    EA = 0U;
    app_resume_normal(18000UL);
    expect_output(0U);
    assert(EA == 0U);

    assert(P5M1 == saved_m1 && P5M0 == saved_m0);
    assert((P5 & (unsigned char)~P5_AP_RESET_MASK) == saved_p5);
    puts("OK: AP-RESET GPIO low-power-on/high-reset test passed");
    return 0;
}
