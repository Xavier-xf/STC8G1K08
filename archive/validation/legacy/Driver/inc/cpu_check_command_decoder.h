#ifndef STC8G1K08_CPU_CHECK_COMMAND_DECODER_H
#define STC8G1K08_CPU_CHECK_COMMAND_DECODER_H

#ifdef CPU_CHECK_COMMAND_DECODER_HOST_TEST
#include <stdint.h>
typedef uint32_t cpu_check_command_time_t;
#else
typedef unsigned long cpu_check_command_time_t;
#endif

typedef unsigned char cpu_check_command_t;

#define CPU_CHECK_COMMAND_NONE 0U
#define CPU_CHECK_COMMAND_ENTER 1U
#define CPU_CHECK_COMMAND_EXIT 2U

typedef unsigned char cpu_check_decoder_state_t;

#define CPU_CHECK_DECODER_IDLE 0U
#define CPU_CHECK_DECODER_SYNC 1U
#define CPU_CHECK_DECODER_DELIMITER 2U
#define CPU_CHECK_DECODER_PAYLOAD 3U

typedef struct {
    cpu_check_command_time_t last_edge_ms;
    unsigned char state : 2;
    unsigned char have_edge : 1;
    unsigned char payload_count : 3;
} cpu_check_command_decoder_t;

void cpu_check_command_decoder_init(cpu_check_command_decoder_t *decoder);
void cpu_check_command_decoder_on_falling_edge(
    cpu_check_command_decoder_t *decoder,
    cpu_check_command_time_t now_ms);
cpu_check_command_t cpu_check_command_decoder_poll(
    cpu_check_command_decoder_t *decoder,
    cpu_check_command_time_t now_ms);

#endif
