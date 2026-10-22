#include "maintenance_runtime.h"
#include <stdio.h>

int main(void) {
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;

    maintenance_runtime_init(&mrt, 0UL);

    /* 先进入维护模式 */
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);
    maintenance_runtime_on_falling_edge(&mrt, 980UL);
    maintenance_runtime_on_falling_edge(&mrt, 1280UL);
    maintenance_runtime_on_falling_edge(&mrt, 1360UL);
    maintenance_runtime_on_falling_edge(&mrt, 1440UL);
    maintenance_runtime_on_falling_edge(&mrt, 1520UL);
    event = maintenance_runtime_poll(&mrt, 2020UL);
    printf("After ENTER: event=%u, state=0x%02X, last_edge=%lu\n", 
           event, mrt.state, (unsigned long)mrt.last_edge_ms);
    printf("  have_edge=%u, decoder_state=%u\n",
           MRT_HAVE_EDGE(mrt.state), MRT_DECODER_STATE(mrt.state));

    /* 第一个 EXIT 边沿 */
    printf("\nFirst EXIT edge at 3000:\n");
    printf("  Before: last_edge=%lu, have_edge=%u\n", 
           (unsigned long)mrt.last_edge_ms, MRT_HAVE_EDGE(mrt.state));
    maintenance_runtime_on_falling_edge(&mrt, 3000UL);
    printf("  After: last_edge=%lu, have_edge=%u, decoder_state=%u\n", 
           (unsigned long)mrt.last_edge_ms, MRT_HAVE_EDGE(mrt.state),
           MRT_DECODER_STATE(mrt.state));

    /* 第二个 EXIT 边沿 (BREAK) */
    printf("\nSecond EXIT edge at 3900 (interval=900):\n");
    printf("  Before: last_edge=%lu, interval=%lu\n", 
           (unsigned long)mrt.last_edge_ms, 3900UL - mrt.last_edge_ms);
    maintenance_runtime_on_falling_edge(&mrt, 3900UL);
    printf("  After: last_edge=%lu, decoder_state=%u\n", 
           (unsigned long)mrt.last_edge_ms, MRT_DECODER_STATE(mrt.state));

    return 0;
}
