#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reset="$root/User/main.c"
config="$root/User/Config.h"
regs="$root/Driver/inc/stc8g1k08_regs.h"
project="$root/RVMDK/STC8G1K08-RESET.uvproj"
logic="$root/Driver/src/reset_controller.c"

fail() {
    printf '%s\n' "RESET 固件静态检查失败: $1" >&2
    exit 1
}

[ -f "$reset" ] || fail "缺少 User/main.c"
[ -f "$config" ] || fail "缺少 User/Config.h"
[ -f "$regs" ] || fail "缺少 Driver/inc/stc8g1k08_regs.h"
[ -f "$logic" ] || fail "缺少 reset_controller.c"
[ -f "$project" ] || fail "缺少独立 RESET Keil 工程"

grep -Fq '<Device>STC8G1K08 Series</Device>' "$project" || fail "器件不是 STC8G1K08 Series"
grep -Fq '<OutputName>stc8g1k08_reset</OutputName>' "$project" || fail "输出名不正确"
grep -Fq '<FilePath>..\User\main.c</FilePath>' "$project" || fail "工程未引用 main.c"
grep -Fq '<FilePath>..\Driver\src\heartbeat_monitor.c</FilePath>' "$project" || fail "工程未引用心跳状态模块"
grep -Fq '<FilePath>..\Driver\src\reset_controller.c</FilePath>' "$project" || fail "工程未引用复位状态模块"

grep -Fq '#define AP_RESET_ASSERT_LEVEL 0U' "$config" || fail "缺少复位极性配置"
grep -Fq '#define STC8G1K08_FIRMWARE_VERSION "1.0.0"' "$config" || fail "缺少固件版本标识"
grep -Fq 'STC8G1K08 reset firmware v' "$reset" || fail "启动横幅未包含固件版本"
[ ! -e "$root/RVMDK/STC8G1K08-SMOKE.uvproj" ] || fail "正式工程目录仍包含 SMOKE 工程"
[ ! -e "$root/RVMDK/STC8G1K08-MONITOR.uvproj" ] || fail "正式工程目录仍包含 MONITOR 工程"
grep -Fq '#define AP_RESET_PULSE_MS 200UL' "$config" || fail "缺少复位脉宽配置"
grep -Fq '#define RESET_HEARTBEAT_GRACE_MS 30000UL' "$config" || fail "RESET 启动宽限不足"
grep -Fq '#define WDT_PRESCALE 6U' "$config" || fail "WDT 分频不是 128"
grep -Fq 'sfr WDT_CONTR = 0xC1' "$regs" || fail "缺少 WDT_CONTR SFR"
grep -Fq 'P5M1 |= P5_AP_RESET_MASK' "$reset" || fail "P5.5 没有高阻释放路径"
grep -Fq 'P5M0 |= P5_AP_RESET_MASK' "$reset" || fail "P5.5 没有复位推挽路径"
grep -Fq 'WDT_CONTR = (unsigned char)(WDT_PRESCALE | WDT_ENABLE_MASK)' "$reset" || fail "没有启动内部 WDT"
grep -Fq 'WDT_CONTR |= WDT_CLEAR_MASK' "$reset" || fail "没有清除内部 WDT"
grep -Fq 'g_reset_output_active = 0U' "$reset" || fail "上电默认输出不是安全状态"
grep -Fq 'reset_controller_output_active' "$reset" || fail "未应用复位状态机输出"
grep -Fq 'void int2_isr(void) interrupt 10' "$reset" || fail "未注册 INT2"

printf '%s\n' 'OK: STC8G1K08 RESET 固件静态检查通过'
