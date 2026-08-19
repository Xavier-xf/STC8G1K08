#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reset="$root/User/main.c"
config="$root/User/Config.h"
regs="$root/Driver/inc/stc8g1k08_regs.h"
project="$root/RVMDK/STC8G1K08-RESET.uvproj"
logic="$root/Driver/src/reset_controller.c"
uart="$root/Driver/src/uart.c"
maintenance="$root/Driver/src/maintenance_controller.c"

fail() {
    printf '%s\n' "RESET 固件静态检查失败: $1" >&2
    exit 1
}

[ -f "$reset" ] || fail "缺少 User/main.c"
[ -f "$config" ] || fail "缺少 User/Config.h"
[ -f "$regs" ] || fail "缺少 Driver/inc/stc8g1k08_regs.h"
[ -f "$logic" ] || fail "缺少 reset_controller.c"
[ -f "$uart" ] || fail "缺少 uart.c"
[ -f "$maintenance" ] || fail "缺少 maintenance_controller.c"
[ -f "$project" ] || fail "缺少独立 RESET Keil 工程"

grep -Fq '<Device>STC8G1K08 Series</Device>' "$project" || fail "器件不是 STC8G1K08 Series"
grep -Fq '<FilePath>..\Driver\src\uart.c</FilePath>' "$project" || fail "工程未引用 UART 模块"
grep -Fq '<FilePath>..\Driver\src\maintenance_controller.c</FilePath>' "$project" || fail "工程未引用维护模块"
grep -Fq '<OutputName>stc8g1k08_reset</OutputName>' "$project" || fail "输出名不正确"
grep -Fq '<FilePath>..\User\main.c</FilePath>' "$project" || fail "工程未引用 main.c"
grep -Fq '<FilePath>..\Driver\src\heartbeat_monitor.c</FilePath>' "$project" || fail "工程未引用心跳状态模块"
grep -Fq '<FilePath>..\Driver\src\reset_controller.c</FilePath>' "$project" || fail "工程未引用复位状态模块"

grep -Fq '#define AP_RESET_ASSERT_LEVEL 0U' "$config" || fail "缺少复位极性配置"
grep -Fq '#define STC8G1K08_FIRMWARE_VERSION "1.0.0"' "$config" || fail "缺少固件版本标识"
grep -Fq 'STC8G1K08 reset firmware v' "$reset" || fail "启动横幅未包含固件版本"
grep -Fq 'sbit RI = SCON^0' "$regs" || fail "缺少 UART 接收标志位"
grep -Fq 'sbit ES = IE^4' "$regs" || fail "缺少 UART 中断使能位"
grep -Fq 'SCON = 0x50' "$uart" || fail "UART 未启用接收"
grep -Fq 'void uart1_isr(void) interrupt 4' "$uart" || fail "未注册 UART1 ISR"
grep -Fq 'ES = 1' "$uart" || fail "未启用 UART1 中断"
grep -Fq 'static volatile unsigned char xdata g_uart_rx_buffer[UART1_RX_BUFFER_SIZE];' "$uart" || fail "UART RX 环形缓冲未放入 XDATA"
[ ! -e "$root/RVMDK/STC8G1K08-SMOKE.uvproj" ] || fail "正式工程目录仍包含 SMOKE 工程"
[ ! -e "$root/RVMDK/STC8G1K08-MONITOR.uvproj" ] || fail "正式工程目录仍包含 MONITOR 工程"
grep -Fq '#define AP_RESET_PULSE_MS 200UL' "$config" || fail "缺少复位脉宽配置"
grep -Fq '#define RESET_HEARTBEAT_GRACE_MS 30000UL' "$config" || fail "RESET 启动宽限不足"
grep -Fq '#define WDT_PRESCALE 6U' "$config" || fail "WDT 分频不是 128"
grep -Fq 'sfr WDT_CONTR = 0xC1' "$regs" || fail "缺少 WDT_CONTR SFR"
grep -Fq 'P5M1 |= P5_AP_RESET_MASK' "$reset" || fail "P5.5 没有高阻释放路径"
grep -Fq 'P5M0 |= P5_AP_RESET_MASK' "$reset" || fail "P5.5 没有复位推挽路径"
grep -Fq 'WDT_CONTR = (unsigned char)(WDT_PRESCALE | WDT_ENABLE_MASK)' "$reset" || fail "没有启动内部 WDT"
grep -Fq '#include "uart.h"' "$reset" || fail "main.c 未接入 UART 模块"
grep -Fq '#include "maintenance_controller.h"' "$reset" || fail "main.c 未接入维护模块"
grep -Fq 'MNT STATUS mode=' "$reset" || fail "缺少按需 STATUS 输出"
grep -Fq '#define COMMAND_LINE_BUFFER_SIZE 16U' "$reset" || fail "维护命令缓冲必须覆盖最长 14 字节命令且避免占满 DATA"
! grep -Fq 'last_report' "$reset" || fail "仍存在周期状态输出"
! grep -Fq 'heartbeat_snapshot(' "$reset" || fail "main.c 不应复制完整心跳结构体"
! grep -Fq 'heartbeat_monitor_t heartbeat;' "$reset" || fail "main.c 不应保留心跳结构体局部副本"
grep -Fq 'heartbeat_status_snapshot' "$reset" || fail "缺少原子心跳状态快照"
grep -Fq 'heartbeat_edge_count_snapshot' "$reset" || fail "缺少原子边沿计数快照"
grep -Fq 'heartbeat_edge_age_snapshot' "$reset" || fail "缺少原子边沿年龄快照"
grep -Fq 'WDT_CONTR |= WDT_CLEAR_MASK' "$reset" || fail "没有清除内部 WDT"
grep -Fq 'g_reset_output_active = 0U' "$reset" || fail "上电默认输出不是安全状态"
grep -Fq 'reset_controller_output_active' "$reset" || fail "未应用复位状态机输出"
grep -Fq 'void int2_isr(void) interrupt 10' "$reset" || fail "未注册 INT2"

printf '%s\n' 'OK: STC8G1K08 RESET 固件静态检查通过'
