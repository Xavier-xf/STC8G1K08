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
    printf("After ENTER: event=%u, last_edge=%lu\n", event, (unsigned long)mrt.last_edge_ms);

    /* EXIT 命令 */
    unsigned long exit_start = 3000UL;
    printf("\nEXIT command starting at %lu:\n", exit_start);
    printf("  Interval from last_edge: %lu ms\n", exit_start - mrt.last_edge_ms);
    
    maintenance_runtime_on_falling_edge(&mrt, exit_start);
    printf("After edge at %lu: decoder_state=%u\n", exit_start, MRT_DECODER_STATE(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 900UL);
    printf("After edge at %lu (BREAK): decoder_state=%u\n", exit_start + 900UL, MRT_DECODER_STATE(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 980UL);
    printf("After edge at %lu (SYNC): decoder_state=%u\n", exit_start + 980UL, MRT_DECODER_STATE(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1280UL);
    printf("After edge at %lu (DELIMITER): decoder_state=%u, payload=%u\n", 
           exit_start + 1280UL, MRT_DECODER_STATE(mrt.state), MRT_PAYLOAD_COUNT(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1360UL);
    printf("After edge at %lu (PAYLOAD 1): payload=%u\n", exit_start + 1360UL, MRT_PAYLOAD_COUNT(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1440UL);
    printf("After edge at %lu (PAYLOAD 2): payload=%u\n", exit_start + 1440UL, MRT_PAYLOAD_COUNT(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1520UL);
    printf("After edge at %lu (PAYLOAD 3): payload=%u\n", exit_start + 1520UL, MRT_PAYLOAD_COUNT(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1600UL);
    printf("After edge at %lu (PAYLOAD 4): payload=%u\n", exit_start + 1600UL, MRT_PAYLOAD_COUNT(mrt.state));

    event = maintenance_runtime_poll(&mrt, exit_start + 2100UL);
    printf("\nPoll at %lu: event=%u (expected EXIT=2)\n", exit_start + 2100UL, event);

    return 0;
}
