#ifndef STC8G1K08_MAINTENANCE_CONTROLLER_H
#define STC8G1K08_MAINTENANCE_CONTROLLER_H

#ifdef MAINTENANCE_CONTROLLER_HOST_TEST
#include <stdint.h>
typedef uint32_t maintenance_controller_ms_t;
#else
/* Keil C51 unsigned long is the 32-bit Timer0 millisecond counter. */
typedef unsigned long maintenance_controller_ms_t;
#endif

#define MAINTENANCE_DEFAULT_LEASE_SECONDS 1800UL
#define MAINTENANCE_MIN_LEASE_SECONDS 60UL
#define MAINTENANCE_MAX_LEASE_SECONDS 3600UL
#define MAINTENANCE_MILLISECONDS_PER_SECOND 1000UL

typedef enum {
    MAINTENANCE_MODE_NORMAL = 0,
    MAINTENANCE_MODE_MAINTENANCE,
} maintenance_mode_t;

typedef enum {
    MAINTENANCE_COMMAND_INVALID = 0,
    MAINTENANCE_COMMAND_ENTER,
    MAINTENANCE_COMMAND_RENEW,
    MAINTENANCE_COMMAND_EXIT,
    MAINTENANCE_COMMAND_STATUS,
} maintenance_command_type_t;

typedef struct {
    maintenance_command_type_t type;
    unsigned long lease_seconds;
    unsigned char has_lease;
} maintenance_command_t;

typedef enum {
    MAINTENANCE_PARSE_OK = 0,
    MAINTENANCE_PARSE_EMPTY,
    MAINTENANCE_PARSE_INVALID_PREFIX,
    MAINTENANCE_PARSE_UNKNOWN_COMMAND,
    MAINTENANCE_PARSE_INVALID_ARGUMENT,
} maintenance_parse_result_t;

typedef enum {
    MAINTENANCE_ERROR_NONE = 0,
    MAINTENANCE_ERROR_LEASE_RANGE,
    MAINTENANCE_ERROR_NOT_ACTIVE,
    MAINTENANCE_ERROR_INVALID_COMMAND,
} maintenance_controller_error_t;

typedef enum {
    MAINTENANCE_ACTION_REJECTED = 0,
    MAINTENANCE_ACTION_ENTERED,
    MAINTENANCE_ACTION_RENEWED,
    MAINTENANCE_ACTION_RESUMED,
    MAINTENANCE_ACTION_STATUS,
} maintenance_controller_action_t;

typedef struct {
    maintenance_mode_t mode;
    maintenance_controller_ms_t started_at_ms;
    maintenance_controller_ms_t lease_ms;
} maintenance_controller_t;

maintenance_parse_result_t maintenance_controller_parse_line(
    const char *line, maintenance_command_t *command);
void maintenance_controller_init(maintenance_controller_t *controller,
                                  maintenance_controller_ms_t now_ms);
maintenance_controller_action_t maintenance_controller_execute(
    maintenance_controller_t *controller,
    const maintenance_command_t *command,
    maintenance_controller_ms_t now_ms,
    maintenance_controller_error_t *error);
unsigned char maintenance_controller_update(maintenance_controller_t *controller,
                                             maintenance_controller_ms_t now_ms);
maintenance_mode_t maintenance_controller_mode(
    const maintenance_controller_t *controller);
maintenance_controller_ms_t maintenance_controller_remaining_ms(
    const maintenance_controller_t *controller,
    maintenance_controller_ms_t now_ms);

#endif
