#include "reset_controller.h"

static heartbeat_monitor_ms_t reset_controller_elapsed(heartbeat_monitor_ms_t now_ms,
                                                        heartbeat_monitor_ms_t then_ms)
{
    return now_ms - then_ms;
}

static void reset_controller_begin_assert(reset_controller_t *controller,
                                          heartbeat_monitor_ms_t now_ms)
{
    controller->state = RESET_CONTROLLER_ASSERT;
    controller->state_started_ms = now_ms;
    ++controller->reset_count;
}

void reset_controller_init(reset_controller_t *controller,
                            heartbeat_monitor_ms_t now_ms)
{
    if (controller == 0) return;
    controller->state = RESET_CONTROLLER_STARTUP;
    controller->state_started_ms = now_ms;
    controller->reset_count = 0UL;
}

void reset_controller_update(reset_controller_t *controller,
                             heartbeat_monitor_status_t heartbeat_status,
                             heartbeat_monitor_ms_t now_ms,
                             heartbeat_monitor_ms_t pulse_ms,
                             heartbeat_monitor_ms_t recovery_ms)
{
    if (controller == 0) return;

    if (controller->state == RESET_CONTROLLER_STARTUP) {
        if (heartbeat_status == HEARTBEAT_MONITOR_HEALTHY) {
            controller->state = RESET_CONTROLLER_MONITORING;
        } else if (heartbeat_status == HEARTBEAT_MONITOR_TIMEOUT) {
            reset_controller_begin_assert(controller, now_ms);
        }
        return;
    }

    if (controller->state == RESET_CONTROLLER_MONITORING) {
        if (heartbeat_status == HEARTBEAT_MONITOR_TIMEOUT) {
            reset_controller_begin_assert(controller, now_ms);
        }
        return;
    }

    if (controller->state == RESET_CONTROLLER_ASSERT) {
        if (reset_controller_elapsed(now_ms, controller->state_started_ms) >= pulse_ms) {
            controller->state = RESET_CONTROLLER_WAIT_RECOVERY;
            /* 从释放动作这一轮开始恢复窗口，复用时间戳，不增加 DATA 字段。 */
            controller->state_started_ms = now_ms;
        }
        return;
    }

    if (controller->state != RESET_CONTROLLER_WAIT_RECOVERY) return;

    /* WAIT_RECOVERY：心跳恢复回到监测；主机起不来则按恢复窗口持续重复复位。 */
    if (heartbeat_status == HEARTBEAT_MONITOR_HEALTHY) {
        controller->state = RESET_CONTROLLER_MONITORING;
    } else if (reset_controller_elapsed(now_ms, controller->state_started_ms) >= recovery_ms) {
        reset_controller_begin_assert(controller, now_ms);
    }
}

unsigned char reset_controller_output_active(const reset_controller_t *controller)
{
    return controller != 0 && controller->state == RESET_CONTROLLER_ASSERT;
}

#ifdef RESET_CONTROLLER_HOST_TEST
reset_controller_state_t reset_controller_state(const reset_controller_t *controller)
{
    if (controller == 0) return RESET_CONTROLLER_STARTUP;
    return controller->state;
}

unsigned long reset_controller_reset_count(const reset_controller_t *controller)
{
    if (controller == 0) return 0UL;
    return controller->reset_count;
}
#endif
