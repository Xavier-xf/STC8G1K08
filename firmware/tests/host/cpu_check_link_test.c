#include "cpu_check_link.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

unsigned char P5, P5M0, P5M1;
static uint32_t clock_ms;
static uint32_t falling_edges[43];
static unsigned int edge_count;
static unsigned int accepted_heartbeats;
/* Reply pulses stay low 4 ms so Tina 4.9 gpiolib samples the level before release. */
#define REPLY_LOW_MS 4U
static uint32_t low_started_ms;
static unsigned int low_pulses;

static unsigned char crc8(const unsigned char *bytes, unsigned int size)
{
    unsigned char crc = 0;
    unsigned int i, shift;
    for (i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (shift = 0; shift < 8; ++shift)
            crc = (unsigned char)((crc & 0x80U) ? ((unsigned int)crc << 1) ^ 0x07U : (unsigned int)crc << 1);
    }
    return crc;
}

static int line_high(void)
{
    /* High means input/high-Z release, never a push-pull or driven-high mode. */
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
        ++clock_ms;
        cpu_check_link_tick();
        assert((P5 & 0x20U) == 0);
        assert((P5M0 & 0x20U) != 0);
        assert((P5M1 & 0x20U) == 0);
        if (before && !line_high()) {
            assert(edge_count < sizeof(falling_edges) / sizeof(falling_edges[0]));
            falling_edges[edge_count++] = clock_ms;
            low_started_ms = clock_ms;
            assert(cpu_check_link_on_falling_edge((uint16_t)clock_ms) == 0);
        } else if (!before && line_high()) {
            assert(clock_ms - low_started_ms == REPLY_LOW_MS);
            ++low_pulses;
        }
    }
}

static unsigned int host_edge(unsigned int interval)
{
    unsigned int age;
    advance(interval);
    age = cpu_check_link_on_falling_edge((uint16_t)clock_ms);
    if (age) ++accepted_heartbeats;
    return age;
}

static void setup(uint32_t start)
{
    P5 = 0;
    P5M0 = 0x20U;
    P5M1 = 0;
    clock_ms = start;
    edge_count = 0;
    accepted_heartbeats = 0;
    cpu_check_link_init();
    assert(line_high());
}

static void request(unsigned char command, unsigned char sequence, int corrupt)
{
    unsigned char bytes[3] = {command, sequence, 0};
    unsigned int i, shift;
    bytes[2] = crc8(bytes, 2);
    if (corrupt) bytes[2] ^= 1U;
    (void)host_edge(20);
    assert(host_edge(400) == 0);
    assert(host_edge(32) == 0);
    for (i = 0; i < sizeof(bytes); ++i)
        for (shift = 0; shift < 8; ++shift)
            assert(host_edge((bytes[i] & (0x80U >> shift)) ? 14 : 8) == 0);
}

static void expect_reply(unsigned char command, unsigned char sequence,
                         unsigned char result, unsigned char status)
{
    unsigned char decoded[5] = {0};
    unsigned int i;
    uint32_t start = clock_ms;
    edge_count = 0;
    low_pulses = 0;
    cpu_check_link_reply(result, status);
    assert(cpu_check_link_poll() == LINK_COMMAND_NONE);
    advance(99);
    assert(edge_count == 0);
    advance(1500);
    assert(edge_count == 43 && low_pulses == 43 && line_high());
    assert(falling_edges[0] - start == 100);
    assert(falling_edges[1] - falling_edges[0] == 400);
    assert(falling_edges[2] - falling_edges[1] == 32);
    for (i = 0; i < 40; ++i) {
        uint32_t interval = falling_edges[i + 3] - falling_edges[i + 2];
        assert(interval == 8 || interval == 14);
        decoded[i / 8] = (unsigned char)((decoded[i / 8] << 1) | (interval == 14));
    }
    assert(decoded[0] == (command | 0x80U) && decoded[1] == sequence);
    assert(decoded[2] == result && decoded[3] == status);
    assert(decoded[4] == crc8(decoded, 4));
    assert(cpu_check_link_poll() == LINK_COMMAND_NONE);
}

int main(void)
{
    unsigned char command;
    for (command = LINK_COMMAND_START; command <= LINK_COMMAND_READ; ++command) {
        setup(0);
        request(command, 7, 0);
        assert(cpu_check_link_poll() == command);
        expect_reply(command, 7, LINK_RESULT_OK, LINK_STATUS_ENABLED | (LINK_PHASE_STARTUP << 2));
        assert(accepted_heartbeats == 0);
    }
    setup(65500);
    request(LINK_COMMAND_READ, 255, 0);
    assert(cpu_check_link_poll() == LINK_COMMAND_READ);
    expect_reply(LINK_COMMAND_READ, 255, LINK_RESULT_OK, 0);

    setup(0);
    request(42, 1, 0);
    assert(cpu_check_link_poll() == 42);
    expect_reply(42, 1, LINK_RESULT_INVALID, 0);
    request(42, 1, 0);
    assert(cpu_check_link_poll() == 42);
    expect_reply(42, 1, LINK_RESULT_INVALID, 0);

    setup(0);
    request(LINK_COMMAND_STOP, 1, 1);
    assert(cpu_check_link_poll() == LINK_COMMAND_NONE);
    advance(1600);
    assert(edge_count == 0);
    request(LINK_COMMAND_STOP, 1, 0);
    assert(cpu_check_link_poll() == LINK_COMMAND_STOP);
    expect_reply(LINK_COMMAND_STOP, 1, LINK_RESULT_OK, 0);

    setup(0);
    request(LINK_COMMAND_READ, 0, 0);
    assert(cpu_check_link_poll() == LINK_COMMAND_NONE);
    request(LINK_COMMAND_READ, 1, 0);
    advance(200);
    assert(cpu_check_link_poll() == LINK_COMMAND_READ);
    expect_reply(LINK_COMMAND_READ, 1, LINK_RESULT_OK, 0);
    request(LINK_COMMAND_START, 2, 0);
    advance(250);
    assert(cpu_check_link_poll() == LINK_COMMAND_NONE);
    advance(1600);
    assert(edge_count == 43);

    /* EXECUTING has its own deadline; it does not inherit READY's remainder. */
    setup(0);
    request(LINK_COMMAND_READ, 9, 0);
    advance(249);
    assert(cpu_check_link_poll() == LINK_COMMAND_READ);
    advance(99);
    expect_reply(LINK_COMMAND_READ, 9, LINK_RESULT_OK, 0);
    request(LINK_COMMAND_STOP, 10, 0);
    assert(cpu_check_link_poll() == LINK_COMMAND_STOP);
    advance(100);
    edge_count = 0;
    cpu_check_link_reply(LINK_RESULT_OK, 0);
    advance(1600);
    assert(edge_count == 0 && line_high());

    setup(0);
    assert(host_edge(0) == 0);
    assert(host_edge(400) == 0);
    advance(35);
    assert(host_edge(200) == 0);
    assert(host_edge(200) == 200);
    setup(0);
    (void)host_edge(0);
    (void)host_edge(400);
    (void)host_edge(32);
    (void)host_edge(8);
    advance(17);
    assert(cpu_check_link_poll() == LINK_COMMAND_NONE);
    request(LINK_COMMAND_READ, 3, 0);
    assert(cpu_check_link_poll() == LINK_COMMAND_READ);
    expect_reply(LINK_COMMAND_READ, 3, LINK_RESULT_OK, 0);

    setup(65500);
    assert(host_edge(0) == 0);
    assert(host_edge(200) == 200);
    assert(host_edge(200) == 200);
    assert(accepted_heartbeats == 2);
    request(LINK_COMMAND_READ, 5, 0);
    assert(cpu_check_link_poll() == LINK_COMMAND_READ);
    expect_reply(LINK_COMMAND_READ, 5, LINK_RESULT_OK, 0);
    assert(accepted_heartbeats == 2);
    advance(65536);
    assert(host_edge(200) == 0);
    puts("PASS: 3-byte requests, 5-byte replies, CRC, NAK, time wrap, bounded handoff, partial frames and heartbeat isolation");
    return 0;
}
