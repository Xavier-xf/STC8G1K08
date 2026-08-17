# STC8G1K08 冒烟固件

本目录是 U14（STC8G1K08 8 脚）第一次上板验证使用的独立 Keil C51 工程。它不是最终外部看门狗固件。

目录布局沿用 STC 官方综合例程：`User/` 放应用入口和配置，`Driver/inc/` 放芯片寄存器定义，`RVMDK/` 放 Keil 工程和编译输出。

## 固件边界

- Pin1/P5.4/CPU_CHECK 配置为纯输入高阻，只读取电平。
- Pin3/P5.5/AP-RESET 配置为纯输入高阻，绝不驱动 Q23。
- Pin5/P3.0 保留为 ISP/UART 接收脚。
- Pin6/P3.1 输出 9600 8N1 诊断信息。
- Timer0 产生 1 ms 计时，用于每秒输出一次状态。
- 不访问 WDT_CONTR，不启动软件或硬件看门狗。
- P54RST=0 必须在 STC-ISP 中设置，不能在运行时代码中代替配置。

## 为什么不直接使用随附通用例程

资料/stc8g-8h-lib-demo-code-20220509/ 是 STC 官方发布的 STC8G/STC8H 通用参考，例程注释明确说明其实际示例以 STC8H8K64U 为基础。其 GPIO 模式和 WDT 寄存器定义可以参考，但 库函数/STC8xxxx.H 中的通用 INT2=P3.6 宏不适用于 STC8G1K08 8 脚；官网 STC8G-cn.pdf 第 20～21 页明确 Pin1/P5.4 才是 INT2。因此本工程使用本地最小寄存器头文件，避免把大封装的中断映射带进 8 脚芯片。

## 编译

1. 使用 Keil C51 打开 `RVMDK/STC8G1K08-SMOKE.uvproj`。
2. 确认目标器件显示为 **STC8G1K08 Series**；不要选择带 LED/TOUCH 功能的 **STC8G1K08T Series**。
3. 确认 `RVMDK/list/` 目录存在且可写，再编译生成 `RVMDK/list/stc8g1k08_smoke.hex`。
4. 本仓库不替你编译，也不包含生成的 HEX；由现场 Keil 环境生成。

如果打开工程时仍显示“未找到设备”，说明项目文件没有被保存或打开的不是这份工程；应关闭 Keil，确认 XML 中是 `<Device>STC8G1K08 Series</Device>` 后重新打开。
如果出现 `Cannot write project file` 或 `cannot create command input file .\\list\\Main.__i`，先把整个 `firmware` 目录复制到本地可写路径（例如 `C:\\Kocom\\STC8G1K08\\firmware`）再打开工程。网络共享目录需要具备文件创建、修改和删除权限。

如果本地 Keil 器件数据库确实没有 `STC8G1K08 Series`，可选择兼容的 MCS-51/STC8G 目标，但不要把通用 STC8xxxx.H 替换进来；正常情况下不应走此兼容路径。

## 烧录前 ISP 配置

详见 isp-config-bringup.txt。至少确认：

- 目标型号为 STC8G1K08 8 脚系列，实际封装以实物/BOM 为准；
- P54RST=0；
- IRC/SYSCLK=24 MHz；
- 硬件 WDT 自动启动关闭；
- 代码保护关闭；
- LVD、复位延时和下载接口按现场实际记录。

## 现场验证顺序

1. 断电检查 VDD_3.3V、GND、P3.0/P3.1 下载线和 USB-UART 地线。
2. 上电烧录，确认 STC-ISP 报告下载成功。
3. 连接 P3.1 到 USB-UART 的 RX，9600 8N1，观察启动横幅。
4. 每次复位后应重新出现横幅；连续运行时每秒出现一行 alive。
5. 让 V853 改变 CPU_CHECK，观察 P54=0/1 跟随变化。
6. 用示波器确认 P5.5 和 AP-RESET 没有被本固件主动拉成复位电平。
7. 至少重复下载/断电启动三次，再记录结果到 验证记录.md。

本固件通过后，才能进入 P5.5/Q23 电气链路验证。不要在这个固件阶段人为停止心跳来测试复位，因为本固件尚未实现超时状态机。
