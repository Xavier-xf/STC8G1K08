# STC8G1K08 验证快照

本目录保存早期 SMOKE/MONITOR 阶段的工程、入口源文件和静态检查脚本，仅用于追溯和对比。

这些文件不是生产固件，不属于正式 Keil 构建目标，也不应作为现场烧录输入。

正式固件只有：

- 工程：\`../RVMDK/STC8G1K08-RESET.uvproj\`
- 入口：\`../User/main.c\`
- 输出：\`../RVMDK/list/stc8g1k08_reset.hex\`

验证快照包括：

- \`STC8G1K08-SMOKE.uvproj\` 与 \`Main.c\`
- \`STC8G1K08-MONITOR.uvproj\`、\`.orig\` 与 \`Monitor.c\`
- 对应的 smoke/monitor 静态检查脚本

修改正式固件时不要把本目录文件重新加入生产工程。

旧 Keil list 输出（Monitor 的 `.hex`、`.obj`、`.build_log.htm` 等）保存在 `RVMDK-list-history/`，正式 `RVMDK/list/` 只服务 RESET 构建。原冒烟 README 保存在 `README-smoke.md`，历史验证记录保存在 `验证记录-历史.md`。
