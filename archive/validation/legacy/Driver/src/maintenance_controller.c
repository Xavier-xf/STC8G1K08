#include "maintenance_controller.h"
#ifdef MAINTENANCE_CONTROLLER_HOST_TEST

static const char *maintenance_skip_spaces(const char *text)
{
    while (*text == ' ') {
        ++text;
    }
    return text;
}

static unsigned char maintenance_match_word(const char *text,
                                             const char *word,
                                             const char **after)
{
    while (*word != '\0') {
        if (*text != *word) return 0U;
        ++text;
        ++word;
    }
    *after = text;
    return 1U;
}

static unsigned char maintenance_parse_seconds(const char *text,
                                               unsigned long *seconds)
{
    unsigned long value = 0UL;
    unsigned long digit;
    unsigned char has_digit = 0U;

    while (*text >= '0' && *text <= '9') {
        digit = (unsigned long)(*text - '0');
        if (value > (0xffffffffUL - digit) / 10UL) return 0U;
        value = value * 10UL + digit;
        has_digit = 1U;
        ++text;
    }
    if (!has_digit || *text != '\0') return 0U;
    *seconds = value;
    return 1U;
}

static maintenance_parse_result_t maintenance_parse_command_tail(
    const char *tail, maintenance_command_type_t type,
    maintenance_command_t *command)
{
    tail = maintenance_skip_spaces(tail);
    command->type = type;
    command->lease_seconds = 0UL;
    command->has_lease = 0U;

    if (type == MAINTENANCE_COMMAND_ENTER ||
        type == MAINTENANCE_COMMAND_RENEW) {
        if (*tail == '\0') return MAINTENANCE_PARSE_OK;
        if (!maintenance_parse_seconds(tail, &command->lease_seconds)) {
            return MAINTENANCE_PARSE_INVALID_ARGUMENT;
        }
        command->has_lease = 1U;
        return MAINTENANCE_PARSE_OK;
    }
    if (*tail != '\0') return MAINTENANCE_PARSE_INVALID_ARGUMENT;
    return MAINTENANCE_PARSE_OK;
}
maintenance_parse_result_t maintenance_controller_parse_line(
    const char *line, maintenance_command_t *command)
{
    const char *tail;

    if (command == 0 || line == 0 || *line == '\0') {
        return MAINTENANCE_PARSE_EMPTY;
    }
    command->type = MAINTENANCE_COMMAND_INVALID;
    command->lease_seconds = 0UL;
    command->has_lease = 0U;
    if (*line != 'M') return MAINTENANCE_PARSE_INVALID_PREFIX;
    ++line;
    if (*line != 'N') return MAINTENANCE_PARSE_INVALID_PREFIX;
    ++line;
    if (*line != 'T') return MAINTENANCE_PARSE_INVALID_PREFIX;
    ++line;
    if (*line != ' ') {
        return MAINTENANCE_PARSE_INVALID_PREFIX;
    }
    tail = maintenance_skip_spaces(line + 1);
    if (*tail == '\0') return MAINTENANCE_PARSE_UNKNOWN_COMMAND;
    if (maintenance_match_word(tail, "ENTER", &tail)) {
        return maintenance_parse_command_tail(tail,
                                               MAINTENANCE_COMMAND_ENTER,
                                               command);
    }
    if (maintenance_match_word(tail, "RENEW", &tail)) {
        return maintenance_parse_command_tail(tail,
                                               MAINTENANCE_COMMAND_RENEW,
                                               command);
    }
    if (maintenance_match_word(tail, "EXIT", &tail)) {
        return maintenance_parse_command_tail(tail,
                                               MAINTENANCE_COMMAND_EXIT,
                                               command);
    }
    if (maintenance_match_word(tail, "STATUS", &tail)) {
        return maintenance_parse_command_tail(tail,
                                               MAINTENANCE_COMMAND_STATUS,
                                               command);
    }
    return MAINTENANCE_PARSE_UNKNOWN_COMMAND;
}
#endif

static maintenance_controller_ms_t maintenance_elapsed(
    maintenance_controller_ms_t now_ms,
    maintenance_controller_ms_t then_ms)
{
    return now_ms - then_ms;
}

static void maintenance_set_error(maintenance_controller_error_t *error,
                                  maintenance_controller_error_t value)
{
    if (error != 0) *error = value;
}

static unsigned char maintenance_lease_ms(
    const maintenance_command_t *command,
    maintenance_controller_ms_t *lease_ms)
{
    unsigned long seconds = command->has_lease != 0U
                                ? command->lease_seconds
                                : MAINTENANCE_DEFAULT_LEASE_SECONDS;
    if (seconds < MAINTENANCE_MIN_LEASE_SECONDS ||
        seconds > MAINTENANCE_MAX_LEASE_SECONDS) {
        return 0U;
    }
    *lease_ms = (maintenance_controller_ms_t)(
        seconds * MAINTENANCE_MILLISECONDS_PER_SECOND);
    return 1U;
}

void maintenance_controller_init(maintenance_controller_t *controller,
                                  maintenance_controller_ms_t now_ms)
{
    if (controller == 0) return;
    controller->mode = MAINTENANCE_MODE_NORMAL;
    controller->started_at_ms = now_ms;
    controller->lease_ms = 0UL;
}

maintenance_controller_action_t maintenance_controller_execute(
    maintenance_controller_t *controller,
    const maintenance_command_t *command,
    maintenance_controller_ms_t now_ms,
    maintenance_controller_error_t *error)
{
    maintenance_controller_ms_t lease_ms;

    maintenance_set_error(error, MAINTENANCE_ERROR_NONE);
    if (controller == 0 || command == 0) {
        maintenance_set_error(error, MAINTENANCE_ERROR_INVALID_COMMAND);
        return MAINTENANCE_ACTION_REJECTED;
    }
    if (command->type == MAINTENANCE_COMMAND_STATUS) {
        return MAINTENANCE_ACTION_STATUS;
    }
    if (command->type == MAINTENANCE_COMMAND_ENTER) {
        if (!maintenance_lease_ms(command, &lease_ms)) {
            maintenance_set_error(error, MAINTENANCE_ERROR_LEASE_RANGE);
            return MAINTENANCE_ACTION_REJECTED;
        }
        controller->mode = MAINTENANCE_MODE_MAINTENANCE;
        controller->started_at_ms = now_ms;
        controller->lease_ms = lease_ms;
        return MAINTENANCE_ACTION_ENTERED;
    }
    if (command->type == MAINTENANCE_COMMAND_RENEW) {
        if (controller->mode != MAINTENANCE_MODE_MAINTENANCE) {
            maintenance_set_error(error, MAINTENANCE_ERROR_NOT_ACTIVE);
            return MAINTENANCE_ACTION_REJECTED;
        }
        if (!maintenance_lease_ms(command, &lease_ms)) {
            maintenance_set_error(error, MAINTENANCE_ERROR_LEASE_RANGE);
            return MAINTENANCE_ACTION_REJECTED;
        }
        controller->started_at_ms = now_ms;
        controller->lease_ms = lease_ms;
        return MAINTENANCE_ACTION_RENEWED;
    }
    if (command->type == MAINTENANCE_COMMAND_EXIT) {
        if (controller->mode != MAINTENANCE_MODE_MAINTENANCE) {
            maintenance_set_error(error, MAINTENANCE_ERROR_NOT_ACTIVE);
            return MAINTENANCE_ACTION_REJECTED;
        }
        controller->mode = MAINTENANCE_MODE_NORMAL;
        controller->started_at_ms = now_ms;
        controller->lease_ms = 0UL;
        return MAINTENANCE_ACTION_RESUMED;
    }
    maintenance_set_error(error, MAINTENANCE_ERROR_INVALID_COMMAND);
    return MAINTENANCE_ACTION_REJECTED;
}

unsigned char maintenance_controller_update(maintenance_controller_t *controller,
                                             maintenance_controller_ms_t now_ms)
{
    if (controller == 0 || controller->mode != MAINTENANCE_MODE_MAINTENANCE) {
        return 0U;
    }
    if (maintenance_elapsed(now_ms, controller->started_at_ms) <
        controller->lease_ms) {
        return 0U;
    }
    controller->mode = MAINTENANCE_MODE_NORMAL;
    controller->started_at_ms = now_ms;
    controller->lease_ms = 0UL;
    return 1U;
}

maintenance_mode_t maintenance_controller_mode(
    const maintenance_controller_t *controller)
{
    if (controller == 0) return MAINTENANCE_MODE_NORMAL;
    return controller->mode;
}

#ifdef MAINTENANCE_CONTROLLER_HOST_TEST
maintenance_controller_ms_t maintenance_controller_remaining_ms(
    const maintenance_controller_t *controller,
    maintenance_controller_ms_t now_ms)
{
    maintenance_controller_ms_t elapsed;

    if (controller == 0 || controller->mode != MAINTENANCE_MODE_MAINTENANCE) {
        return 0UL;
    }
    elapsed = maintenance_elapsed(now_ms, controller->started_at_ms);
    if (elapsed >= controller->lease_ms) return 0UL;
    return controller->lease_ms - elapsed;
}
#endif
