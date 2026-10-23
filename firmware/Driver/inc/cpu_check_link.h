#ifndef STC8G1K08_CPU_CHECK_LINK_H
#define STC8G1K08_CPU_CHECK_LINK_H

/*
 * CPU_CHECK 单线链路：区分主机普通心跳与命令帧，接收 3 字节请求，
 * 由主循环执行后以开漏方式发送 5 字节应答。帧格式与 Linux 端
 * stc_watchdog 协议一致；本模块独占 P5.4 的模式与输出。
 */

#define LINK_COMMAND_NONE 0U
#define LINK_COMMAND_START 1U
#define LINK_COMMAND_STOP 2U
#define LINK_COMMAND_READ 3U
#define LINK_RESULT_OK 0U
#define LINK_RESULT_INVALID 1U

/* 应答状态字节：bit0 启用，bit1 本轮见过心跳，bit2..4 阶段，bit5..7 为 0。 */
#define LINK_STATUS_ENABLED 0x01U
#define LINK_STATUS_HEARTBEAT 0x02U
#define LINK_STATUS_PHASE_SHIFT 2U
#define LINK_PHASE_STOPPED 0U
#define LINK_PHASE_STARTUP 1U
#define LINK_PHASE_MONITORING 2U
#define LINK_PHASE_RESET 3U
#define LINK_PHASE_RECOVERY 4U

/* 以下两个接口仅由 Timer0/INT2 中断调用。 */
void cpu_check_link_tick(void);
/* 返回已确认的上一次正常心跳距今的毫秒数；协议边沿和噪声返回 0。 */
unsigned int cpu_check_link_on_falling_edge(unsigned int now_ms);

/* 以下接口由主循环调用；poll/reply 调用期间必须关闭总中断。 */
void cpu_check_link_init(void);
unsigned char cpu_check_link_poll(void);
void cpu_check_link_reply(unsigned char result, unsigned char status);

#endif
