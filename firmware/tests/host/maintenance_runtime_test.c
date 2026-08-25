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

/* Emit a frame with independently selected protocol intervals. */
static maintenance_runtime_event_t emit_frame_with_timing(
    maintenance_runtime_t *mrt,
    maintenance_runtime_ms_t start,
    unsigned long break_ms,
    unsigned long sync_ms,
    unsigned long delimiter_ms,
    unsigned long payload_slot_ms,
    unsigned long trailer_ms,
    unsigned int pulses)
{
    unsigned int pulse;
    maintenance_runtime_ms_t edge;

    edge = start;
    maintenance_runtime_on_falling_edge(mrt, edge);
    edge = (maintenance_runtime_ms_t)(edge + break_ms);
    maintenance_runtime_on_falling_edge(mrt, edge);
    edge = (maintenance_runtime_ms_t)(edge + sync_ms);
    maintenance_runtime_on_falling_edge(mrt, edge);
    edge = (maintenance_runtime_ms_t)(edge + delimiter_ms);
    maintenance_runtime_on_falling_edge(mrt, edge);
    for (pulse = 0U; pulse < pulses; ++pulse) {
        edge = (maintenance_runtime_ms_t)(edge + payload_slot_ms);
        maintenance_runtime_on_falling_edge(mrt, edge);
    }
    edge = (maintenance_runtime_ms_t)(edge + trailer_ms);
    return maintenance_runtime_poll(mrt, edge);
}

static maintenance_runtime_event_t emit_timed_start(
    maintenance_runtime_t *mrt,
    unsigned long break_ms,
    unsigned long sync_ms,
    unsigned long delimiter_ms,
    unsigned long payload_slot_ms,
    unsigned long trailer_ms)
{
    return emit_frame_with_timing(mrt, 0UL, break_ms, sync_ms,
                                  delimiter_ms, payload_slot_ms,
                                  trailer_ms, 3U);
}

TEST(rejects_break_below_minimum)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);
    assert(emit_timed_start(&mrt, 749UL, 60UL, 240UL, 60UL, 500UL) ==
           MRT_EVENT_NONE);
}

TEST(accepts_break_at_minimum)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);
    assert(emit_timed_start(&mrt, 750UL, 60UL, 240UL, 60UL, 500UL) ==
           MRT_EVENT_START);
}

TEST(rejects_sync_outside_window)
{
    static const unsigned long values[] = {59UL, 151UL};
    unsigned int index;

    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        maintenance_runtime_t mrt;

        maintenance_runtime_init(&mrt, 0UL);
        assert(emit_timed_start(&mrt, 750UL, values[index], 240UL,
                                60UL, 500UL) == MRT_EVENT_NONE);
    }
}

TEST(accepts_sync_window_edges)
{
    static const unsigned long values[] = {60UL, 150UL};
    unsigned int index;

    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        maintenance_runtime_t mrt;

        maintenance_runtime_init(&mrt, 0UL);
        assert(emit_timed_start(&mrt, 750UL, values[index], 240UL,
                                60UL, 500UL) == MRT_EVENT_START);
    }
}

TEST(rejects_delimiter_outside_window)
{
    static const unsigned long values[] = {239UL, 361UL};
    unsigned int index;

    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        maintenance_runtime_t mrt;

        maintenance_runtime_init(&mrt, 0UL);
        assert(emit_timed_start(&mrt, 750UL, 60UL, values[index],
                                60UL, 500UL) == MRT_EVENT_NONE);
    }
}

TEST(accepts_delimiter_window_edges)
{
    static const unsigned long values[] = {240UL, 360UL};
    unsigned int index;

    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        maintenance_runtime_t mrt;

        maintenance_runtime_init(&mrt, 0UL);
        assert(emit_timed_start(&mrt, 750UL, 60UL, values[index],
                                60UL, 500UL) == MRT_EVENT_START);
    }
}

TEST(rejects_payload_slot_outside_window)
{
    static const unsigned long values[] = {59UL, 151UL};
    unsigned int index;

    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        maintenance_runtime_t mrt;

        maintenance_runtime_init(&mrt, 0UL);
        assert(emit_timed_start(&mrt, 750UL, 60UL, 240UL,
                                values[index], 500UL) == MRT_EVENT_NONE);
    }
}

TEST(accepts_payload_slot_window_edges)
{
    static const unsigned long values[] = {60UL, 150UL};
    unsigned int index;

    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        maintenance_runtime_t mrt;

        maintenance_runtime_init(&mrt, 0UL);
        assert(emit_timed_start(&mrt, 750UL, 60UL, 240UL,
                                values[index], 500UL) == MRT_EVENT_START);
    }
}

TEST(rejects_trailer_below_minimum)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);
    assert(emit_timed_start(&mrt, 750UL, 60UL, 240UL, 60UL, 499UL) ==
           MRT_EVENT_NONE);
}

TEST(accepts_trailer_at_minimum)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);
    assert(emit_timed_start(&mrt, 750UL, 60UL, 240UL, 60UL, 500UL) ==
           MRT_EVENT_START);
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
    RUN_TEST(rejects_break_below_minimum);
    RUN_TEST(accepts_break_at_minimum);
    RUN_TEST(rejects_sync_outside_window);
    RUN_TEST(accepts_sync_window_edges);
    RUN_TEST(rejects_delimiter_outside_window);
    RUN_TEST(accepts_delimiter_window_edges);
    RUN_TEST(rejects_payload_slot_outside_window);
    RUN_TEST(accepts_payload_slot_window_edges);
    RUN_TEST(rejects_trailer_below_minimum);
    RUN_TEST(accepts_trailer_at_minimum);
    printf("\n=== All Tests Passed ===\n");
    return 0;
}
