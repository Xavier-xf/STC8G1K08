#include "maintenance_runtime.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

unsigned char P5, P5M0, P5M1;

/* Feed the API an explicitly wrapping clock, not the host's wider unsigned int.
 * Keep a separate elapsed clock so expected wire intervals cannot wrap too. */
static uint16_t clock_ms;
static uint32_t elapsed_ms;
static uint32_t reply_edges[43];
static unsigned int reply_edge_count;
static unsigned int wraps;

static int line_high(void)
{
    assert((P5M1 & 0x10U) != 0U);
    if (P5M0 & 0x10U) {
        assert((P5 & 0x10U) == 0U);
        return 0;
    }
    assert((P5 & 0x10U) != 0U);
    return 1;
}

static void advance(unsigned int duration)
{
    while (duration--) {
        int before = line_high();

        clock_ms = (uint16_t)(clock_ms + 1U);
        ++elapsed_ms;
        if (clock_ms == 0U) ++wraps;
        maintenance_runtime_tick();
        assert((P5 & 0x20U) == 0U);
        assert((P5M0 & 0x20U) != 0U && (P5M1 & 0x20U) == 0U);
        if (before && !line_high()) {
            assert(reply_edge_count < sizeof(reply_edges) / sizeof(reply_edges[0]));
            reply_edges[reply_edge_count++] = elapsed_ms;
            /* A generated reply edge must never be counted as a heartbeat. */
            assert(maintenance_runtime_on_falling_edge(clock_ms) == 0U);
        }
    }
}

static unsigned int host_edge(unsigned int interval)
{
    advance(interval);
    return maintenance_runtime_on_falling_edge(clock_ms);
}

static void setup(uint16_t start)
{
    P5 = 0U;
    P5M0 = 0x20U;
    P5M1 = 0U;
    clock_ms = start;
    elapsed_ms = 0U;
    reply_edge_count = 0U;
    wraps = 0U;
    maintenance_runtime_init();
    assert(line_high());
}

static unsigned char crc8(const unsigned char *bytes, unsigned int size)
{
    unsigned char crc = 0U;
    unsigned int index, bit;

    for (index = 0U; index < size; ++index) {
        crc ^= bytes[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (unsigned char)((crc & 0x80U) ?
                                  ((unsigned int)crc << 1) ^ 0x07U :
                                  (unsigned int)crc << 1);
        }
    }
    return crc;
}

static void send_request(unsigned char command, unsigned char sequence)
{
    unsigned char bytes[3] = {command, sequence, 0U};
    unsigned int index, bit;

    bytes[2] = crc8(bytes, 2U);
    assert(host_edge(0U) == 0U);
    assert(host_edge(400U) == 0U);
    assert(host_edge(32U) == 0U);
    for (index = 0U; index < sizeof(bytes); ++index) {
        for (bit = 0U; bit < 8U; ++bit) {
            assert(host_edge((bytes[index] & (0x80U >> bit)) ? 14U : 8U) == 0U);
        }
    }
}

static void expect_reply(unsigned char command, unsigned char sequence,
                         unsigned char result, unsigned char status)
{
    unsigned char decoded[5] = {0U};
    unsigned char expected[5] = {(unsigned char)(command | 0x80U), sequence,
                                 result, status, 0U};
    uint32_t start = elapsed_ms;
    unsigned int bit;

    expected[4] = crc8(expected, 4U);
    reply_edge_count = 0U;
    maintenance_runtime_reply(result, status);
    assert(maintenance_runtime_poll() == MRT_COMMAND_NONE);
    advance(99U);
    assert(reply_edge_count == 0U && line_high());
    advance(1U);
    assert(reply_edge_count == 1U && !line_high());
    advance(1500U);
    assert(reply_edge_count == 43U && line_high());
    assert(reply_edges[0] - start == 100U);
    assert(reply_edges[1] - reply_edges[0] == 400U);
    assert(reply_edges[2] - reply_edges[1] == 32U);
    for (bit = 0U; bit < 40U; ++bit) {
        uint32_t interval = reply_edges[bit + 3U] - reply_edges[bit + 2U];

        assert(interval == 8U || interval == 14U);
        decoded[bit / 8U] = (unsigned char)((decoded[bit / 8U] << 1) |
                                           (interval == 14U));
    }
    assert(memcmp(decoded, expected, sizeof(expected)) == 0);
    assert(maintenance_runtime_poll() == MRT_COMMAND_NONE);
}

static void expect_heartbeat_recovery(void)
{
    assert(host_edge(0U) == 0U);
    assert(host_edge(200U) == 200U);
    assert(host_edge(200U) == 200U);
    assert(maintenance_runtime_poll() == MRT_COMMAND_NONE && line_high());
}

static void test_normal_heartbeats_across_wrap(void)
{
    unsigned int index;

    /* Preserve the old no-false-break intent using valid normal heartbeat
     * intervals. The second edge lands exactly on zero, with no 400 ms break. */
    setup((uint16_t)(65536UL - 200UL));
    assert(host_edge(0U) == 0U);
    for (index = 0U; index < 400U; ++index) {
        assert(host_edge(200U) == 200U);
        assert(maintenance_runtime_poll() == MRT_COMMAND_NONE);
    }
    assert(wraps == 2U && reply_edge_count == 0U && line_high());
}

static void test_request_and_reply_wrap_positions(void)
{
    unsigned int offset;
    unsigned char status = MRT_STATUS_ENABLED | MRT_STATUS_HEARTBEAT |
        (MRT_PHASE_MONITORING << MRT_STATUS_PHASE_SHIFT);

    /* Move the wrap through every millisecond of break/sync/data, the bounded
     * main-loop handoff, TX wait and complete reply; not just one start value. */
    for (offset = 1U; offset <= 2000U; ++offset) {
        unsigned char command = (unsigned char)(MRT_COMMAND_START + offset % 3U);
        unsigned char sequence = (unsigned char)(1U + offset % 255U);

        setup((uint16_t)(65536UL - offset));
        send_request(command, sequence);
        advance(249U);
        assert(maintenance_runtime_poll() == command);
        assert(maintenance_runtime_poll() == MRT_COMMAND_NONE);
        expect_reply(command, sequence, MRT_RESULT_OK, status);
        assert(wraps == 1U);
        expect_heartbeat_recovery();
    }
}

static void test_timeouts_at_wrap(void)
{
    unsigned int scenario;

    /* Each deadline falls exactly at 0: sync, partial data, ready-but-unpolled,
     * polled-but-unreplied, and idle. Never let a uint16 subtraction extend it. */
    for (scenario = 0U; scenario < 5U; ++scenario) {
        unsigned int timeout;
        unsigned int lead;

        switch (scenario) {
        case 0U: timeout = 35U; lead = 400U; break;
        case 1U: timeout = 17U; lead = 400U + 32U + 8U; break;
        case 2U:
        case 3U: timeout = 250U; lead = 0U; break;
        default: timeout = 1000U; lead = 0U; break;
        }
        if (scenario == 2U || scenario == 3U) {
            /* Measure an independent request's duration to align its deadline. */
            setup(0U);
            send_request(MRT_COMMAND_READ, 0xA5U);
            lead = (unsigned int)elapsed_ms;
        }
        setup((uint16_t)(65536UL - lead - timeout));
        if (scenario == 2U || scenario == 3U) {
            send_request(MRT_COMMAND_READ, 0xA5U);
            if (scenario == 3U) assert(maintenance_runtime_poll() == MRT_COMMAND_READ);
        } else {
            assert(host_edge(0U) == 0U);
            if (scenario != 4U) assert(host_edge(400U) == 0U);
            if (scenario == 1U) {
                assert(host_edge(32U) == 0U);
                assert(host_edge(8U) == 0U);
            }
        }
        advance(timeout - 1U);
        assert(clock_ms == UINT16_MAX && wraps == 0U);
        advance(1U);
        assert(clock_ms == 0U && wraps == 1U);
        assert(maintenance_runtime_poll() == MRT_COMMAND_NONE);
        /* A late reply must not resurrect expired READY/EXECUTING state. */
        maintenance_runtime_reply(MRT_RESULT_OK, MRT_STATUS_ENABLED);
        advance(1600U);
        assert(reply_edge_count == 0U && line_high());
        expect_heartbeat_recovery();
        send_request(MRT_COMMAND_READ, 0x5AU);
        assert(maintenance_runtime_poll() == MRT_COMMAND_READ);
        expect_reply(MRT_COMMAND_READ, 0x5AU, MRT_RESULT_OK, 0U);
        expect_heartbeat_recovery();
    }
}

static void test_full_wrap_idle_does_not_reuse_stale_edge(void)
{
    setup(65336U);
    assert(host_edge(0U) == 0U);
    assert(host_edge(200U) == 200U);
    advance(65536U);
    /* Modulo age looks like a heartbeat/break, but the idle timer forgot it. */
    assert(host_edge(400U) == 0U);
    assert(host_edge(32U) == 0U);
    assert(host_edge(200U) == 200U);
    assert(reply_edge_count == 0U && maintenance_runtime_poll() == MRT_COMMAND_NONE);
}

int main(void)
{
    test_normal_heartbeats_across_wrap();
    puts("PASS: 16-bit normal heartbeat stream across two wraps, no false break");
    test_request_and_reply_wrap_positions();
    puts("PASS: 2000 wrap positions across START/STOP/READ requests, handoff and full CRC replies");
    test_timeouts_at_wrap();
    puts("PASS: five timeout deadlines at wrap, late reply ignored, heartbeat/request recovery");
    test_full_wrap_idle_does_not_reuse_stale_edge();
    puts("PASS: full-wrap idle discards stale heartbeat/break interval");
    return 0;
}
