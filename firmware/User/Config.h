#ifndef STC8G1K08_SMOKE_CONFIG_H
#define STC8G1K08_SMOKE_CONFIG_H

/*
 * 冒烟固件的时钟假设必须与 STC-ISP 中的 IRC/SYSCLK 选项一致。
 * 本版按 24 MHz 编译；如果 ISP 使用了其他频率，必须同步修改并重新计算
 * Timer0 的 1 ms 时基和 UART1 的 9600 baud 重载值。
 */
#define MAIN_FOSC 24000000UL
#define UART_BAUDRATE 9600UL
#define TIMER0_TICK_HZ 1000UL

#define TIMER0_RELOAD (65536UL - (MAIN_FOSC / TIMER0_TICK_HZ))
#define UART1_RELOAD (65536UL - ((MAIN_FOSC / 4UL) / UART_BAUDRATE))

#include "stc8g1k08_regs.h"

#endif
