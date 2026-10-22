#ifndef STC8G1K08_MAINTENANCE_RUNTIME_H
#define STC8G1K08_MAINTENANCE_RUNTIME_H

#define MRT_COMMAND_NONE 0U
#define MRT_COMMAND_START 1U
#define MRT_COMMAND_STOP 2U
#define MRT_COMMAND_READ 3U
#define MRT_RESULT_OK 0U
#define MRT_RESULT_INVALID 1U

#define MRT_STATUS_ENABLED 0x01U
#define MRT_STATUS_HEARTBEAT 0x02U
#define MRT_PHASE_STOPPED 0U
#define MRT_PHASE_STARTUP 1U
#define MRT_PHASE_MONITORING 2U
#define MRT_PHASE_RESET 3U
#define MRT_PHASE_RECOVERY 4U
#define MRT_STATUS_PHASE_SHIFT 2U

void maintenance_runtime_init(void);
void maintenance_runtime_tick(void);
/* Returns the age of the preceding confirmed heartbeat, or zero for protocol edges. */
unsigned int maintenance_runtime_on_falling_edge(unsigned int now_ms);
/* Main-loop handoff and reply calls require interrupts disabled. */
unsigned char maintenance_runtime_poll(void);
void maintenance_runtime_reply(unsigned char result, unsigned char status);

#endif
