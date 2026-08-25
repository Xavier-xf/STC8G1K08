#include "maintenance_runtime.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running %s...\n", #name); \
    test_##name(); \
    printf("  PASSED\n"); \
} while (0)

/* Emit one P5.4 frame using the production timing windows. */
static maintenance_runtime_event_t emit_frame(maintenance_runtime_t *mrt,
                                              maintenance_runtime_ms_t start,
                                              unsigned int pulses)
{
    unsigned int pulse;

    maintenance_runtime_on_falling_edge(mrt, start);
    maintenance_runtime_on_falling_edge(mrt, start + 900UL);
    maintenance_runtime_on_falling_edge(mrt, start + 1030UL);
    maintenance_runtime_on_falling_edge(mrt, start + 1310UL);
    for (pulse = 0U; pulse < pulses; ++pulse) {
        maintenance_runtime_on_falling_edge(
            mrt, start + 1450UL + pulse * 120UL);
    }
    return maintenance_runtime_poll(
        mrt, start + 1450UL + pulses * 120UL + 500UL);
}

TEST(cold_start_disabled)
{
    maintenance_runtime_t mrt;

    memset(&mrt, 0xFF, sizeof(mrt));
    maintenance_runtime_init(&mrt, 1000UL);
    assert(maintenance_runtime_mode(&mrt) == MRT_MODE_NORMAL);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
}

TEST(start_three_pulses_enables_monitor)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;

    maintenance_runtime_init(&mrt, 0UL);
    event = emit_frame(&mrt, 0UL, 3U);
    assert(event == MRT_EVENT_START);
    assert(maintenance_runtime_is_active(&mrt) == 1U);
    assert(maintenance_runtime_mode(&mrt) == MRT_MODE_MAINTENANCE);
}

TEST(start_is_idempotent)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);
    assert(emit_frame(&mrt, 0UL, 3U) == MRT_EVENT_START);
    assert(emit_frame(&mrt, 3000UL, 3U) == MRT_EVENT_START);
    assert(maintenance_runtime_is_active(&mrt) == 1U);
}

TEST(stop_four_pulses_disables_monitor)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);
    assert(emit_frame(&mrt, 0UL, 3U) == MRT_EVENT_START);
    assert(emit_frame(&mrt, 3000UL, 4U) == MRT_EVENT_STOP);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
    assert(maintenance_runtime_mode(&mrt) == MRT_MODE_NORMAL);
}

TEST(stop_is_idempotent)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);
    assert(emit_frame(&mrt, 0UL, 4U) == MRT_EVENT_STOP);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
    assert(emit_frame(&mrt, 3000UL, 4U) == MRT_EVENT_STOP);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
}

TEST(stop_is_permanent_and_has_no_lease_expiry)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);
    assert(emit_frame(&mrt, 0UL, 3U) == MRT_EVENT_START);
    assert(emit_frame(&mrt, 3000UL, 4U) == MRT_EVENT_STOP);

    /* A stopped monitor must remain stopped well past the old 30-minute lease. */
    assert(maintenance_runtime_poll(&mrt, 1803000UL) == MRT_EVENT_NONE);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
}

TEST(normal_heartbeat_is_ignored)
{
    maintenance_runtime_t mrt;
    unsigned int index;

    maintenance_runtime_init(&mrt, 0UL);
    for (index = 0U; index < 100U; ++index) {
        maintenance_runtime_on_falling_edge(&mrt, index * 200UL);
        assert(maintenance_runtime_poll(&mrt, index * 200UL + 50UL) ==
               MRT_EVENT_NONE);
        assert(maintenance_runtime_is_active(&mrt) == 0U);
    }
}

int main(void)
{
    printf("=== Maintenance Runtime START/STOP Test Suite ===\n\n");
    RUN_TEST(cold_start_disabled);
    RUN_TEST(start_three_pulses_enables_monitor);
    RUN_TEST(start_is_idempotent);
    RUN_TEST(stop_four_pulses_disables_monitor);
    RUN_TEST(stop_is_idempotent);
    RUN_TEST(stop_is_permanent_and_has_no_lease_expiry);
    RUN_TEST(normal_heartbeat_is_ignored);
    printf("\n=== All Tests Passed ===\n");
    return 0;
}
