#include "Config.h"

/*
 * STC8G1K08 冒烟固件
 *
 * 目的：
 * 1. 验证实际芯片能够被 ISP 重复下载并稳定运行；
 * 2. 验证 Pin1/P5.4 上的 CPU_CHECK 只作为输入，不会把心跳误判为 STC 复位；
 * 3. 通过 Pin6/P3.1 输出诊断信息，观察 CPU_CHECK 和 Pin3/P5.5 的电平；
 * 4. 在确认 Q23/AP-RESET 极性前，绝不驱动 Pin3/P5.5。
 *
 * 重要硬件选项：
 * P54RST=0 必须在 STC-ISP 中配置，使 Pin1 保持普通 GPIO。
 * 本固件不访问看门狗寄存器，也不包含 AP-RESET 脉冲逻辑。
 */

static volatile unsigned long g_millisecond;

/* 将 Timer0 重装为 1 ms，时钟按 Config.h 的 24 MHz 计算。 */
static void timer0_init(void)
{
    TMOD &= (unsigned char)~0x03; /* Timer0 使用 16 位定时模式。 */
    AUXR |= 0x80;                 /* Timer0 使用 1T 时钟。 */

    TH0 = (unsigned char)(TIMER0_RELOAD >> 8);
    TL0 = (unsigned char)TIMER0_RELOAD;
    TF0 = 0;
    ET0 = 1;
    TR0 = 1;
}

/* Timer1 只作为 UART1 的波特率发生器，不启用 Timer1 中断。 */
static void uart1_init(void)
{
    unsigned long reload;

    /* UART1 固定到 P3.0/RxD、P3.1/TxD，避免复用到 P5.4/P5.5。 */
    P_SW1 &= (unsigned char)~0xC0;

    /* 串口模式 1：8 数据位、1 停止位；冒烟阶段不接收数据。 */
    SCON = 0x40;
    TI = 0;

    reload = UART1_RELOAD;

    TR1 = 0;
    AUXR &= (unsigned char)~0x01; /* UART1 的波特率发生器使用 Timer1。 */
    TMOD &= (unsigned char)~0x40; /* Timer1 作为定时器，不使用外部计数。 */
    TMOD &= (unsigned char)~0x30; /* Timer1 采用 16 位自动重装配置。 */
    AUXR |= 0x40;                 /* Timer1 使用 1T 时钟。 */
    TH1 = (unsigned char)(reload >> 8);
    TL1 = (unsigned char)reload;
    ET1 = 0;
    INT_CLKO &= (unsigned char)~0x02;
    TR1 = 1;
}

/*
 * 只配置本次冒烟所需的端口：
 * - P5.4/CPU_CHECK：纯输入高阻；
 * - P5.5/AP-RESET：纯输入高阻，保持不驱动 Q23；
 * - P3.0：输入，保留 ISP/UART 接收脚；
 * - P3.1：推挽输出，用于诊断串口发送。
 *
 * P54RST=0 不是运行时写寄存器动作，而是必须保存到 ISP 配置中的硬件选项。
 */
static void gpio_safe_init(void)
{
    /* 先切换为纯输入，避免 P5.5 在冒烟阶段成为推挽输出。 */
    P5M1 |= 0x30;
    P5M0 &= (unsigned char)~0x30;
    P5 |= 0x30; /* 输入状态下释放端口锁存值，便于后续切换输出时可控。 */

    /* P3.0 保持输入；P3.1 设置为推挽输出并置为 UART 空闲高电平。 */
    P3M1 |= 0x01;
    P3M0 &= (unsigned char)~0x01;
    P3M1 &= (unsigned char)~0x02;
    P3M0 |= 0x02;
    P3 |= 0x02;
}

static void uart1_putc(char value)
{
    SBUF = value;
    while (!TI) {
        /*
         * 发送期间允许 Timer0 中断继续累加毫秒计数。
         * 这里不调用任何看门狗清零操作。
         */
    }
    TI = 0;
}

static void uart1_puts(const char *text)
{
    while (*text != '\0') {
        uart1_putc(*text++);
    }
}

/* 读取毫秒计数时短暂关闭中断，避免 32 位变量被半更新地读出。 */
static unsigned long millis_snapshot(void)
{
    unsigned long value;

    EA = 0;
    value = g_millisecond;
    EA = 1;
    return value;
}

void timer0_isr(void) interrupt 1
{
    TH0 = (unsigned char)(TIMER0_RELOAD >> 8);
    TL0 = (unsigned char)TIMER0_RELOAD;
    ++g_millisecond;
}

void main(void)
{
    unsigned long last_report;
    unsigned long now;

    gpio_safe_init();
    uart1_init();
    timer0_init();

    EA = 1;

    uart1_puts("\r\nSTC8G1K08 smoke firmware\r\n");
    uart1_puts("P54=CPU_CHECK input, P55=AP-RESET high-z\r\n");
    uart1_puts("P54RST=0, WDT=OFF, output-drive=OFF\r\n");

    last_report = millis_snapshot();

    for (;;) {
        now = millis_snapshot();
        if ((unsigned long)(now - last_report) >= 1000UL) {
            last_report = now;

            uart1_puts("alive P54=");
            uart1_putc(CPU_CHECK ? '1' : '0');
            uart1_puts(" P55=");
            uart1_putc(AP_RESET_TEST ? '1' : '0');
            uart1_puts(" output=high-z\r\n");
        }
    }
}
