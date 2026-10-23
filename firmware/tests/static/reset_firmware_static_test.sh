#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
firmware_root=$(CDPATH= cd -- "$root/../.." && pwd)

# Maintained inputs only; validation archives and Keil listings are not required.
python3 - "$firmware_root" <<'PY'
import pathlib
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

root = pathlib.Path(sys.argv[1]).resolve()


def require(condition, message):
    if not condition:
        raise SystemExit("RESET 固件静态检查失败: " + message)


def local_path(base, spelling):
    path = (base / spelling.replace("\\", "/")).resolve()
    require(path.is_relative_to(root), "不得引用跨工程文件: " + spelling)
    require(path.is_file(), "缺少文件: " + spelling)
    return path


project = local_path(root, "RVMDK/STC8G1K08-RESET.uvproj")
targets = ET.parse(project).findall("./Targets/Target")
require(len(targets) == 1, "正式工程必须只有一个目标")
target = targets[0]
require(target.findtext("./TargetOption/TargetCommonOption/Device") ==
        "STC8G1K08 Series", "器件不是 STC8G1K08")
require(target.findtext("./TargetOption/Target51/Target51Misc/MemoryModel") ==
        "0", "正式工程必须保持 SMALL/DATA 模式")
expected = {root / name for name in (
    "User/main.c", "Driver/src/heartbeat_monitor.c", "Driver/src/uart.c",
    "Driver/src/reset_controller.c", "Driver/src/cpu_check_link.c")}
files = target.findall("./Groups/Group/Files/File")
paths = [local_path(project.parent, node.findtext("FilePath", "")) for node in files]
require(len(paths) == 5 and set(paths) == expected,
        "正式工程必须只编入五个本工程源码（无历史验证/跨工程源码）")
require(all(node.findtext("FileType") == "1" for node in files),
        "正式源码必须作为 C 文件编译")
require(all(node.findtext(".//IncludeInBuild", "1") == "1" for node in files),
        "正式源码不得从构建中排除")
require(all(node.text == "0" for node in target.findall(".//MemoryModel")),
        "文件级设置不得覆盖 SMALL 模式")
controls = " ".join(node.text or "" for node in target.findall(".//MiscControls"))
require(not re.search(r"\b(?:COMPACT|LARGE|XDATA|IDATA)\b", controls, re.I),
        "编译/链接控制不得覆盖生产内存路径")
require(not any("HOST_TEST" in (node.text or "")
                for node in target.findall(".//Define") + target.findall(".//MiscControls")),
        "正式工程不得启用 host 测试分支")
include_dirs = []
for node in target.findall(".//IncludePath"):
    for spelling in (node.text or "").split(";"):
        if spelling:
            path = (project.parent / spelling.replace("\\", "/")).resolve()
            require(path.is_relative_to(root) and path.is_dir(),
                    "头文件目录必须留在独立 firmware 内: " + spelling)
            include_dirs.append(path)

# Follow local headers too, so a wrapper/symlink cannot import application sources.
source = {}
pending = list(paths)
while pending:
    path = pending.pop()
    if path in source:
        continue
    text = path.read_text()
    source[path] = text
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    for delimiter, spelling in re.findall(r'^\s*#\s*include\s*([<"])([^>"\n]+)', text, re.M):
        if delimiter == "<":
            require(spelling == "stdint.h", "非本地依赖: " + spelling)
            continue
        candidates = [base / spelling for base in [path.parent] + include_dirs]
        matches = [candidate for candidate in candidates if candidate.is_file()]
        require(bool(matches), "无法独立解析头文件: " + spelling)
        included = local_path(matches[0].parent, matches[0].name)
        require(included.suffix == ".h", "生产代码不得 include 其他工程/实现源码")
        pending.append(included)

# Select production branches without compiling C51 syntax or writing build outputs.
active = {}
for path in paths:
    active[path.name] = subprocess.check_output(
        ["gcc", "-E", "-P", "-x", "c"] +
        ["-I" + str(directory) for directory in include_dirs] + [str(path)], text=True)
    require(not re.search(r"\b(?:xdata|idata)\b", active[path.name]),
            path.name + " 不得改为 XDATA/IDATA 生产路径")
print("OK: 独立五源码工程、SMALL/DATA、无生产 xdata/idata")

config = source[root / "User/Config.h"]
macros = dict(re.findall(r'^\s*#\s*define\s+(\w+)\s+([^\n]+)', config, re.M))
for name, value in {
    "MAIN_FOSC": 24000000, "TIMER0_TICK_HZ": 1000, "UART_BAUDRATE": 9600,
    "AP_RESET_ASSERT_LEVEL": 1, "AP_RESET_PULSE_MS": 200,
    "RESET_HEARTBEAT_GRACE_MS": 30000, "HEARTBEAT_MONITOR_TIMEOUT_MS": 5000,
    "RESET_RECOVERY_WINDOW_MS": 120000,
}.items():
    literal = re.sub(r"[uUlL]+$", "", macros.get(name, "").strip())
    require(literal.isdigit() and int(literal) == value, "板级时序/极性配置不符: " + name)
require(re.fullmatch(r'"[^"\n]+"', macros.get("STC8G1K08_FIRMWARE_VERSION", "")),
        "固件版本必须是非空字符串，而不是固定旧版本")
require("STC8G1K08_FIRMWARE_VERSION" in source[root / "User/main.c"],
        "启动横幅必须引用版本宏")
require(re.search(r"protocol=short-frame\s+3/5\s+bytes", source[root / "User/main.c"]),
        "唯一正式协议应声明为 3 字节请求/5 字节应答")
header = source[root / "Driver/inc/cpu_check_link.h"]
api = set(re.findall(r'\b(cpu_check_link_\w+)\s*\([^;{}]*\)\s*;', header))
require(api == {"cpu_check_link_" + name for name in (
    "init", "tick", "on_falling_edge", "poll", "reply")},
    "链路接口必须是当前短帧 API，不得恢复旧事件/维护模式接口")
require(not re.search(r'\bHEARTBEAT_MONITOR_GRACE_MS\b', config), "不得保留未使用的宽限配置")
main_source = active["main.c"]
require(re.search(r'heartbeat_monitor_status\s*\([^;]*\breset_grace_ms\s*\(\s*\)', main_source),
        "开机宽限必须只在 STARTUP 阶段生效（经 reset_grace_ms）")
require(re.search(r'reset_controller_update\s*\([^;]*\b120000UL\b', main_source),
        "复位决策必须传入恢复窗口，未恢复时重复复位")
for name, value in {"NONE": 0, "START": 1, "STOP": 2, "READ": 3}.items():
    require(re.search(r'^\s*#\s*define\s+LINK_COMMAND_' + name +
                      r'\s+' + str(value) + r'U?\b', header, re.M), "短帧命令不符: " + name)
# Host runners check actual frame lengths/CRC and electrical register transitions;
# do not freeze the decoder's bit counts, state names or temporary variables here.
print("OK: 当前唯一 3/5 短帧接口、版本宏和板级时序")

# Call-graph responsibility check for the C51 SMALL overlay boundary.
functions = {}
for filename, text in active.items():
    text = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', '""', text)
    for match in re.finditer(r'\b(\w+)\s*\([^;{}]*\)\s*(?:interrupt\s+\d+\s*)?\{', text):
        name = match.group(1)
        if name in {"if", "for", "while", "switch"}:
            continue
        depth, end = 1, match.end()
        while depth and end < len(text):
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        require(depth == 0 and name not in functions, "无法分析函数: " + name)
        functions[name] = (text[match.end():end - 1], match.group())
calls = {name: set(re.findall(r'\b(\w+)\s*\(', body)) & functions.keys()
         for name, (body, _) in functions.items()}


def reachable(start):
    seen, pending = set(), [start]
    while pending:
        name = pending.pop()
        require(name in functions, "缺少职责入口: " + name)
        if name not in seen:
            seen.add(name)
            pending.extend(calls[name] - seen)
    return seen


main = reachable("main")
timer = reachable("timer0_isr")
edge = reachable("int2_isr")
require(re.search(r'\binterrupt\s+1\b', functions["timer0_isr"][1]), "Timer0 向量必须为 1")
require(re.search(r'\binterrupt\s+10\b', functions["int2_isr"][1]), "INT2 向量必须为 10")
require(not (main & timer or main & edge or timer & edge),
        "main/Timer0/INT2 不得共享函数调用链（SMALL overlay 安全边界）")
require({"cpu_check_link_tick", "line_write"} <= timer,
        "Timer0 必须负责计时和应答线输出")
require({"cpu_check_link_on_falling_edge", "heartbeat_monitor_on_falling_edge"} <= edge,
        "INT2 必须负责边沿解码和已确认心跳记录")
require({"cpu_check_link_poll", "cpu_check_link_reply", "reset_controller_update",
         "apply_reset_output", "wdt_clear"} <= main,
        "主循环必须负责命令执行/应答准备、复位决策和喂狗")
require(not any(name.startswith("uart1_") for name in timer | edge), "ISR 不得阻塞输出 UART")
require(not re.search(r'\binterrupt\s+4\b|\bES\s*=\s*1\b', active["uart.c"]),
        "正式 UART 只用于诊断，不得恢复 UART RX 协议")
for name, (body, _) in functions.items():
    for match in re.finditer(r'\bcpu_check_link_(?:poll|reply)\s*\(', body):
        writes = re.findall(r'\bEA\s*=\s*([^;]+);', body[:match.start()])
        require(writes and writes[-1].strip() in {"0", "0U"},
                name + " 的命令交接/应答准备必须关中断")
print("OK: Timer0/INT2/main 分工、无共享调用链、命令交接临界区")

regs = source[root / "Driver/inc/stc8g1k08_regs.h"]
for signal, pin in (("CPU_CHECK", 4), ("AP_RESET", 5)):
    require(re.search(r'\bsbit\s+' + signal + r'\s*=\s*P5\s*\^\s*' + str(pin) + r'\s*;', regs),
            "板级引脚绑定错误: " + signal)
require(not re.search(r'\bsbit\s+(?:PCON_POF|PCON_LVDF|WDT_FLAG)\s*=', regs),
        "复位原因 SFR 不可声明为可位寻址")
# INT2 reads the 32-bit tick and link state without masking Timer0.
priority = r'(?:IP|IPH|IP2|IP2H|PT0|PT1|PX0|PX1|PS)'
require(not re.search(r'\b(?:sfr|sbit)\s+' + priority + r'\b', regs),
        "不得定义中断优先级寄存器：Timer0 与 INT2 必须同级不嵌套")
for filename, text in active.items():
    require(not re.search(r'\b' + priority + r'\s*[|&^]?=(?!=)', text),
            filename + " 不得修改中断优先级")
for name, (body, _) in functions.items():
    if re.search(r'\bP5(?:M[01])?\s*[|&]?=\s*[^;]*\b0x10U?\b', body):
        require(name in {"line_write", "cpu_check_link_init"},
                "P5.4 只能由 cpu_check_link 配置: " + name)
for name, (body, _) in functions.items():
    if re.search(r'P5M[01]\s*[|&]?=\s*[^;]*\b0x20\b', body):
        require(name == "gpio_board_init", "P5.5 推挽模式只能初始化，运行中不得释放")
    if re.search(r'\bP5\s*[|&]?=\s*[^;]*\b0x20\b', body):
        require(name in {"ap_reset_release", "ap_reset_assert"},
                "P5.5 电平只能经 ap_reset_release/assert 改变: " + name)
for name in ("ap_reset_release", "ap_reset_assert"):
    callers = {caller for caller, called in calls.items() if name in called}
    require(callers <= {"gpio_board_init", "apply_reset_output"},
            "运行期 P5.5 只能经 apply_reset_output 改变: " + ", ".join(sorted(callers)))
line = functions["line_write"][0]
require(not re.search(r'P5M1\s*&=', line), "P5.4 应答不得切为推挽模式")
require(re.search(r'P5M0\s*\|=', line) and re.search(r'P5M0\s*&=', line),
        "P5.4 应答必须可低拉/释放，不能持续驱动高电平")
print("OK: P5.4 低拉高释/P5.5 持续推挽低正常高复位的静态边界")
print("OK: STC8G1K08 RESET 固件静态检查通过（不读取历史生成物）")
PY
