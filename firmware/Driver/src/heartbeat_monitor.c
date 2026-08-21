#include "heartbeat_monitor.h"

static heartbeat_monitor_ms_t heartbeat_monitor_elapsed(heartbeat_monitor_ms_t now_ms,
                                                        heartbeat_monitor_ms_t then_ms)
{
    return now_ms - then_ms;
}

void heartbeat_monitor_init(heartbeat_monitor_t *monitor, heartbeat_monitor_ms_t now_ms)
{
    if (monitor == 0) return;
    monitor->started_at_ms = now_ms;
    monitor->last_edge_ms = now_ms;
    monitor->edge_count = 0UL;
    monitor->edge_seen = 0U;
}

void heartbeat_monitor_on_falling_edge(heartbeat_monitor_t *monitor,
                                       heartbeat_monitor_ms_t now_ms)
{
    if (monitor == 0) return;
    monitor->last_edge_ms = now_ms;
    ++monitor->edge_count;
    monitor->edge_seen = 1U;
}

heartbeat_monitor_status_t heartbeat_monitor_status(const heartbeat_monitor_t *monitor,
                                                    heartbeat_monitor_ms_t now_ms,
                                                    heartbeat_monitor_ms_t grace_ms,
                                                    heartbeat_monitor_ms_t timeout_ms)
{
    if (monitor == 0) return HEARTBEAT_MONITOR_TIMEOUT;
    if (heartbeat_monitor_elapsed(now_ms, monitor->started_at_ms) < grace_ms) {
        return HEARTBEAT_MONITOR_STARTUP;
    }
    if (!monitor->edge_seen ||
        heartbeat_monitor_elapsed(now_ms, monitor->last_edge_ms) >= timeout_ms) {
        return HEARTBEAT_MONITOR_TIMEOUT;
    }
    return HEARTBEAT_MONITOR_HEALTHY;
}

#ifdef HEARTBEAT_MONITOR_HOST_TEST
unsigned long heartbeat_monitor_edge_count(const heartbeat_monitor_t *monitor)
{
    if (monitor == 0) return 0UL;
    return monitor->edge_count;
}

heartbeat_monitor_ms_t heartbeat_monitor_edge_age_ms(const heartbeat_monitor_t *monitor,
                                                      heartbeat_monitor_ms_t now_ms)
{
    if (monitor == 0) return 0UL;
    return heartbeat_monitor_elapsed(now_ms, monitor->last_edge_ms);
}
#endif
