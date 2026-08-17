#ifndef STC8G1K08_HEARTBEAT_MONITOR_H
#define STC8G1K08_HEARTBEAT_MONITOR_H
#ifdef HEARTBEAT_MONITOR_HOST_TEST
#include <stdint.h>
typedef uint32_t heartbeat_monitor_ms_t;
#else
/* Keil C51 的 unsigned long 为 32 位，对应 Timer0 毫秒计数器。 */
typedef unsigned long heartbeat_monitor_ms_t;
#endif


typedef enum {
    HEARTBEAT_MONITOR_STARTUP = 0,
    HEARTBEAT_MONITOR_HEALTHY,
    HEARTBEAT_MONITOR_TIMEOUT,
} heartbeat_monitor_status_t;

typedef struct {
    heartbeat_monitor_ms_t started_at_ms;
    heartbeat_monitor_ms_t last_edge_ms;
    unsigned long edge_count;
    unsigned char edge_seen;
} heartbeat_monitor_t;

void heartbeat_monitor_init(heartbeat_monitor_t *monitor, heartbeat_monitor_ms_t now_ms);
void heartbeat_monitor_on_falling_edge(heartbeat_monitor_t *monitor,
                                       heartbeat_monitor_ms_t now_ms);
heartbeat_monitor_status_t heartbeat_monitor_status(const heartbeat_monitor_t *monitor,
                                                    heartbeat_monitor_ms_t now_ms,
                                                    heartbeat_monitor_ms_t grace_ms,
                                                    heartbeat_monitor_ms_t timeout_ms);
unsigned long heartbeat_monitor_edge_count(const heartbeat_monitor_t *monitor);
heartbeat_monitor_ms_t heartbeat_monitor_edge_age_ms(const heartbeat_monitor_t *monitor,
                                                      heartbeat_monitor_ms_t now_ms);

#endif
