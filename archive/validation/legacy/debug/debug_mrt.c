#include "maintenance_runtime.h"
#include <stdio.h>

int main(void) {
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;

    maintenance_runtime_init(&mrt, 0UL);
    printf("Initialized: state=0x%02X\n", mrt.state);

    /* BREAK: 900 ms */
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    printf("After edge at 0: state=0x%02X, decoder_state=%u, have_edge=%u\n",
           mrt.state, MRT_DECODER_STATE(mrt.state), MRT_HAVE_EDGE(mrt.state));

    maintenance_runtime_on_falling_edge(&mrt, 900UL);
    printf("After edge at 900 (BREAK): state=0x%02X, decoder_state=%u\n",
           mrt.state, MRT_DECODER_STATE(mrt.state));

    /* SYNC: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, 980UL);
    printf("After edge at 980 (SYNC): state=0x%02X, decoder_state=%u\n",
           mrt.state, MRT_DECODER_STATE(mrt.state));

    /* DELIMITER: 300 ms */
    maintenance_runtime_on_falling_edge(&mrt, 1280UL);
    printf("After edge at 1280 (DELIMITER): state=0x%02X, decoder_state=%u, payload=%u\n",
           mrt.state, MRT_DECODER_STATE(mrt.state), MRT_PAYLOAD_COUNT(mrt.state));

    /* PAYLOAD 1: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, 1360UL);
    printf("After edge at 1360 (PAYLOAD 1): state=0x%02X, payload=%u\n",
           mrt.state, MRT_PAYLOAD_COUNT(mrt.state));

    /* PAYLOAD 2: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, 1440UL);
    printf("After edge at 1440 (PAYLOAD 2): state=0x%02X, payload=%u\n",
           mrt.state, MRT_PAYLOAD_COUNT(mrt.state));

    /* PAYLOAD 3: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, 1520UL);
    printf("After edge at 1520 (PAYLOAD 3): state=0x%02X, payload=%u\n",
           mrt.state, MRT_PAYLOAD_COUNT(mrt.state));

    /* Poll before trailer */
    event = maintenance_runtime_poll(&mrt, 1600UL);
    printf("Poll at 1600: event=%u\n", event);

    /* Poll after trailer */
    event = maintenance_runtime_poll(&mrt, 2020UL);
    printf("Poll at 2020: event=%u (expected ENTER=1)\n", event);
    printf("Is active: %u\n", maintenance_runtime_is_active(&mrt));

    return 0;
}
