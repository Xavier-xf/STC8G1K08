# STC8G1K08 工作仓库

对外交付只复制 `firmware/`，不需要整个工作仓库。Linux 端是另一份独立的 `stc_watchdog/`，两端不共享源码，也不需要本产品的 `cpu_check/` 接入层。

- [固件入口](firmware/README.md)：源码结构、协议、移植边界及构建方法。
- [烧录说明](firmware/docs/flashing.md)：时钟、ISP 选项和电气条件。
- [验证记录](firmware/docs/verification.md)：当前结果、验证方法与未测项目。
- [历史归档](archive/README.md)：旧工程、旧文档、过程记录及构建产物，只供追溯。
- `资料/`：芯片资料与工具，不属于固件源码依赖。

当前唯一正式工程是 `firmware/RVMDK/STC8G1K08-RESET.uvproj`。版本以 `firmware/User/Config.h` 和实际烧录后的横幅为准，不在本索引重复维护。
