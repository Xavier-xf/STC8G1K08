#include <stdio.h>

#include "reset_controller.h"

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static int test_timeout_pulse_and_recovery(void)
{
    reset_controller_t controller;
    int ok = 1;

    reset_controller_init(&controller, 0UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_STARTUP,
                 "controller starts in startup");
    reset_controller_update(&controller, HEARTBEAT_MONITOR_STARTUP, 9999UL, 200UL);
    ok &= expect(!reset_controller_output_active(&controller),
                 "startup keeps reset output inactive");

    reset_controller_update(&controller, HEARTBEAT_MONITOR_HEALTHY, 10000UL, 200UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_MONITORING,
                 "healthy heartbeat enters monitoring");
    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 11000UL, 200UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_ASSERT,
                 "timeout starts reset assertion");
    ok &= expect(reset_controller_output_active(&controller),
                 "assert state drives reset output");

    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 11199UL, 200UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_ASSERT,
                 "reset remains asserted until pulse expires");
    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 11200UL, 200UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_WAIT_RECOVERY,
                 "pulse completion enters wait for recovery");
    ok &= expect(!reset_controller_output_active(&controller),
                 "wait for recovery releases reset output");

    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 15000UL, 200UL);
    ok &= expect(reset_controller_reset_count(&controller) == 1UL,
                 "timeout does not retrigger during wait for recovery");
    reset_controller_update(&controller, HEARTBEAT_MONITOR_HEALTHY, 15001UL, 200UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_MONITORING,
                 "healthy heartbeat exits wait for recovery");
    return ok;
}

static int test_second_timeout_and_rollover(void)
{
    reset_controller_t controller;
    int ok = 1;

    reset_controller_init(&controller, 0xfffffff0UL);
    reset_controller_update(&controller, HEARTBEAT_MONITOR_HEALTHY,
                            0xfffffff5UL, 10UL);
    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT,
                            0x00000005UL, 10UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_ASSERT,
                 "timeout transition works across millisecond rollover");
    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT,
                            0x0000000fUL, 10UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_WAIT_RECOVERY,
                 "pulse duration works across millisecond rollover");
    reset_controller_update(&controller, HEARTBEAT_MONITOR_HEALTHY,
                            0x00000010UL, 10UL);
    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT,
                            0x00000020UL, 10UL);
    ok &= expect(reset_controller_reset_count(&controller) == 2UL,
                 "recovered controller can assert a second reset");
    return ok;
}

int main(void)
{
    int ok = 1;
    ok &= test_timeout_pulse_and_recovery();
    ok &= test_second_timeout_and_rollover();
    if (!ok) return 1;
    puts("OK: reset controller state test passed");
    return 0;
}
