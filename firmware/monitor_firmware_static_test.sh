#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
monitor="$root/User/Monitor.c"
config="$root/User/Config.h"
regs="$root/Driver/inc/stc8g1k08_regs.h"
project="$root/RVMDK/STC8G1K08-MONITOR.uvproj"
logic="$root/Driver/src/heartbeat_monitor.c"

fail() {
    printf '%s\n' "监控固件静态检查失败: $1" >&2
    exit 1
}

[ -f "$monitor" ] || fail "缺少 User/Monitor.c"
[ -f "$config" ] || fail "缺少 User/Config.h"
[ -f "$regs" ] || fail "缺少 Driver/inc/stc8g1k08_regs.h"
[ -f "$logic" ] || fail "缺少 Driver/src/heartbeat_monitor.c"
[ -f "$project" ] || fail "缺少独立 MONITOR Keil 工程"

grep -Fq '<Device>STC8G1K08 Series</Device>' "$project" || fail "Keil 工程器件必须是 STC8G1K08 Series"
grep -Fq '<OutputName>stc8g1k08_monitor</OutputName>' "$project" || fail "Keil 工程输出名不正确"
grep -Fq '<FilePath>..\User\Monitor.c</FilePath>' "$project" || fail "Keil 工程未引用 User/Monitor.c"
grep -Fq '<FilePath>..\Driver\src\heartbeat_monitor.c</FilePath>' "$project" || fail "Keil 工程未引用状态机实现"
grep -Fq '<IncludePath>..\User;..\Driver\inc</IncludePath>' "$project" || fail "Keil 工程头文件路径不完整"

grep -Fq '#define HEARTBEAT_MONITOR_GRACE_MS 10000UL' "$config" || fail "缺少 10 秒启动宽限"
grep -Fq '#define HEARTBEAT_MONITOR_TIMEOUT_MS 1000UL' "$config" || fail "缺少 1 秒心跳超时"
grep -Fq '#define INT2_ENABLE_MASK 0x10' "$config" || fail "缺少 INT2 使能位定义"
grep -Fq 'P5M1 |= 0x30' "$monitor" || fail "P5.4/P5.5 未配置为高阻输入"
grep -Fq 'P5M0 &= (unsigned char)~0x30' "$monitor" || fail "P5.4/P5.5 未清除输出模式"
grep -Fq 'INT_CLKO |= INT2_ENABLE_MASK' "$monitor" || fail "未启用 P5.4 的 INT2 下降沿中断"
grep -Fq 'void int2_isr(void) interrupt 10' "$monitor" || fail "未注册 INT2 中断服务程序"
grep -Fq 'heartbeat_monitor_on_falling_edge' "$monitor" || fail "INT2 未记录 CPU_CHECK 边沿"
grep -Fq 'heartbeat_monitor_status' "$monitor" || fail "未输出心跳状态"
grep -Fq 'output=high-z' "$monitor" || fail "未声明 P5.5 高阻诊断状态"
grep -Fq 'WDT_CONTR' "$monitor" && fail "监控阶段不应访问 WDT_CONTR"
grep -Fq 'AP_RESET_TEST =' "$monitor" && fail "监控阶段不应驱动 P5.5"

printf '%s\n' 'OK: STC8G1K08 监控固件静态检查通过'
