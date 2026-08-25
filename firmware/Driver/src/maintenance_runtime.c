#include "maintenance_runtime.h"

#ifdef MAINTENANCE_RUNTIME_HOST_TEST
typedef maintenance_runtime_ms_t mrt_interval_t;
#define MRT_EDGE_ELAPSED(now_ms, then_ms) \
    ((maintenance_runtime_ms_t)((now_ms) - (then_ms)))
#else
typedef unsigned int mrt_interval_t;
#define MRT_EDGE_ELAPSED(now_ms, then_ms) \
    ((unsigned int)((unsigned int)(now_ms) - (unsigned int)(then_ms)))
#endif

/* 重置解码器状态（保留 have_edge 和 monitor_enabled） */
static void mrt_decoder_reset(maintenance_runtime_t *mrt)
{
    unsigned char state = mrt->state;
    MRT_SET_DECODER_STATE(state, MRT_DECODER_IDLE);
    MRT_SET_PAYLOAD_COUNT(state, 0U);
    MRT_SET_HAVE_EDGE(state, 0U);
    mrt->state = state;
    /* 维护开关状态保持不变；下一帧从新的首边沿开始。 */
}

/* 初始化 */
void maintenance_runtime_init(maintenance_runtime_t *mrt,
                              maintenance_runtime_ms_t now_ms)
{
    if (mrt == 0) return;
    mrt->last_edge_ms = now_ms;
    mrt->state = 0U;  /* 全部字段初始化为 0 */
}

void maintenance_runtime_set_active(maintenance_runtime_t *mrt,
                                     unsigned char active)
{
    unsigned char state;

    if (mrt == 0) return;
    state = mrt->state;
    MRT_SET_MONITOR_ENABLED(state, active != 0U ? 1U : 0U);
    MRT_SET_EVENT_PENDING(state, 0U);
    mrt->state = state;
}

/* INT2 边沿处理 */
void maintenance_runtime_on_falling_edge(maintenance_runtime_t *mrt,
                                         maintenance_runtime_ms_t now_ms)
{
    mrt_interval_t interval;
    unsigned char state;
    unsigned char decoder_state;
    unsigned char payload_count;

    if (mrt == 0) return;

    state = mrt->state;

    /* 第一个边沿 */
    if (MRT_HAVE_EDGE(state) == 0U) {
        mrt->last_edge_ms = now_ms;
        MRT_SET_HAVE_EDGE(state, 1U);
        mrt->state = state;
        return;
    }

    /* 计算间隔 */
    interval = MRT_EDGE_ELAPSED(now_ms, mrt->last_edge_ms);
    mrt->last_edge_ms = now_ms;

    decoder_state = MRT_DECODER_STATE(state);

    /* IDLE: 等待 BREAK */
    if (decoder_state == MRT_DECODER_IDLE) {
        if (interval >= MRT_BREAK_MIN_MS) {
            MRT_SET_DECODER_STATE(state, MRT_DECODER_SYNC);
            mrt->state = state;
        }
        return;
    }

    /* SYNC: 等待短脉冲 */
    if (decoder_state == MRT_DECODER_SYNC) {
        if (interval < MRT_SHORT_MIN_MS || interval > MRT_SHORT_MAX_MS) {
            mrt_decoder_reset(mrt);
        } else {
            MRT_SET_DECODER_STATE(state, MRT_DECODER_DELIMITER);
            mrt->state = state;
        }
        return;
    }

    /* DELIMITER: 等待分隔符 */
    if (decoder_state == MRT_DECODER_DELIMITER) {
        if (interval < MRT_DELIMITER_MIN_MS || interval > MRT_DELIMITER_MAX_MS) {
            mrt_decoder_reset(mrt);
        } else {
            MRT_SET_DECODER_STATE(state, MRT_DECODER_PAYLOAD);
            MRT_SET_PAYLOAD_COUNT(state, 0U);
            mrt->state = state;
        }
        return;
    }

    /* PAYLOAD: 接收载荷脉冲 */
    payload_count = MRT_PAYLOAD_COUNT(state);
    if (interval >= MRT_TRAILER_MIN_MS) {
        /*
         * 主循环可能正被 UART 日志阻塞，导致 trailer 到期后的第一个
         * 普通心跳下降沿先进入 ISR。此时先锁存合法事件，不能把它当作
         * 错误载荷清掉；主循环稍后通过 poll() 取走事件。
         */
        if (payload_count == 3U) {
            MRT_SET_MONITOR_ENABLED(state, 1U);
            MRT_SET_EVENT_PENDING(state, 1U);
            mrt->state = state;
            mrt_decoder_reset(mrt);
            return;
        }
        if (payload_count == 4U) {
            MRT_SET_MONITOR_ENABLED(state, 0U);
            MRT_SET_EVENT_PENDING(state, 1U);
            mrt->state = state;
            mrt_decoder_reset(mrt);
            return;
        }
        mrt_decoder_reset(mrt);
    } else if (interval < MRT_SHORT_MIN_MS || interval > MRT_SHORT_MAX_MS) {
        mrt_decoder_reset(mrt);
    } else {
        ++payload_count;
        if (payload_count > 4U) {
            mrt_decoder_reset(mrt);
        } else {
            MRT_SET_PAYLOAD_COUNT(state, payload_count);
            mrt->state = state;
        }
    }
}

/* 轮询事件 */
maintenance_runtime_event_t maintenance_runtime_poll(
    maintenance_runtime_t *mrt,
    maintenance_runtime_ms_t now_ms)
{
    mrt_interval_t elapsed;
    unsigned char state;
    unsigned char decoder_state;
    unsigned char payload_count;
    unsigned char monitor_enabled;

    if (mrt == 0) return MRT_EVENT_NONE;

    state = mrt->state;
    monitor_enabled = MRT_MONITOR_ENABLED(state);

    if (MRT_EVENT_PENDING(state) != 0U) {
        MRT_SET_EVENT_PENDING(state, 0U);
        mrt->state = state;
        if (monitor_enabled != 0U) return MRT_EVENT_START;
        return MRT_EVENT_STOP;
    }

    /* 检查解码器状态 */
    if (MRT_HAVE_EDGE(state) == 0U) {
        return MRT_EVENT_NONE;
    }

    decoder_state = MRT_DECODER_STATE(state);

    /* PAYLOAD 状态：等待 trailer */
    if (decoder_state == MRT_DECODER_PAYLOAD) {
        elapsed = MRT_EDGE_ELAPSED(now_ms, mrt->last_edge_ms);
        if (elapsed >= MRT_TRAILER_MIN_MS) {
            payload_count = MRT_PAYLOAD_COUNT(state);

            /* 3 个脉冲 = START */
            if (payload_count == 3U) {
                MRT_SET_MONITOR_ENABLED(state, 1U);
                mrt->state = state;
                mrt_decoder_reset(mrt);
                return MRT_EVENT_START;
            }

            /* 4 个脉冲 = STOP */
            if (payload_count == 4U) {
                MRT_SET_MONITOR_ENABLED(state, 0U);
                mrt->state = state;
                mrt_decoder_reset(mrt);
                return MRT_EVENT_STOP;
            }

            /* 其他脉冲数无效 */
            mrt_decoder_reset(mrt);
        }
    }
    /* SYNC/DELIMITER 超时检查 */
    else if (decoder_state != MRT_DECODER_IDLE) {
        if (MRT_EDGE_ELAPSED(now_ms, mrt->last_edge_ms) >= MRT_SYNC_TIMEOUT_MS) {
            mrt_decoder_reset(mrt);
        }
    }

    return MRT_EVENT_NONE;
}

/* 查询当前模式 */
maintenance_runtime_mode_t maintenance_runtime_mode(
    const maintenance_runtime_t *mrt)
{
    if (mrt == 0) return MRT_MODE_NORMAL;
    return MRT_MONITOR_ENABLED(mrt->state) != 0U
        ? MRT_MODE_MAINTENANCE
        : MRT_MODE_NORMAL;
}

#ifdef MAINTENANCE_RUNTIME_HOST_TEST
/* 查询是否在维护模式 */
unsigned char maintenance_runtime_is_active(const maintenance_runtime_t *mrt)
{
    if (mrt == 0) return 0U;
    return MRT_MONITOR_ENABLED(mrt->state);
}

#endif
