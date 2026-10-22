#include "maintenance_runtime.h"
#include <stdio.h>

int main(void) {
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;

    maintenance_runtime_init(&mrt, 0UL);

    /* 先进入维护模式 (3 个载荷脉冲) */
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);
    maintenance_runtime_on_falling_edge(&mrt, 980UL);
    maintenance_runtime_on_falling_edge(&mrt, 1280UL);
    maintenance_runtime_on_falling_edge(&mrt, 1360UL);
    maintenance_runtime_on_falling_edge(&mrt, 1440UL);
    maintenance_runtime_on_falling_edge(&mrt, 1520UL);
    event = maintenance_runtime_poll(&mrt, 2020UL);
    printf("ENTER result: event=%u (expected 1), active=%u\n", 
           event, maintenance_runtime_is_active(&mrt));

    /* 发送 EXIT (4 个载荷脉冲) */
    printf("\nSending EXIT command...\n");
    maintenance_runtime_on_falling_edge(&mrt, 3000UL);
    printf("Edge at 3000, state=0x%02X\n", mrt.state);
    
    maintenance_runtime_on_falling_edge(&mrt, 3900UL);  /* BREAK */
    printf("Edge at 3900 (BREAK), decoder_state=%u\n", MRT_DECODER_STATE(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, 3980UL);  /* SYNC */
    printf("Edge at 3980 (SYNC), decoder_state=%u\n", MRT_DECODER_STATE(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, 4280UL);  /* DELIMITER */
    printf("Edge at 4280 (DELIMITER), decoder_state=%u, payload=%u\n", 
           MRT_DECODER_STATE(mrt.state), MRT_PAYLOAD_COUNT(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, 4360UL);  /* PAYLOAD 1 */
    printf("Edge at 4360 (PAYLOAD 1), payload=%u\n", MRT_PAYLOAD_COUNT(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, 4440UL);  /* PAYLOAD 2 */
    printf("Edge at 4440 (PAYLOAD 2), payload=%u\n", MRT_PAYLOAD_COUNT(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, 4520UL);  /* PAYLOAD 3 */
    printf("Edge at 4520 (PAYLOAD 3), payload=%u\n", MRT_PAYLOAD_COUNT(mrt.state));
    
    maintenance_runtime_on_falling_edge(&mrt, 4600UL);  /* PAYLOAD 4 */
    printf("Edge at 4600 (PAYLOAD 4), payload=%u\n", MRT_PAYLOAD_COUNT(mrt.state));

    event = maintenance_runtime_poll(&mrt, 5100UL);
    printf("\nEXIT result: event=%u (expected 2), active=%u\n", 
           event, maintenance_runtime_is_active(&mrt));

    return 0;
}
