#include "maintenance_runtime.h"

#ifdef MAINTENANCE_RUNTIME_HOST_TEST
#include <stdint.h>
typedef uint16_t mrt_tick_t;
extern unsigned char P5, P5M0, P5M1;
#else
#include "Config.h"
typedef unsigned int mrt_tick_t;
#endif

#define MRT_IDLE 0U
#define MRT_SYNC 1U
#define MRT_RECEIVE 2U
#define MRT_READY 3U
#define MRT_EXECUTING 4U
#define MRT_TX_WAIT 5U
#define MRT_TX_BREAK_LOW 6U
#define MRT_TX_BREAK_HIGH 7U
#define MRT_TX_SYNC_LOW 8U
#define MRT_TX_SYNC_HIGH 9U
#define MRT_TX_DATA_LOW 10U
#define MRT_TX_DATA_HIGH 11U

static volatile unsigned char frame[5];
static volatile unsigned char state;
static volatile unsigned char bit_count;
static volatile unsigned char have_edge;
static volatile mrt_tick_t last_edge_ms;
static volatile mrt_tick_t delay_ms;

/* Only Timer0 calls this; no main-loop/ISR overlay sharing. */
static void line_write(unsigned char high)
{
    if (high) {
        P5M1 |= 0x10U;
        P5M0 &= (unsigned char)~0x10U;
        P5 |= 0x10U;
    } else {
        P5 &= (unsigned char)~0x10U;
        P5M1 |= 0x10U;
        P5M0 |= 0x10U;
    }
}

static unsigned char frame_crc(unsigned char size)
{
    unsigned char crc = 0U;
    unsigned char index;
    unsigned char shift;

    for (index = 0U; index < size; ++index) {
        crc ^= frame[index];
        for (shift = 0U; shift < 8U; ++shift) {
            if (crc & 0x80U) crc = (unsigned char)((crc << 1) ^ 0x07U);
            else crc = (unsigned char)(crc << 1);
        }
    }
    return crc;
}

void maintenance_runtime_init(void)
{
    state = MRT_IDLE;
    bit_count = 0U;
    have_edge = 0U;
    last_edge_ms = 0U;
    delay_ms = 0U;
    P5M1 |= 0x10U;
    P5M0 &= (unsigned char)~0x10U;
    P5 |= 0x10U;
}

unsigned int maintenance_runtime_on_falling_edge(unsigned int now_ms)
{
    mrt_tick_t interval;

    if (state >= MRT_READY) return 0U;
    interval = (mrt_tick_t)((mrt_tick_t)now_ms - last_edge_ms);
    last_edge_ms = (mrt_tick_t)now_ms;
    if (!have_edge) {
        have_edge = 1U;
        delay_ms = 1000U;
        return 0U;
    }
    if (state == MRT_IDLE) {
        if (interval >= 398U && interval <= 402U) {
            state = MRT_SYNC;
            bit_count = 0U;
            delay_ms = 35U;
        } else {
            delay_ms = 1000U;
            if (interval >= 170U && interval <= 250U) return interval;
        }
        return 0U;
    }
    if (state == MRT_SYNC) {
        if (interval < 30U || interval > 34U) goto invalid;
        state = MRT_RECEIVE;
    } else {
        if (!((interval >= 6U && interval <= 10U) ||
              (interval >= 12U && interval <= 16U))) goto invalid;
        frame[bit_count >> 3] = (unsigned char)((frame[bit_count >> 3] << 1) |
                                               (interval >= 12U ? 1U : 0U));
        if (++bit_count == 24U) {
            state = MRT_READY;
            /* Allow the existing 9600-baud diagnostic line to finish. */
            delay_ms = 250U;
            return 0U;
        }
    }
    delay_ms = 17U;
    return 0U;
invalid:
    state = MRT_IDLE;
    have_edge = 0U;
    delay_ms = 0U;
    return 0U;
}

unsigned char maintenance_runtime_poll(void)
{
    if (state != MRT_READY) return MRT_COMMAND_NONE;
    if (!frame[0] || (frame[0] & 0x80U) || !frame[1] || frame_crc(2U) != frame[2]) {
        state = MRT_IDLE;
        have_edge = 0U;
        delay_ms = 0U;
        return MRT_COMMAND_NONE;
    }
    state = MRT_EXECUTING;
    return frame[0];
}

void maintenance_runtime_reply(unsigned char result, unsigned char status)
{
    if (state != MRT_EXECUTING) return;
    frame[0] |= 0x80U;
    frame[2] = result;
    frame[3] = status;
    frame[4] = frame_crc(4U);
    bit_count = 0U;
    delay_ms = 100U;
    state = MRT_TX_WAIT;
}

void maintenance_runtime_tick(void)
{
    if (delay_ms) {
        --delay_ms;
        if (delay_ms) return;
    }
    switch (state) {
    case MRT_IDLE:
        have_edge = 0U;
        break;
    case MRT_SYNC:
    case MRT_RECEIVE:
    case MRT_READY:
    case MRT_EXECUTING:
        state = MRT_IDLE;
        have_edge = 0U;
        break;
    case MRT_TX_WAIT:
        line_write(0U);
        state = MRT_TX_BREAK_LOW;
        delay_ms = 2U;
        break;
    case MRT_TX_BREAK_LOW:
        line_write(1U);
        state = MRT_TX_BREAK_HIGH;
        delay_ms = 398U;
        break;
    case MRT_TX_BREAK_HIGH:
        line_write(0U);
        state = MRT_TX_SYNC_LOW;
        delay_ms = 2U;
        break;
    case MRT_TX_SYNC_LOW:
        line_write(1U);
        state = MRT_TX_SYNC_HIGH;
        delay_ms = 30U;
        break;
    case MRT_TX_SYNC_HIGH:
        line_write(0U);
        state = MRT_TX_DATA_LOW;
        delay_ms = 2U;
        break;
    case MRT_TX_DATA_LOW:
        line_write(1U);
        if (bit_count == 40U) {
            state = MRT_IDLE;
            have_edge = 0U;
        } else {
            delay_ms = (frame[bit_count >> 3] & (0x80U >> (bit_count & 7U))) ? 12U : 6U;
            ++bit_count;
            state = MRT_TX_DATA_HIGH;
        }
        break;
    case MRT_TX_DATA_HIGH:
        line_write(0U);
        state = MRT_TX_DATA_LOW;
        delay_ms = 2U;
        break;
    }
}
