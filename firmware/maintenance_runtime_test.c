#include "maintenance_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 测试辅助宏 */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running %s...\n", #name); \
    test_##name(); \
    printf("  PASSED\n"); \
} while(0)

/* 测试：初始化 */
TEST(init)
{
    maintenance_runtime_t mrt;
    memset(&mrt, 0xFF, sizeof(mrt));

    maintenance_runtime_init(&mrt, 1000UL);

    assert(mrt.last_edge_ms == 1000UL);
    assert(mrt.lease_end_ms == 0UL);
    assert(mrt.state == 0U);
    assert(maintenance_runtime_mode(&mrt) == MRT_MODE_NORMAL);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
}

/* 测试：ENTER 命令（3 个脉冲）*/
TEST(enter_command)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;

    maintenance_runtime_init(&mrt, 0UL);

    /* BREAK: 900 ms */
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);

    /* SYNC: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, 980UL);

    /* DELIMITER: 300 ms */
    maintenance_runtime_on_falling_edge(&mrt, 1280UL);

    /* PAYLOAD 1: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, 1360UL);

    /* PAYLOAD 2: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, 1440UL);

    /* PAYLOAD 3: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, 1520UL);

    /* 等待 trailer >= 500 ms */
    event = maintenance_runtime_poll(&mrt, 1600UL);
    assert(event == MRT_EVENT_NONE);

    event = maintenance_runtime_poll(&mrt, 2020UL);
    assert(event == MRT_EVENT_ENTER);
    assert(maintenance_runtime_is_active(&mrt) == 1U);
    assert(maintenance_runtime_mode(&mrt) == MRT_MODE_MAINTENANCE);
}

/* 测试：EXIT 命令（4 个脉冲）*/
TEST(exit_command)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;

    maintenance_runtime_init(&mrt, 0UL);

    /* 先进入维护模式 */
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);
    maintenance_runtime_on_falling_edge(&mrt, 980UL);
    maintenance_runtime_on_falling_edge(&mrt, 1280UL);
    maintenance_runtime_on_falling_edge(&mrt, 1360UL);
    maintenance_runtime_on_falling_edge(&mrt, 1440UL);
    maintenance_runtime_on_falling_edge(&mrt, 1520UL);
    event = maintenance_runtime_poll(&mrt, 2020UL);
    assert(event == MRT_EVENT_ENTER);

    /* 模拟一些普通心跳 (200ms 间隔)，避免下一个命令被误判 */
    maintenance_runtime_on_falling_edge(&mrt, 2220UL);
    maintenance_runtime_on_falling_edge(&mrt, 2420UL);
    maintenance_runtime_on_falling_edge(&mrt, 2620UL);

    /* 发送 EXIT（4 个载荷脉冲）*/
    maintenance_runtime_ms_t exit_start = 3000UL;

    /* BREAK: 900 ms */
    maintenance_runtime_on_falling_edge(&mrt, exit_start);
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 900UL);

    /* SYNC: 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 980UL);

    /* DELIMITER: 300 ms */
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1280UL);

    /* PAYLOAD 1-4: 每个 80 ms */
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1360UL);
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1440UL);
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1520UL);
    maintenance_runtime_on_falling_edge(&mrt, exit_start + 1600UL);

    event = maintenance_runtime_poll(&mrt, exit_start + 2100UL);
    assert(event == MRT_EVENT_EXIT);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
    assert(maintenance_runtime_mode(&mrt) == MRT_MODE_NORMAL);
}

/* 测试：租约到期 */
TEST(lease_expiry)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;
    maintenance_runtime_ms_t remaining;

    maintenance_runtime_init(&mrt, 0UL);

    /* 进入维护模式 */
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);
    maintenance_runtime_on_falling_edge(&mrt, 980UL);
    maintenance_runtime_on_falling_edge(&mrt, 1280UL);
    maintenance_runtime_on_falling_edge(&mrt, 1360UL);
    maintenance_runtime_on_falling_edge(&mrt, 1440UL);
    maintenance_runtime_on_falling_edge(&mrt, 1520UL);
    event = maintenance_runtime_poll(&mrt, 2020UL);
    assert(event == MRT_EVENT_ENTER);

    /* 检查剩余时间 */
    remaining = maintenance_runtime_remaining_ms(&mrt, 2020UL);
    assert(remaining == MRT_DEFAULT_LEASE_MS);

    /* 租约未到期 */
    event = maintenance_runtime_poll(&mrt, 2020UL + MRT_DEFAULT_LEASE_MS - 1000UL);
    assert(event == MRT_EVENT_NONE);
    assert(maintenance_runtime_is_active(&mrt) == 1U);

    /* 租约到期 */
    event = maintenance_runtime_poll(&mrt, 2020UL + MRT_DEFAULT_LEASE_MS);
    assert(event == MRT_EVENT_EXPIRED);
    assert(maintenance_runtime_is_active(&mrt) == 0U);
    assert(maintenance_runtime_mode(&mrt) == MRT_MODE_NORMAL);
}

/* 测试：普通心跳不触发命令 */
TEST(normal_heartbeat_ignored)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;
    int i;

    maintenance_runtime_init(&mrt, 0UL);

    /* 模拟 200ms 周期的普通心跳 */
    for (i = 0; i < 100; ++i) {
        maintenance_runtime_on_falling_edge(&mrt, i * 200UL);
        event = maintenance_runtime_poll(&mrt, i * 200UL + 50UL);
        assert(event == MRT_EVENT_NONE);
        assert(maintenance_runtime_is_active(&mrt) == 0U);
    }
}

/* 测试：BREAK 被短脉冲打断 */
TEST(break_interrupted)
{
    maintenance_runtime_t mrt;

    maintenance_runtime_init(&mrt, 0UL);

    /* BREAK 开始 */
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);

    /* SYNC 阶段：收到超范围脉冲 */
    maintenance_runtime_on_falling_edge(&mrt, 1300UL);  /* 400 ms, 超出 SHORT_MAX */

    /* 应该重置到 IDLE */
    assert(MRT_DECODER_STATE(mrt.state) == MRT_DECODER_IDLE);
}

/* 测试：RENEW（在维护模式下的 ENTER）*/
TEST(renew_extends_lease)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;
    maintenance_runtime_ms_t remaining;

    maintenance_runtime_init(&mrt, 0UL);

    /* 进入维护模式 */
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);
    maintenance_runtime_on_falling_edge(&mrt, 980UL);
    maintenance_runtime_on_falling_edge(&mrt, 1280UL);
    maintenance_runtime_on_falling_edge(&mrt, 1360UL);
    maintenance_runtime_on_falling_edge(&mrt, 1440UL);
    maintenance_runtime_on_falling_edge(&mrt, 1520UL);
    event = maintenance_runtime_poll(&mrt, 2020UL);
    assert(event == MRT_EVENT_ENTER);

    /* 过半个租约 */
    maintenance_runtime_poll(&mrt, 2020UL + MRT_DEFAULT_LEASE_MS / 2);
    remaining = maintenance_runtime_remaining_ms(&mrt, 2020UL + MRT_DEFAULT_LEASE_MS / 2);
    assert(remaining == MRT_DEFAULT_LEASE_MS / 2);

    /* 在 RENEW 前插入一些普通心跳，避免误判 */
    maintenance_runtime_ms_t pre_renew = 2020UL + MRT_DEFAULT_LEASE_MS / 2 - 600UL;
    maintenance_runtime_on_falling_edge(&mrt, pre_renew);
    maintenance_runtime_on_falling_edge(&mrt, pre_renew + 200UL);
    maintenance_runtime_on_falling_edge(&mrt, pre_renew + 400UL);

    /* RENEW（再次 ENTER）*/
    maintenance_runtime_ms_t renew_time = 2020UL + MRT_DEFAULT_LEASE_MS / 2;
    maintenance_runtime_on_falling_edge(&mrt, renew_time);
    maintenance_runtime_on_falling_edge(&mrt, renew_time + 900UL);
    maintenance_runtime_on_falling_edge(&mrt, renew_time + 980UL);
    maintenance_runtime_on_falling_edge(&mrt, renew_time + 1280UL);
    maintenance_runtime_on_falling_edge(&mrt, renew_time + 1360UL);
    maintenance_runtime_on_falling_edge(&mrt, renew_time + 1440UL);
    maintenance_runtime_on_falling_edge(&mrt, renew_time + 1520UL);
    event = maintenance_runtime_poll(&mrt, renew_time + 2020UL);
    assert(event == MRT_EVENT_ENTER);

    /* 租约应该从 renew_time + 2020 重新开始 */
    remaining = maintenance_runtime_remaining_ms(&mrt, renew_time + 2020UL);
    assert(remaining == MRT_DEFAULT_LEASE_MS);
}

/* 测试：时间溢出处理 */
TEST(time_overflow)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;
    maintenance_runtime_ms_t start = 0xFFFFF000UL;  /* 接近溢出 */

    maintenance_runtime_init(&mrt, start);

    /* BREAK 跨越溢出点 */
    maintenance_runtime_on_falling_edge(&mrt, start);
    maintenance_runtime_on_falling_edge(&mrt, start + 900UL);  /* 会溢出 */

    /* SYNC */
    maintenance_runtime_on_falling_edge(&mrt, start + 980UL);

    /* DELIMITER */
    maintenance_runtime_on_falling_edge(&mrt, start + 1280UL);

    /* PAYLOAD 1-3 */
    maintenance_runtime_on_falling_edge(&mrt, start + 1360UL);
    maintenance_runtime_on_falling_edge(&mrt, start + 1440UL);
    maintenance_runtime_on_falling_edge(&mrt, start + 1520UL);

    /* Poll 也会跨越溢出 */
    event = maintenance_runtime_poll(&mrt, start + 2020UL);
    assert(event == MRT_EVENT_ENTER);
    assert(maintenance_runtime_is_active(&mrt) == 1U);
}

TEST(emitted_enter_waveform)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;

    maintenance_runtime_init(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);
    maintenance_runtime_on_falling_edge(&mrt, 1030UL);
    maintenance_runtime_on_falling_edge(&mrt, 1310UL);
    maintenance_runtime_on_falling_edge(&mrt, 1450UL);
    maintenance_runtime_on_falling_edge(&mrt, 1570UL);
    maintenance_runtime_on_falling_edge(&mrt, 1690UL);

    event = maintenance_runtime_poll(&mrt, 2189UL);
    assert(event == MRT_EVENT_NONE);
    event = maintenance_runtime_poll(&mrt, 2190UL);
    assert(event == MRT_EVENT_ENTER);
}

/* trailer 后第一个普通下降沿先到达时，事件仍必须被锁存。 */
TEST(trailer_edge_latches_enter)
{
    maintenance_runtime_t mrt;
    maintenance_runtime_event_t event;

    maintenance_runtime_init(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 0UL);
    maintenance_runtime_on_falling_edge(&mrt, 900UL);
    maintenance_runtime_on_falling_edge(&mrt, 1030UL);
    maintenance_runtime_on_falling_edge(&mrt, 1310UL);
    maintenance_runtime_on_falling_edge(&mrt, 1450UL);
    maintenance_runtime_on_falling_edge(&mrt, 1570UL);
    maintenance_runtime_on_falling_edge(&mrt, 1690UL);

    /* 模拟主循环被 UART 输出占用，先收到 600 ms 后的普通下降沿。 */
    maintenance_runtime_on_falling_edge(&mrt, 2290UL);
    event = maintenance_runtime_poll(&mrt, 2290UL);
    assert(event == MRT_EVENT_ENTER);
    assert(maintenance_runtime_mode(&mrt) == MRT_MODE_MAINTENANCE);
}

/* 主函数 */
int main(void)
{
    printf("=== Maintenance Runtime Test Suite ===\n\n");

    RUN_TEST(init);
    RUN_TEST(enter_command);
    RUN_TEST(exit_command);
    RUN_TEST(lease_expiry);
    RUN_TEST(normal_heartbeat_ignored);
    RUN_TEST(break_interrupted);
    RUN_TEST(renew_extends_lease);
    RUN_TEST(time_overflow);
    RUN_TEST(emitted_enter_waveform);
    RUN_TEST(trailer_edge_latches_enter);

    printf("\n=== All Tests Passed ===\n");
    return 0;
}
