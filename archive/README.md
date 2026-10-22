# 历史归档

2026-10-22 将历史材料从工作仓库根目录及正式固件目录迁入本目录。移动前后按 SHA-256 核对内容；原路径、归档路径与校验值保存在 `manifest.json`。不删除原始调试证据，不将旧结果视为当前验收。

| 目录或文件 | 内容 |
|---|---|
| `work-notes/` | V1.0.4 说明、维护模式分析、阶段计划、进度与发现记录 |
| `design-notes/` | 旧设计与实施计划 |
| `validation/` | SMOKE/MONITOR 实验工程、备份和旧接口测试 |
| `build-output/` | 归档前 RVMDK/list 的 HEX、M51、对象、列表与构建日志 |
| `ide-state/` | IDE 调试选项与个人窗口布局 |
| `README-previous.md` | 原工作仓库入口说明 |
| `firmware-README-previous.md` | 原固件说明 |
| `verification-history.md` | 原版本混合验证记录 |
| `isp-config-bringup.txt` | 原 ISP 填写表，含已过时的输出方式检查 |

**这里的 HEX 不属于正式交付。** 归档构建日志记录 `data=110.0 xdata=0 code=4487`，与用户当前短帧固件已确认的 `data=112.0 xdata=0 code=4283` 不同；没有证据证明归档二进制对应当前源码，不能直接烧录或用于领导审核的当前构建结果。

历史文件中的相对路径、旧命令及旧极性可能已经失效，保留原文用于追溯，不保证旧脚本可运行。实际开发、测试和烧录只按 `../firmware/README.md` 及其 docs 执行。复制 `firmware/` 给接收方时，不带本目录。
