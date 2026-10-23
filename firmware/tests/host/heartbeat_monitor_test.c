#include <stdio.h>

#include "heartbeat_monitor.h"

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static int test_grace_and_healthy_state(void)
{
    heartbeat_monitor_t monitor;
    int ok = 1;

    heartbeat_monitor_init(&monitor, 0UL);
    ok &= expect(heartbeat_monitor_status(&monitor, 9999UL, 10000UL, 1000UL) ==
                     HEARTBEAT_MONITOR_STARTUP,
                 "startup grace remains active before 10 seconds");
    ok &= expect(heartbeat_monitor_status(&monitor, 10000UL, 10000UL, 1000UL) ==
                     HEARTBEAT_MONITOR_TIMEOUT,
                 "no edge is a timeout when startup grace ends");

    heartbeat_monitor_on_falling_edge(&monitor, 10100UL);
    ok &= expect(heartbeat_monitor_edge_count(&monitor) == 1UL,
                 "first falling edge increments edge count");
    ok &= expect(heartbeat_monitor_edge_age_ms(&monitor, 10300UL) == 200UL,
                 "edge age uses the monitor millisecond clock");
    ok &= expect(heartbeat_monitor_status(&monitor, 10300UL, 10000UL, 1000UL) ==
                     HEARTBEAT_MONITOR_HEALTHY,
                 "recent falling edge marks heartbeat healthy");
    return ok;
}

static int test_timeout_and_recovery(void)
{
    heartbeat_monitor_t monitor;
    int ok = 1;

    heartbeat_monitor_init(&monitor, 0UL);
    heartbeat_monitor_on_falling_edge(&monitor, 9900UL);
    ok &= expect(heartbeat_monitor_status(&monitor, 10900UL, 10000UL, 1000UL) ==
                     HEARTBEAT_MONITOR_TIMEOUT,
                 "edge age equal to one second is a timeout");

    heartbeat_monitor_on_falling_edge(&monitor, 10901UL);
    ok &= expect(heartbeat_monitor_status(&monitor, 10901UL, 10000UL, 1000UL) ==
                     HEARTBEAT_MONITOR_HEALTHY,
                 "new falling edge recovers from timeout");
    ok &= expect(heartbeat_monitor_edge_count(&monitor) == 2UL,
                 "recovery edge is counted");
    return ok;
}

static int test_zero_grace_never_reports_startup(void)
{
    heartbeat_monitor_t monitor;
    int ok = 1;

    /* Outside STARTUP the caller passes grace 0: a wrapped start age cannot hide a timeout. */
    heartbeat_monitor_init(&monitor, 0UL);
    ok &= expect(heartbeat_monitor_status(&monitor, 0UL, 0UL, 5000UL) ==
                     HEARTBEAT_MONITOR_TIMEOUT,
                 "zero grace without an edge is a timeout immediately");
    heartbeat_monitor_on_falling_edge(&monitor, 0xffffe000UL);
    ok &= expect(heartbeat_monitor_status(&monitor, 0x00000010UL, 0UL, 5000UL) ==
                     HEARTBEAT_MONITOR_TIMEOUT,
                 "zero grace keeps timeout after the start age wraps");
    ok &= expect(heartbeat_monitor_status(&monitor, 0x00000010UL, 30000UL, 5000UL) ==
                     HEARTBEAT_MONITOR_STARTUP,
                 "a constant grace would re-enter startup after the wrap");
    heartbeat_monitor_on_falling_edge(&monitor, 0x00000010UL);
    ok &= expect(heartbeat_monitor_status(&monitor, 0x00000020UL, 0UL, 5000UL) ==
                     HEARTBEAT_MONITOR_HEALTHY,
                 "zero grace still reports a recent edge as healthy");
    return ok;
}

static int test_millisecond_rollover(void)
{
    heartbeat_monitor_t monitor;
    int ok = 1;

    heartbeat_monitor_init(&monitor, 0xfffffff0UL);
    heartbeat_monitor_on_falling_edge(&monitor, 0xfffffffeUL);
    ok &= expect(heartbeat_monitor_edge_age_ms(&monitor, 0x00000020UL) == 34UL,
                 "edge age remains correct across unsigned rollover");
    ok &= expect(heartbeat_monitor_status(&monitor, 0x00000020UL, 10UL, 100UL) ==
                     HEARTBEAT_MONITOR_HEALTHY,
                 "recent edge remains healthy across unsigned rollover");
    ok &= expect(heartbeat_monitor_status(&monitor, 0x00000080UL, 10UL, 100UL) ==
                     HEARTBEAT_MONITOR_TIMEOUT,
                 "timeout remains correct across unsigned rollover");
    return ok;
}

int main(void)
{
    int ok = 1;

    ok &= test_grace_and_healthy_state();
    ok &= test_timeout_and_recovery();
    ok &= test_zero_grace_never_reports_startup();
    ok &= test_millisecond_rollover();
    if (!ok) return 1;
    puts("OK: heartbeat monitor state test passed");
    return 0;
}
