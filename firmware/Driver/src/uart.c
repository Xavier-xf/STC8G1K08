#include "Config.h"
#include "uart.h"
#if STC8G1K08_UART_LOG_ENABLE

void uart1_init(void)
{
    unsigned long reload;

    P_SW1 &= (unsigned char)~0xC0;
    SCON = 0x40;
    TI = 0;
    reload = UART1_RELOAD;

    TR1 = 0;
    AUXR &= (unsigned char)~0x01;
    TMOD &= (unsigned char)~0x40;
    TMOD &= (unsigned char)~0x30;
    AUXR |= 0x40;
    TH1 = (unsigned char)(reload >> 8);
    TL1 = (unsigned char)reload;
    ET1 = 0;
    INT_CLKO &= (unsigned char)~0x02;
    TR1 = 1;
}

void uart1_putc(char value)
{
    SBUF = value;
    while (!TI) {
    }
    TI = 0;
}

void uart1_puts(const char *text)
{
    while (*text != '\0') {
        uart1_putc(*text++);
    }
}

void uart1_put_u32(unsigned long value)
{
    char digits[10];
    unsigned char count = 0U;

    if (value == 0UL) {
        uart1_putc('0');
        return;
    }
    while (value != 0UL && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    }
    while (count != 0U) {
        uart1_putc(digits[--count]);
    }
}
#endif
