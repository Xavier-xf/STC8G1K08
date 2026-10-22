#include <stdio.h>

#include "maintenance_controller.h"

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static int test_parse_commands(void)
{
    maintenance_command_t command;
    int ok = 1;

    ok &= expect(maintenance_controller_parse_line("MNT ENTER", &command) ==
                     MAINTENANCE_PARSE_OK,
                 "ENTER without lease parses");
    ok &= expect(command.type == MAINTENANCE_COMMAND_ENTER &&
                     command.has_lease == 0U,
                 "ENTER without lease uses default at execution");

    ok &= expect(maintenance_controller_parse_line("MNT ENTER 60", &command) ==
                     MAINTENANCE_PARSE_OK,
                 "minimum lease parses");
    ok &= expect(command.lease_seconds == 60UL && command.has_lease != 0U,
                 "minimum lease is retained");

    ok &= expect(maintenance_controller_parse_line("MNT RENEW 3600", &command) ==
                     MAINTENANCE_PARSE_OK,
                 "maximum lease parses");
    ok &= expect(command.type == MAINTENANCE_COMMAND_RENEW &&
                     command.lease_seconds == 3600UL,
                 "renew command retains its lease");

    ok &= expect(maintenance_controller_parse_line("MNT EXIT", &command) ==
                     MAINTENANCE_PARSE_OK &&
                     command.type == MAINTENANCE_COMMAND_EXIT,
                 "EXIT parses");
    ok &= expect(maintenance_controller_parse_line("MNT STATUS", &command) ==
                     MAINTENANCE_PARSE_OK &&
                     command.type == MAINTENANCE_COMMAND_STATUS,
                 "STATUS parses");

    ok &= expect(maintenance_controller_parse_line("MNT ENTER x", &command) ==
                     MAINTENANCE_PARSE_INVALID_ARGUMENT,
                 "non-numeric lease is rejected");
    ok &= expect(maintenance_controller_parse_line("MNT ENTER 60 extra", &command) ==
                     MAINTENANCE_PARSE_INVALID_ARGUMENT,
                 "extra lease argument is rejected");
    ok &= expect(maintenance_controller_parse_line("MNT NOPE", &command) ==
                     MAINTENANCE_PARSE_UNKNOWN_COMMAND,
                 "unknown command is rejected");
    ok &= expect(maintenance_controller_parse_line("MNT STATUS extra", &command) ==
                     MAINTENANCE_PARSE_INVALID_ARGUMENT,
                 "STATUS arguments are rejected");
    ok &= expect(maintenance_controller_parse_line("BAD ENTER", &command) ==
                     MAINTENANCE_PARSE_INVALID_PREFIX,
                 "invalid prefix is rejected");
    ok &= expect(maintenance_controller_parse_line("M", &command) ==
                     MAINTENANCE_PARSE_INVALID_PREFIX,
                 "short prefix is rejected");
    ok &= expect(maintenance_controller_parse_line("MN", &command) ==
                     MAINTENANCE_PARSE_INVALID_PREFIX,
                 "incomplete prefix is rejected");
    ok &= expect(maintenance_controller_parse_line("MNT", &command) ==
                     MAINTENANCE_PARSE_INVALID_PREFIX,
                 "missing command separator is rejected");
    return ok;
}


static int test_enter_renew_exit_and_bounds(void)
{
    maintenance_controller_t controller;
    maintenance_command_t command;
    maintenance_controller_error_t error;
    int ok = 1;

    maintenance_controller_init(&controller, 1000UL);
    ok &= expect(maintenance_controller_mode(&controller) ==
                     MAINTENANCE_MODE_NORMAL,
                 "controller starts in normal mode");

    command.type = MAINTENANCE_COMMAND_ENTER;
    command.has_lease = 0U;
    command.lease_seconds = 0UL;
    ok &= expect(maintenance_controller_execute(&controller, &command, 1000UL,
                                                 &error) ==
                     MAINTENANCE_ACTION_ENTERED,
                 "ENTER enters maintenance");
    ok &= expect(error == MAINTENANCE_ERROR_NONE &&
                     maintenance_controller_mode(&controller) ==
                         MAINTENANCE_MODE_MAINTENANCE,
                 "default ENTER has no error and changes mode");
    ok &= expect(maintenance_controller_remaining_ms(&controller, 1000UL) ==
                     1800000UL,
                 "default lease is 1800 seconds");

    command.type = MAINTENANCE_COMMAND_RENEW;
    command.has_lease = 1U;
    command.lease_seconds = 60UL;
    ok &= expect(maintenance_controller_execute(&controller, &command, 2000UL,
                                                 &error) ==
                     MAINTENANCE_ACTION_RENEWED,
                 "RENEW changes the active lease");
    ok &= expect(maintenance_controller_remaining_ms(&controller, 2000UL) ==
                     60000UL,
                 "renew starts from the current timestamp");

    command.type = MAINTENANCE_COMMAND_RENEW;
    command.lease_seconds = 59UL;
    ok &= expect(maintenance_controller_execute(&controller, &command, 2000UL,
                                                 &error) ==
                     MAINTENANCE_ACTION_REJECTED &&
                     error == MAINTENANCE_ERROR_LEASE_RANGE,
                 "lease below minimum is rejected");
    command.lease_seconds = 3601UL;
    ok &= expect(maintenance_controller_execute(&controller, &command, 2000UL,
                                                 &error) ==
                     MAINTENANCE_ACTION_REJECTED &&
                     error == MAINTENANCE_ERROR_LEASE_RANGE,
                 "lease above maximum is rejected");

    command.type = MAINTENANCE_COMMAND_EXIT;
    command.has_lease = 0U;
    ok &= expect(maintenance_controller_execute(&controller, &command, 3000UL,
                                                 &error) ==
                     MAINTENANCE_ACTION_RESUMED,
                 "EXIT resumes normal mode");
    ok &= expect(maintenance_controller_mode(&controller) ==
                     MAINTENANCE_MODE_NORMAL &&
                     maintenance_controller_remaining_ms(&controller, 3000UL) ==
                         0UL,
                 "EXIT clears the volatile lease");

    command.type = MAINTENANCE_COMMAND_EXIT;
    ok &= expect(maintenance_controller_execute(&controller, &command, 4000UL,
                                                 &error) ==
                     MAINTENANCE_ACTION_REJECTED &&
                     error == MAINTENANCE_ERROR_NOT_ACTIVE,
                 "EXIT is rejected outside maintenance");

    command.has_lease = 1U;
    command.type = MAINTENANCE_COMMAND_RENEW;
    command.lease_seconds = 60UL;
    ok &= expect(maintenance_controller_execute(&controller, &command, 3000UL,
                                                 &error) ==
                     MAINTENANCE_ACTION_REJECTED &&
                     error == MAINTENANCE_ERROR_NOT_ACTIVE,
                 "RENEW is rejected outside maintenance");
    return ok;
}

static int test_expiry_rollover_and_reset(void)
{
    maintenance_controller_t controller;
    maintenance_command_t command;
    maintenance_controller_error_t error;
    int ok = 1;

    maintenance_controller_init(&controller, 0xfffffff0UL);
    command.type = MAINTENANCE_COMMAND_ENTER;
    command.has_lease = 1U;
    command.lease_seconds = 60UL;
    maintenance_controller_execute(&controller, &command, 0xfffffff0UL, &error);
    ok &= expect(maintenance_controller_update(&controller, 0x0000ea4fUL) == 0U,
                 "lease remains active before rollover expiry");
    ok &= expect(maintenance_controller_remaining_ms(&controller, 0x0000ea4fUL) ==
                     1UL,
                 "remaining lease handles timestamp rollover");
    ok &= expect(maintenance_controller_update(&controller, 0x0000ea50UL) != 0U,
                 "lease expires at the exact duration");
    ok &= expect(maintenance_controller_mode(&controller) ==
                     MAINTENANCE_MODE_NORMAL,
                 "expiry returns to normal mode");
    maintenance_controller_init(&controller, 1234UL);
    command.type = MAINTENANCE_COMMAND_ENTER;
    command.has_lease = 1U;
    command.lease_seconds = 60UL;
    maintenance_controller_execute(&controller, &command, 1234UL, &error);
    maintenance_controller_init(&controller, 2000UL);
    ok &= expect(maintenance_controller_mode(&controller) ==
                     MAINTENANCE_MODE_NORMAL &&
                     maintenance_controller_remaining_ms(&controller, 2000UL) ==
                         0UL,
                 "reinitialization clears maintenance state");
    return ok;
}

int main(void)
{
    int ok = 1;

    ok &= test_parse_commands();
    ok &= test_enter_renew_exit_and_bounds();
    ok &= test_expiry_rollover_and_reset();
    if (!ok) return 1;
    puts("OK: maintenance controller test passed");
    return 0;
}
