#ifndef STC8G1K08_RESET_CONTROLLER_H
#define STC8G1K08_RESET_CONTROLLER_H

#include "heartbeat_monitor.h"

typedef enum {
    RESET_CONTROLLER_STARTUP = 0,
    RESET_CONTROLLER_MONITORING,
    RESET_CONTROLLER_ASSERT,
    RESET_CONTROLLER_WAIT_RECOVERY,
} reset_controller_state_t;

typedef struct {
    reset_controller_state_t state;
    heartbeat_monitor_ms_t assert_started_ms;
    unsigned long reset_count;
} reset_controller_t;

void reset_controller_init(reset_controller_t *controller,
                            heartbeat_monitor_ms_t now_ms);
void reset_controller_update(reset_controller_t *controller,
                             heartbeat_monitor_status_t heartbeat_status,
                             heartbeat_monitor_ms_t now_ms,
                             heartbeat_monitor_ms_t pulse_ms);
#ifdef RESET_CONTROLLER_HOST_TEST
reset_controller_state_t reset_controller_state(const reset_controller_t *controller);
#endif
unsigned char reset_controller_output_active(const reset_controller_t *controller);
#ifdef RESET_CONTROLLER_HOST_TEST
unsigned long reset_controller_reset_count(const reset_controller_t *controller);

#endif
#endif
