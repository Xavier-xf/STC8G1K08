#include "cpu_check_command_decoder.h"

#include <stdint.h>
#include <stdio.h>

static int expect(int condition, const char *message)
{
    if (!condition) fprintf(stderr, "%s\n", message);
    return condition;
}

static void emit_enter_frame(cpu_check_command_decoder_t *decoder,
                             uint32_t base)
{
    cpu_check_command_decoder_on_falling_edge(decoder, base);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 900U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 980U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 1280U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 1360U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 1440U);
}

static void emit_exit_frame(cpu_check_command_decoder_t *decoder,
                            uint32_t base)
{
    cpu_check_command_decoder_on_falling_edge(decoder, base);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 900U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 980U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 1280U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 1360U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 1440U);
    cpu_check_command_decoder_on_falling_edge(decoder, base + 1520U);
}

static int test_valid_frames(void)
{
    cpu_check_command_decoder_t decoder;
    int ok = 1;

    cpu_check_command_decoder_init(&decoder);
    emit_enter_frame(&decoder, 0U);
    ok &= expect(cpu_check_command_decoder_poll(&decoder, 1940U) ==
                     CPU_CHECK_COMMAND_ENTER,
                 "three payload pulses decode ENTER after trailer");

    cpu_check_command_decoder_init(&decoder);
    emit_exit_frame(&decoder, 0U);
    ok &= expect(cpu_check_command_decoder_poll(&decoder, 2020U) ==
                     CPU_CHECK_COMMAND_EXIT,
                 "four payload pulses decode EXIT after trailer");
    return ok;
}

static int test_rejections(void)
{
    cpu_check_command_decoder_t decoder;
    int ok = 1;

    cpu_check_command_decoder_init(&decoder);
    cpu_check_command_decoder_on_falling_edge(&decoder, 0U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 200U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 400U);
    ok &= expect(cpu_check_command_decoder_poll(&decoder, 1200U) ==
                     CPU_CHECK_COMMAND_NONE,
                 "ordinary 200 ms heartbeat is not a command");

    cpu_check_command_decoder_init(&decoder);
    cpu_check_command_decoder_on_falling_edge(&decoder, 0U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 700U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 780U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 1080U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 1160U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 1240U);
    ok &= expect(cpu_check_command_decoder_poll(&decoder, 1800U) ==
                     CPU_CHECK_COMMAND_NONE,
                 "short break below threshold is rejected");

    cpu_check_command_decoder_init(&decoder);
    cpu_check_command_decoder_on_falling_edge(&decoder, 0U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 900U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 980U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 1280U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 1360U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 1500U);
    ok &= expect(cpu_check_command_decoder_poll(&decoder, 2100U) ==
                     CPU_CHECK_COMMAND_NONE,
                 "invalid payload interval is rejected");

    cpu_check_command_decoder_init(&decoder);
    emit_enter_frame(&decoder, 0U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 1520U);
    cpu_check_command_decoder_on_falling_edge(&decoder, 1600U);
    ok &= expect(cpu_check_command_decoder_poll(&decoder, 2200U) ==
                     CPU_CHECK_COMMAND_NONE,
                 "excess payload pulses are rejected");
    return ok;
}

static int test_timestamp_wrap(void)
{
    cpu_check_command_decoder_t decoder;
    uint32_t base = UINT32_MAX - 500U;

    cpu_check_command_decoder_init(&decoder);
    emit_enter_frame(&decoder, base);
    return expect(cpu_check_command_decoder_poll(&decoder, base + 1940U) ==
                      CPU_CHECK_COMMAND_ENTER,
                  "decoder handles millisecond timestamp wraparound");
}

static int test_compact_decoder_state(void)
{
    return expect(sizeof(cpu_check_command_decoder_t) <= 8U,
                  "decoder state exceeds the DATA-safe budget");
}

int main(void)
{
    int ok = 1;

    ok &= test_valid_frames();
    ok &= test_rejections();
    ok &= test_timestamp_wrap();
    ok &= test_compact_decoder_state();
    return ok ? 0 : 1;
}
