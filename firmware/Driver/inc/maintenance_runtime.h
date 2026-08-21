#ifndef STC8G1K08_MAINTENANCE_RUNTIME_H
#define STC8G1K08_MAINTENANCE_RUNTIME_H

/*
 * 维护运行时 - 精简 P5.4 协议解码与维护模式管理
 *
 * 设计目标：
 * 1. 只占用很少 DATA（last_edge_ms: 4字节，状态/计数/标志: 1-2字节）
 * 2. 只处理 ENTER/RENEW/EXIT 二进制事件，不解析文本命令
 * 3. INT2 边沿先记入心跳监控，再交给此解码器
 * 4. 不依赖 XDATA 或复杂结构体
 */

#ifdef MAINTENANCE_RUNTIME_HOST_TEST
#include <stdint.h>
typedef uint32_t maintenance_runtime_ms_t;
#else
typedef unsigned long maintenance_runtime_ms_t;
#endif

/* P5.4 协议时序常量（与原 cpu_check_command_decoder 兼容）*/
#define MRT_BREAK_MIN_MS 750UL
#define MRT_SHORT_MIN_MS 60UL
#define MRT_SHORT_MAX_MS 150UL
#define MRT_DELIMITER_MIN_MS 240UL
#define MRT_DELIMITER_MAX_MS 360UL
#define MRT_TRAILER_MIN_MS 500UL
#define MRT_SYNC_TIMEOUT_MS 600UL

/* 维护模式租约常量 */
#define MRT_DEFAULT_LEASE_MS 1800000UL  /* 1800秒 = 30分钟 */
#define MRT_MIN_LEASE_MS 60000UL        /* 60秒 */
#define MRT_MAX_LEASE_MS 3600000UL      /* 3600秒 = 1小时 */

/*
 * 维护运行时状态。
 *
 * C51 Small 模型的生产 DATA 只有 128 字节。协议只需要比较小于 65 秒
 * 的边沿间隔，租约最长 1 小时，因此生产版不必保存两个完整的 32 位毫秒
 * 时间戳：边沿使用 16 位毫秒模数，租约使用 16 位 256ms tick。
 * 主机测试版保留完整毫秒字段，便于验证回绕和精确租约语义。
 */
#ifdef MAINTENANCE_RUNTIME_HOST_TEST
typedef struct {
    maintenance_runtime_ms_t last_edge_ms;
    maintenance_runtime_ms_t lease_end_ms;
    unsigned char state;
#else
typedef struct {
    unsigned int last_edge_ms;  /* 16 位毫秒模数，仅用于协议间隔 */
    unsigned int lease_end_ms;  /* 16 位 256ms tick 的截止时刻 */
    unsigned char state;
#endif
    /*
     * state 位域布局（避免 C51 bitfield 的额外开销）：
     * bit 0-1: decoder_state (0=IDLE, 1=SYNC, 2=DELIMITER, 3=PAYLOAD)
     * bit 2: have_edge (0=no, 1=yes)
     * bit 3-5: payload_count (0-7)
     * bit 6: maintenance_active (0=NORMAL, 1=MAINTENANCE)
     * bit 7: trailer event pending (event type follows maintenance_active)
     */
} maintenance_runtime_t;

/* 状态字段访问宏 */
#define MRT_DECODER_STATE(s) ((s) & 0x03U)
#define MRT_HAVE_EDGE(s) (((s) >> 2) & 0x01U)
#define MRT_PAYLOAD_COUNT(s) (((s) >> 3) & 0x07U)
#define MRT_MAINTENANCE_ACTIVE(s) (((s) >> 6) & 0x01U)
#define MRT_EVENT_PENDING(s) (((s) >> 7) & 0x01U)

#define MRT_SET_DECODER_STATE(s, v) ((s) = ((s) & 0xFCU) | ((v) & 0x03U))
#define MRT_SET_HAVE_EDGE(s, v) ((s) = ((s) & 0xFBU) | (((v) & 0x01U) << 2))
#define MRT_SET_PAYLOAD_COUNT(s, v) ((s) = ((s) & 0xC7U) | (((v) & 0x07U) << 3))
#define MRT_SET_MAINTENANCE_ACTIVE(s, v) ((s) = ((s) & 0xBFU) | (((v) & 0x01U) << 6))
#define MRT_SET_EVENT_PENDING(s, v) ((s) = ((s) & 0x7FU) | (((v) & 0x01U) << 7))

/* Decoder 状态常量 */
#define MRT_DECODER_IDLE 0U
#define MRT_DECODER_SYNC 1U
#define MRT_DECODER_DELIMITER 2U
#define MRT_DECODER_PAYLOAD 3U

/* 事件类型 */
typedef unsigned char maintenance_runtime_event_t;
#define MRT_EVENT_NONE 0U
#define MRT_EVENT_ENTER 1U
#define MRT_EVENT_EXIT 2U
#define MRT_EVENT_EXPIRED 3U

/* 模式查询 */
typedef unsigned char maintenance_runtime_mode_t;
#define MRT_MODE_NORMAL 0U
#define MRT_MODE_MAINTENANCE 1U

/* 初始化 */
void maintenance_runtime_init(maintenance_runtime_t *mrt,
                              maintenance_runtime_ms_t now_ms);

/* INT2 边沿处理（在心跳监控记录后调用）*/
void maintenance_runtime_on_falling_edge(maintenance_runtime_t *mrt,
                                         maintenance_runtime_ms_t now_ms);

/* 轮询事件（主循环调用）*/
maintenance_runtime_event_t maintenance_runtime_poll(
    maintenance_runtime_t *mrt,
    maintenance_runtime_ms_t now_ms);

/* 查询当前模式 */
maintenance_runtime_mode_t maintenance_runtime_mode(
    const maintenance_runtime_t *mrt);

#ifdef MAINTENANCE_RUNTIME_HOST_TEST
/* 查询是否在维护模式 */
unsigned char maintenance_runtime_is_active(const maintenance_runtime_t *mrt);

/* 查询剩余租约时间（仅用于测试和日志） */
maintenance_runtime_ms_t maintenance_runtime_remaining_ms(
    const maintenance_runtime_t *mrt,
    maintenance_runtime_ms_t now_ms);
#endif

#endif
