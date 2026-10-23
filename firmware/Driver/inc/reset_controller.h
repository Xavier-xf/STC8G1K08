#ifndef STC8G1K08_RESET_CONTROLLER_H
#define STC8G1K08_RESET_CONTROLLER_H

#include "heartbeat_monitor.h"

/*
 * 复位决策状态机，不操作 GPIO：
 * STARTUP -> MONITORING（心跳正常）或 ASSERT（宽限结束仍无心跳）；
 * MONITORING -> ASSERT（心跳超时）；
 * ASSERT -> WAIT_RECOVERY（脉冲结束）；
 * WAIT_RECOVERY -> MONITORING（心跳恢复）或 ASSERT（恢复窗口内未恢复，重复复位）。
 * 枚举顺序 +1 即应答状态中的阶段编号，不得调整。
 */
typedef enum {
    RESET_CONTROLLER_STARTUP = 0,
    RESET_CONTROLLER_MONITORING,
    RESET_CONTROLLER_ASSERT,
    RESET_CONTROLLER_WAIT_RECOVERY,
} reset_controller_state_t;

typedef struct {
    reset_controller_state_t state;
    /* ASSERT 为脉冲起点，WAIT_RECOVERY 为释放起点，复用一个时间戳。 */
    heartbeat_monitor_ms_t state_started_ms;
    unsigned long reset_count;
} reset_controller_t;

void reset_controller_init(reset_controller_t *controller,
                            heartbeat_monitor_ms_t now_ms);
/* recovery_ms 从脉冲结束开始计时，应不小于主机最坏开机到恢复心跳时间。 */
void reset_controller_update(reset_controller_t *controller,
                             heartbeat_monitor_status_t heartbeat_status,
                             heartbeat_monitor_ms_t now_ms,
                             heartbeat_monitor_ms_t pulse_ms,
                             heartbeat_monitor_ms_t recovery_ms);
unsigned char reset_controller_output_active(const reset_controller_t *controller);
#ifdef RESET_CONTROLLER_HOST_TEST
reset_controller_state_t reset_controller_state(const reset_controller_t *controller);
unsigned long reset_controller_reset_count(const reset_controller_t *controller);
#endif

#endif
