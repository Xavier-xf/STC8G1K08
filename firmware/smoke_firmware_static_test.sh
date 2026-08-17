#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
main="$root/User/Main.c"
config="$root/User/Config.h"
regs="$root/Driver/inc/stc8g1k08_regs.h"
project="$root/RVMDK/STC8G1K08-SMOKE.uvproj"
list="$root/RVMDK/list"

fail() {
    printf '%s\n' "冒烟固件静态检查失败: $1" >&2
    exit 1
}

[ -f "$main" ] || fail "缺少 User/Main.c"
[ -f "$config" ] || fail "缺少 User/Config.h"
[ -f "$regs" ] || fail "缺少 Driver/inc/stc8g1k08_regs.h"
[ -d "$list" ] || fail "缺少 RVMDK/list 输出目录"
[ -f "$project" ] || fail "缺少 RVMDK/STC8G1K08-SMOKE.uvproj"
grep -Fq '<Device>STC8G1K08 Series</Device>' "$project" || fail "Keil 工程器件必须是 STC8G1K08 Series"
grep -Fq '<Cpu>IRAM(0-0xFF) XRAM(0-0x03FF) IROM(0-0x1FF8) CLOCK(24000000) MODP2</Cpu>' "$project" || fail "Keil 工程存储器配置不匹配 STC8G1K08 8K 器件"

grep -Fq '#include "Config.h"' "$main" || fail "Main.c 未引用 User/Config.h"
grep -Fq '<IncludePath>..\User;..\Driver\inc</IncludePath>' "$project" || fail "Keil 工程未配置 User/Driver/inc 头文件路径"
grep -Fq '<FilePath>..\User\Main.c</FilePath>' "$project" || fail "Keil 工程未引用 User/Main.c"

grep -Fq 'sbit CPU_CHECK = P5^4;' "$regs" || fail "CPU_CHECK 未绑定 P5.4"
grep -Fq 'sbit AP_RESET_TEST = P5^5;' "$regs" || fail "AP-RESET 测试脚未绑定 P5.5"
grep -Fq 'P54RST=0' "$main" || fail "未记录 P54RST=0 前提"
grep -Fq 'P5M1 |= 0x30' "$main" || fail "P5.4/P5.5 未配置为纯输入高阻"
grep -Fq 'P5M0 &= (unsigned char)~0x30' "$main" || fail "P5.4/P5.5 未清除输出模式"
grep -Fq 'WDT_CONTR' "$main" && fail "冒烟固件不应访问 WDT_CONTR"
grep -Fq 'AP_RESET_TEST =' "$main" && fail "冒烟固件不应驱动 P5.5"
grep -Fq 'P54RST = 1' "$main" && fail "冒烟固件不应把 P5.4 配成复位脚"
grep -Fq 'P54RST = 0' "$main" && fail "P54RST 只能由 ISP 选项配置，不应写入运行时代码"
grep -Fq 'P54' "$main" || fail "未采样 P5.4"
grep -Fq 'P55' "$main" || fail "未输出 P5.5 安全状态"

printf '%s\n' 'OK: STC8G1K08 冒烟固件静态检查通过'
