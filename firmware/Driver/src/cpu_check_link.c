#include "cpu_check_link.h"

#ifdef CPU_CHECK_LINK_HOST_TEST
#include <stdint.h>
typedef uint16_t link_tick_t;
extern unsigned char P5, P5M0, P5M1;
#else
#include "Config.h"
/* C51 的 unsigned int 为 16 位；所有间隔按 16 位无符号差值计算。 */
typedef unsigned int link_tick_t;
#endif

#define LINK_LINE_MASK 0x10U /* P5.4 / CPU_CHECK */
#define LINK_REQUEST_BYTES 3U
#define LINK_REPLY_BYTES 5U
#define LINK_REPLY_FLAG 0x80U

/* 协议下降沿间隔与 Linux 端 stc_watchdog_protocol.h 的对应常量一致。 */
#define LINK_BREAK_MS 400U
#define LINK_SYNC_MS 32U
#define LINK_ZERO_MS 8U
#define LINK_ONE_MS 14U
#define LINK_TOLERANCE_MS 2U
/* 主机心跳 100 ms 翻转，下降沿名义间隔 200 ms。 */
#define LINK_HEARTBEAT_MIN_MS 170U
#define LINK_HEARTBEAT_MAX_MS 250U

/* 帧内等待下一沿的期限：最大合法间隔 + 1 ms 量化余量。 */
#define LINK_SYNC_TIMEOUT_MS (LINK_SYNC_MS + LINK_TOLERANCE_MS + 1U)
#define LINK_BIT_TIMEOUT_MS (LINK_ONE_MS + LINK_TOLERANCE_MS + 1U)
/* 空闲超过 1 s 未见下降沿则丢弃上一沿，避免很久之后的沿被当成心跳或前导。 */
#define LINK_IDLE_TIMEOUT_MS 1000U
/*
 * 主循环取走完整请求的期限。最坏阻塞来自 9600 baud 同步日志：
 * 最长 app_report 140 字符 + "AP_RESET release\r\n" 18 字符，约 165 ms。
 * 主机接收上限 1700 ms 依赖本期限，不能取消，也不能随意加大。
 */
#define LINK_HANDOFF_MS 250U
/*
 * poll 到 reply 之间只执行命令并取样状态（最长为 STOP 的释放日志约 19 ms）。
 * 独立计时，不继承 READY 剩余时间，正确性不依赖日志长度。
 */
#define LINK_EXECUTE_MS 100U
/* 应答准备好后等待主机切换为输入，对应主机 STC_WDT_TURNAROUND_MS。 */
#define LINK_TURNAROUND_MS 100U
/*
 * 应答低脉宽（请求方向仍为 2 ms）。Tina 4.9 gpiolib 在中断线程中读取电平，
 * 线程运行时线已回高则丢弃下降沿事件；4 ms 给线程留出余量。双方只按下降沿
 * 间隔解码，脉宽不改变帧格式；最短 8 ms 位间隔仍保留 4 ms 高电平。
 */
#define LINK_REPLY_LOW_MS 4U

#define LINK_IDLE 0U
#define LINK_SYNC 1U
#define LINK_RECEIVE 2U
/* 以下状态期间忽略输入边沿，自身应答不会被当成心跳。 */
#define LINK_READY 3U
#define LINK_EXECUTING 4U
#define LINK_TX_WAIT 5U
#define LINK_TX_BREAK_LOW 6U
#define LINK_TX_BREAK_HIGH 7U
#define LINK_TX_SYNC_LOW 8U
#define LINK_TX_SYNC_HIGH 9U
#define LINK_TX_DATA_LOW 10U
#define LINK_TX_DATA_HIGH 11U

/* 请求与应答复用同一缓冲区，节省 SMALL 模式 DATA。 */
static volatile unsigned char frame[LINK_REPLY_BYTES];
static volatile unsigned char state;
static volatile unsigned char bit_count;
static volatile unsigned char have_edge;
static volatile link_tick_t last_edge_ms;
/* Timer0 每 1 ms 递减；减到 0 时执行当前状态的到期动作。 */
static volatile link_tick_t delay_ms;
/* 仅 Timer0 调用：高电平=高阻释放（依赖外部上拉），低电平=开漏拉低。 */
static void line_write(unsigned char high)
{
    if (high) {
        P5M1 |= LINK_LINE_MASK;
        P5M0 &= (unsigned char)~LINK_LINE_MASK;
        P5 |= LINK_LINE_MASK;
    } else {
        P5 &= (unsigned char)~LINK_LINE_MASK;
        P5M1 |= LINK_LINE_MASK;
        P5M0 |= LINK_LINE_MASK;
    }
}

/* CRC-8：poly 0x07，init 0，无反射，无末尾异或。 */
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

void cpu_check_link_init(void)
{
    state = LINK_IDLE;
    bit_count = 0U;
    have_edge = 0U;
    last_edge_ms = 0U;
    delay_ms = 0U;
    /* 高阻输入，线路由外部上拉保持高电平。 */
    P5M1 |= LINK_LINE_MASK;
    P5M0 &= (unsigned char)~LINK_LINE_MASK;
    P5 |= LINK_LINE_MASK;
}

/*
 * 普通心跳由后一沿确认：只有与上一沿间隔落在心跳窗口内，才返回上一沿
 * 距今的时间。前导首沿之后紧跟 400 ms 间隔进入 SYNC，因此不会被记为心跳。
 */
unsigned int cpu_check_link_on_falling_edge(unsigned int now_ms)
{
    link_tick_t interval;

    if (state >= LINK_READY) return 0U;
    interval = (link_tick_t)((link_tick_t)now_ms - last_edge_ms);
    last_edge_ms = (link_tick_t)now_ms;
    if (!have_edge) {
        have_edge = 1U;
        delay_ms = LINK_IDLE_TIMEOUT_MS;
        return 0U;
    }
    if (state == LINK_IDLE) {
        if (interval >= LINK_BREAK_MS - LINK_TOLERANCE_MS &&
            interval <= LINK_BREAK_MS + LINK_TOLERANCE_MS) {
            state = LINK_SYNC;
            bit_count = 0U;
            delay_ms = LINK_SYNC_TIMEOUT_MS;
        } else {
            delay_ms = LINK_IDLE_TIMEOUT_MS;
            if (interval >= LINK_HEARTBEAT_MIN_MS && interval <= LINK_HEARTBEAT_MAX_MS)
                return interval;
        }
        return 0U;
    }
    if (state == LINK_SYNC) {
        if (interval < LINK_SYNC_MS - LINK_TOLERANCE_MS ||
            interval > LINK_SYNC_MS + LINK_TOLERANCE_MS) goto invalid;
        state = LINK_RECEIVE;
    } else {
        if (!((interval >= LINK_ZERO_MS - LINK_TOLERANCE_MS &&
               interval <= LINK_ZERO_MS + LINK_TOLERANCE_MS) ||
              (interval >= LINK_ONE_MS - LINK_TOLERANCE_MS &&
               interval <= LINK_ONE_MS + LINK_TOLERANCE_MS))) goto invalid;
        /* 高位先收；间隔不小于位 1 下限即为 1。 */
        frame[bit_count >> 3] = (unsigned char)((frame[bit_count >> 3] << 1) |
            (interval >= LINK_ONE_MS - LINK_TOLERANCE_MS ? 1U : 0U));
        if (++bit_count == LINK_REQUEST_BYTES * 8U) {
            state = LINK_READY;
            delay_ms = LINK_HANDOFF_MS;
            return 0U;
        }
    }
    delay_ms = LINK_BIT_TIMEOUT_MS;
    return 0U;
invalid:
    state = LINK_IDLE;
    have_edge = 0U;
    delay_ms = 0U;
    return 0U;
}

/* 请求：命令、非零序号、CRC。校验失败直接丢弃，不应答。 */
unsigned char cpu_check_link_poll(void)
{
    if (state != LINK_READY) return LINK_COMMAND_NONE;
    if (!frame[0] || (frame[0] & LINK_REPLY_FLAG) || !frame[1] ||
        frame_crc(LINK_REQUEST_BYTES - 1U) != frame[2]) {
        state = LINK_IDLE;
        have_edge = 0U;
        delay_ms = 0U;
        return LINK_COMMAND_NONE;
    }
    state = LINK_EXECUTING;
    delay_ms = LINK_EXECUTE_MS;
    return frame[0];
}

/* 应答：命令|0x80、原序号、结果、状态、CRC。期限已过的事务不再应答。 */
void cpu_check_link_reply(unsigned char result, unsigned char status)
{
    if (state != LINK_EXECUTING) return;
    frame[0] |= LINK_REPLY_FLAG;
    frame[2] = result;
    frame[3] = status;
    frame[4] = frame_crc(LINK_REPLY_BYTES - 1U);
    bit_count = 0U;
    delay_ms = LINK_TURNAROUND_MS;
    state = LINK_TX_WAIT;
}

/* 每个下降沿后保持低电平 LINK_REPLY_LOW_MS，再释放到下一沿。 */
void cpu_check_link_tick(void)
{
    if (delay_ms) {
        --delay_ms;
        if (delay_ms) return;
    }
    switch (state) {
    case LINK_IDLE:
        have_edge = 0U;
        break;
    case LINK_SYNC:
    case LINK_RECEIVE:
    case LINK_READY:
    case LINK_EXECUTING:
        state = LINK_IDLE;
        have_edge = 0U;
        break;
    case LINK_TX_WAIT:
        line_write(0U);
        state = LINK_TX_BREAK_LOW;
        delay_ms = LINK_REPLY_LOW_MS;
        break;
    case LINK_TX_BREAK_LOW:
        line_write(1U);
        state = LINK_TX_BREAK_HIGH;
        delay_ms = LINK_BREAK_MS - LINK_REPLY_LOW_MS;
        break;
    case LINK_TX_BREAK_HIGH:
        line_write(0U);
        state = LINK_TX_SYNC_LOW;
        delay_ms = LINK_REPLY_LOW_MS;
        break;
    case LINK_TX_SYNC_LOW:
        line_write(1U);
        state = LINK_TX_SYNC_HIGH;
        delay_ms = LINK_SYNC_MS - LINK_REPLY_LOW_MS;
        break;
    case LINK_TX_SYNC_HIGH:
        line_write(0U);
        state = LINK_TX_DATA_LOW;
        delay_ms = LINK_REPLY_LOW_MS;
        break;
    case LINK_TX_DATA_LOW:
        line_write(1U);
        if (bit_count == LINK_REPLY_BYTES * 8U) {
            state = LINK_IDLE;
            have_edge = 0U;
        } else {
            delay_ms = (frame[bit_count >> 3] & (0x80U >> (bit_count & 7U))) ?
                       LINK_ONE_MS - LINK_REPLY_LOW_MS : LINK_ZERO_MS - LINK_REPLY_LOW_MS;
            ++bit_count;
            state = LINK_TX_DATA_HIGH;
        }
        break;
    case LINK_TX_DATA_HIGH:
        line_write(0U);
        state = LINK_TX_DATA_LOW;
        delay_ms = LINK_REPLY_LOW_MS;
        break;
    }
}
