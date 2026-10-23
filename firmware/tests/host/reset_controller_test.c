#include <stdio.h>

#include "reset_controller.h"

#define PULSE_MS 200UL
#define RECOVERY_MS 120000UL

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static void update(reset_controller_t *controller, heartbeat_monitor_status_t status,
                   heartbeat_monitor_ms_t now_ms)
{
    reset_controller_update(controller, status, now_ms, PULSE_MS, RECOVERY_MS);
}

static int test_timeout_pulse_and_recovery(void)
{
    reset_controller_t controller;
    int ok = 1;

    reset_controller_init(&controller, 0UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_STARTUP,
                 "controller starts in startup");
    update(&controller, HEARTBEAT_MONITOR_STARTUP, 9999UL);
    ok &= expect(!reset_controller_output_active(&controller),
                 "startup keeps reset output inactive");

    update(&controller, HEARTBEAT_MONITOR_HEALTHY, 10000UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_MONITORING,
                 "healthy heartbeat enters monitoring");
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 11000UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_ASSERT,
                 "timeout starts reset assertion");
    ok &= expect(reset_controller_output_active(&controller),
                 "assert state drives reset output");

    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 11199UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_ASSERT,
                 "reset remains asserted until pulse expires");
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 11200UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_WAIT_RECOVERY,
                 "pulse completion enters wait for recovery");
    ok &= expect(!reset_controller_output_active(&controller),
                 "wait for recovery releases reset output");

    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 15000UL);
    ok &= expect(reset_controller_reset_count(&controller) == 1UL,
                 "timeout inside the recovery window does not retrigger");
    update(&controller, HEARTBEAT_MONITOR_HEALTHY, 15001UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_MONITORING,
                 "healthy heartbeat exits wait for recovery");
    return ok;
}

static int test_repeat_reset_until_recovery(void)
{
    reset_controller_t controller;
    heartbeat_monitor_ms_t released_ms = 11000UL + PULSE_MS;
    unsigned long cycle;
    int ok = 1;

    /* A host that never sends a heartbeat after START, nor after any reset. */
    reset_controller_init(&controller, 0UL);
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 11000UL);
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, released_ms);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_WAIT_RECOVERY &&
                 reset_controller_reset_count(&controller) == 1UL,
                 "startup timeout produces the first reset");
    for (cycle = 2UL; cycle <= 4UL; ++cycle) {
        update(&controller, HEARTBEAT_MONITOR_TIMEOUT, released_ms + RECOVERY_MS - 1UL);
        ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_WAIT_RECOVERY &&
                     reset_controller_reset_count(&controller) == cycle - 1UL,
                     "host gets the whole recovery window after each pulse");
        update(&controller, HEARTBEAT_MONITOR_TIMEOUT, released_ms + RECOVERY_MS);
        ok &= expect(reset_controller_output_active(&controller) &&
                     reset_controller_reset_count(&controller) == cycle,
                     "no recovery within the window asserts reset again");
        update(&controller, HEARTBEAT_MONITOR_TIMEOUT,
               released_ms + RECOVERY_MS + PULSE_MS - 1UL);
        ok &= expect(reset_controller_output_active(&controller),
                     "repeated reset keeps the configured pulse width");
        released_ms += RECOVERY_MS + PULSE_MS;
        update(&controller, HEARTBEAT_MONITOR_TIMEOUT, released_ms);
        ok &= expect(!reset_controller_output_active(&controller),
                     "repeated pulse releases into wait for recovery");
    }
    update(&controller, HEARTBEAT_MONITOR_HEALTHY, released_ms + 40000UL);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_MONITORING,
                 "heartbeat recovery ends repeated resets");
    update(&controller, HEARTBEAT_MONITOR_HEALTHY, released_ms + 10UL * RECOVERY_MS);
    ok &= expect(reset_controller_reset_count(&controller) == 4UL,
                 "monitoring never repeats a reset while healthy");
    return ok;
}

static int test_delayed_release_and_boundary_recovery(void)
{
    reset_controller_t controller;
    heartbeat_monitor_ms_t actual_release = 11000UL + PULSE_MS + 100UL;
    int ok = 1;

    reset_controller_init(&controller, 0UL);
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 11000UL);
    /* Main-loop latency extends the pulse; recovery starts when it is released. */
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, actual_release);
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, actual_release + RECOVERY_MS - 1UL);
    ok &= expect(!reset_controller_output_active(&controller) && controller.reset_count == 1UL,
                 "delayed release still gives the host a full recovery window");
    update(&controller, HEARTBEAT_MONITOR_HEALTHY, actual_release + RECOVERY_MS);
    ok &= expect(controller.state == RESET_CONTROLLER_MONITORING && controller.reset_count == 1UL,
                 "heartbeat at the recovery deadline takes precedence over reset");
    return ok;
}

static int test_second_timeout_and_rollover(void)
{
    reset_controller_t controller;
    int ok = 1;

    reset_controller_init(&controller, 0xfffffff0UL);
    update(&controller, HEARTBEAT_MONITOR_HEALTHY, 0xfffffff5UL);
    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 0x00000005UL, 10UL, RECOVERY_MS);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_ASSERT,
                 "timeout transition works across millisecond rollover");
    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 0x0000000fUL, 10UL, RECOVERY_MS);
    ok &= expect(reset_controller_state(&controller) == RESET_CONTROLLER_WAIT_RECOVERY,
                 "pulse duration works across millisecond rollover");
    reset_controller_update(&controller, HEARTBEAT_MONITOR_HEALTHY, 0x00000010UL, 10UL, RECOVERY_MS);
    reset_controller_update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 0x00000020UL, 10UL, RECOVERY_MS);
    ok &= expect(reset_controller_reset_count(&controller) == 2UL,
                 "recovered controller can assert a second reset");
    return ok;
}

static int test_recovery_window_across_rollover(void)
{
    reset_controller_t controller;
    heartbeat_monitor_ms_t released_ms = 0xffff0000UL + PULSE_MS;
    int ok = 1;

    reset_controller_init(&controller, 0xfffe0000UL);
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, 0xffff0000UL);
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT, released_ms);
    /* released_ms + RECOVERY_MS wraps the 32-bit millisecond counter. */
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT,
           (heartbeat_monitor_ms_t)(released_ms + RECOVERY_MS - 1UL));
    ok &= expect(reset_controller_reset_count(&controller) == 1UL,
                 "recovery window is not cut short by rollover");
    update(&controller, HEARTBEAT_MONITOR_TIMEOUT,
           (heartbeat_monitor_ms_t)(released_ms + RECOVERY_MS));
    ok &= expect(reset_controller_output_active(&controller) &&
                 reset_controller_reset_count(&controller) == 2UL,
                 "recovery window expires correctly across rollover");
    return ok;
}

int main(void)
{
    int ok = 1;
    ok &= test_timeout_pulse_and_recovery();
    ok &= test_repeat_reset_until_recovery();
    ok &= test_delayed_release_and_boundary_recovery();
    ok &= test_second_timeout_and_rollover();
    ok &= test_recovery_window_across_rollover();
    if (!ok) return 1;
    puts("OK: reset controller state test passed (repeat reset until recovery, rollover)");
    return 0;
}
