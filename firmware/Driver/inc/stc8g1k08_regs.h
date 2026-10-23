#ifndef STC8G1K08_REGS_H
#define STC8G1K08_REGS_H

/*
 * STC8G1K08 8 脚正式 RESET 固件的最小寄存器定义。
 *
 * 这里只保留本固件需要的寄存器，避免直接复用通用例程中针对大封装
 * 的 INT2/INT3 引脚宏。Pin1 的 CPU_CHECK 和 Pin3 的 AP-RESET 依据
 * STC8G-cn.pdf 第 20～21 页分别绑定到 P5.4 和 P5.5。
 */

/* 端口和端口模式寄存器 */
sfr P3 = 0xB0;
sfr P3M1 = 0xB1;
sfr P3M0 = 0xB2;
sfr P5 = 0xC8;
sfr P5M1 = 0xC9;
sfr P5M0 = 0xCA;

/* 定时器和中断寄存器 */
sfr TCON = 0x88;
sfr TMOD = 0x89;
sfr TL0 = 0x8A;
sfr TL1 = 0x8B;
sfr TH0 = 0x8C;
sfr TH1 = 0x8D;
sfr AUXR = 0x8E;
sfr INT_CLKO = 0x8F;
sfr PCON = 0x87;
sfr IE = 0xA8;
sfr WDT_CONTR = 0xC1;

/* 串口 1 和端口复用寄存器 */
sfr SCON = 0x98;
sfr SBUF = 0x99;
sfr P_SW1 = 0xA2;

/* Timer0 控制位 */
sbit TF0 = TCON^5;
sbit TR0 = TCON^4;

/* Timer1 控制位 */
sbit TR1 = TCON^6;

/* 中断使能位 */
sbit ET0 = IE^1;
sbit ES = IE^4;
sbit ET1 = IE^3;
sbit EA = IE^7;

/* UART1 发送完成标志 */
sbit RI = SCON^0;
sbit TI = SCON^1;

/* 复位原因标志所在 SFR 不可位寻址，只能按字节掩码读写。 */
#define PCON_POF_MASK 0x10U
#define PCON_LVDF_MASK 0x20U
#define WDT_FLAG_MASK 0x80U

/*
 * 中断优先级寄存器（IP/IPH 等）有意不定义：Timer0 与 INT2 必须保持同一
 * 默认优先级、互不嵌套。INT2 中读取 32 位毫秒计数和链路状态依赖这一点。
 */

/*
 * 板级信号定义：
 * Pin1/P5.4 是 CPU_CHECK 单线链路，由 cpu_check_link 独占配置；
 * Pin3/P5.5 是 AP-RESET 控制脚，低电平通电，高电平复位，持续推挽驱动。
 */
sbit CPU_CHECK = P5^4;
sbit AP_RESET = P5^5;

#endif
