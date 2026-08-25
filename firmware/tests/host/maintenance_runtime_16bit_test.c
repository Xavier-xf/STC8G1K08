#include "maintenance_runtime.h"

#include <assert.h>
#include <stdio.h>

/* A normal heartbeat stream crossing the production 16-bit millisecond wrap. */
static void test_wrap_does_not_create_false_break(void)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, (maintenance_runtime_ms_t)65000U);
    maintenance_runtime_on_falling_edge(&mrt,
                                        (maintenance_runtime_ms_t)65000U);
    maintenance_runtime_on_falling_edge(&mrt,
                                        (maintenance_runtime_ms_t)100U);
    maintenance_runtime_on_falling_edge(&mrt,
                                        (maintenance_runtime_ms_t)230U);
    maintenance_runtime_on_falling_edge(&mrt,
                                        (maintenance_runtime_ms_t)510U);
    maintenance_runtime_on_falling_edge(&mrt,
                                        (maintenance_runtime_ms_t)650U);
    maintenance_runtime_on_falling_edge(&mrt,
                                        (maintenance_runtime_ms_t)770U);
    maintenance_runtime_on_falling_edge(&mrt,
                                        (maintenance_runtime_ms_t)890U);

    assert(maintenance_runtime_poll(&mrt,
                                    (maintenance_runtime_ms_t)1390U) ==
           MRT_EVENT_NONE);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
}

int main(void)
{
    printf("=== Maintenance Runtime 16-bit Wrap Test ===\n");
    test_wrap_does_not_create_false_break();
    printf("  PASSED\n");
    return 0;
}
