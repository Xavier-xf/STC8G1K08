#include "cpu_check_command_decoder.h"

#define CPU_CHECK_BREAK_MIN_MS 750UL
#define CPU_CHECK_SHORT_MIN_MS 60UL
#define CPU_CHECK_SHORT_MAX_MS 110UL
#define CPU_CHECK_DELIMITER_MIN_MS 240UL
#define CPU_CHECK_DELIMITER_MAX_MS 360UL
#define CPU_CHECK_TRAILER_MIN_MS 500UL
#define CPU_CHECK_SYNC_TIMEOUT_MS 600UL
#define CPU_CHECK_MAX_PAYLOAD_PULSES 4U


static unsigned char cpu_check_in_range(cpu_check_command_time_t value,
                                        cpu_check_command_time_t minimum,
                                        cpu_check_command_time_t maximum)
{
    return value >= minimum && value <= maximum;
}

static void cpu_check_decoder_reset(cpu_check_command_decoder_t *decoder)
{
    decoder->state = CPU_CHECK_DECODER_IDLE;
    decoder->payload_count = 0U;
}

void cpu_check_command_decoder_init(cpu_check_command_decoder_t *decoder)
{
    if (decoder == 0) return;
    decoder->state = CPU_CHECK_DECODER_IDLE;
    decoder->last_edge_ms = 0UL;
    decoder->have_edge = 0U;
    decoder->payload_count = 0U;
}

void cpu_check_command_decoder_on_falling_edge(
    cpu_check_command_decoder_t *decoder,
    cpu_check_command_time_t now_ms)
{
    cpu_check_command_time_t interval;

    if (decoder == 0) return;
    if (decoder->have_edge == 0U) {
        decoder->last_edge_ms = now_ms;
        decoder->have_edge = 1U;
        return;
    }
    interval = now_ms - decoder->last_edge_ms;
    decoder->last_edge_ms = now_ms;

    if (decoder->state == CPU_CHECK_DECODER_IDLE) {
        if (interval >= CPU_CHECK_BREAK_MIN_MS) {
            decoder->state = CPU_CHECK_DECODER_SYNC;
        }
        return;
    }
    if (decoder->state == CPU_CHECK_DECODER_SYNC) {
        if (!cpu_check_in_range(interval, CPU_CHECK_SHORT_MIN_MS,
                                CPU_CHECK_SHORT_MAX_MS)) {
            cpu_check_decoder_reset(decoder);
        } else {
            decoder->state = CPU_CHECK_DECODER_DELIMITER;
        }
        return;
    }
    if (decoder->state == CPU_CHECK_DECODER_DELIMITER) {
        if (!cpu_check_in_range(interval, CPU_CHECK_DELIMITER_MIN_MS,
                                CPU_CHECK_DELIMITER_MAX_MS)) {
            cpu_check_decoder_reset(decoder);
        } else {
            decoder->state = CPU_CHECK_DECODER_PAYLOAD;
            decoder->payload_count = 1U;
        }
        return;
    }
    if (!cpu_check_in_range(interval, CPU_CHECK_SHORT_MIN_MS,
                            CPU_CHECK_SHORT_MAX_MS)) {
        cpu_check_decoder_reset(decoder);
    } else {
        ++decoder->payload_count;
        if (decoder->payload_count > CPU_CHECK_MAX_PAYLOAD_PULSES) {
            cpu_check_decoder_reset(decoder);
        }
    }
}

cpu_check_command_t cpu_check_command_decoder_poll(
    cpu_check_command_decoder_t *decoder,
    cpu_check_command_time_t now_ms)
{
    cpu_check_command_time_t elapsed;
    cpu_check_command_t command = CPU_CHECK_COMMAND_NONE;

    if (decoder == 0 || decoder->have_edge == 0U) return command;
    if (decoder->state == CPU_CHECK_DECODER_PAYLOAD) {
        elapsed = now_ms - decoder->last_edge_ms;
        if (elapsed >= CPU_CHECK_TRAILER_MIN_MS) {
            if (decoder->payload_count == 3U) {
                command = CPU_CHECK_COMMAND_ENTER;
            } else if (decoder->payload_count == 4U) {
                command = CPU_CHECK_COMMAND_EXIT;
            }
            cpu_check_decoder_reset(decoder);
        }
    } else if (decoder->state != CPU_CHECK_DECODER_IDLE &&
               now_ms - decoder->last_edge_ms >=
                   CPU_CHECK_SYNC_TIMEOUT_MS) {
        cpu_check_decoder_reset(decoder);
    }
    return command;
}
