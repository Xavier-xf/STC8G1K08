#include "Config.h"
#include "uart.h"

/* Keep the ISR ring out of the STC8G1K08 256-byte internal DATA RAM. */
static volatile unsigned char xdata g_uart_rx_buffer[UART1_RX_BUFFER_SIZE];
static volatile unsigned char g_uart_rx_head;
static volatile unsigned char g_uart_rx_tail;
static volatile unsigned char g_uart_rx_overflow;
static volatile unsigned char g_uart_tx_done;

static unsigned char uart1_next_index(unsigned char index)
{
    ++index;
    if (index >= UART1_RX_BUFFER_SIZE) index = 0U;
    return index;
}

void uart1_init(void)
{
    unsigned long reload;

    P_SW1 &= (unsigned char)~0xC0;
    SCON = 0x50;
    TI = 0;
    RI = 0;
    g_uart_rx_head = 0U;
    g_uart_rx_tail = 0U;
    g_uart_rx_overflow = 0U;
    g_uart_tx_done = 1U;
    reload = UART1_RELOAD;

    TR1 = 0;
    AUXR &= (unsigned char)~0x01;
    TMOD &= (unsigned char)~0x40;
    TMOD &= (unsigned char)~0x30;
    AUXR |= 0x40;
    INT_CLKO &= (unsigned char)~0x02;
    TH1 = (unsigned char)(reload >> 8);
    TL1 = (unsigned char)reload;
    ET1 = 0;
    TR1 = 1;
    ES = 1;
}

void uart1_isr(void) interrupt 4
{
    unsigned char next;

    if (RI != 0) {
        RI = 0;
        next = uart1_next_index(g_uart_rx_head);
        if (next == g_uart_rx_tail) {
            g_uart_rx_overflow = 1U;
        } else {
            g_uart_rx_buffer[g_uart_rx_head] = SBUF;
            g_uart_rx_head = next;
        }
    }
    if (TI != 0) {
        TI = 0;
        g_uart_tx_done = 1U;
    }
}

void uart1_putc(char value)
{
    g_uart_tx_done = 0U;
    SBUF = value;
    while (g_uart_tx_done == 0U) {
        /* UART1 ISR marks the byte complete and continues receiving bytes. */
    }
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

unsigned char uart1_read_byte(unsigned char *value)
{
    unsigned char saved_ea;

    if (value == 0) return 0U;
    saved_ea = EA;
    EA = 0;
    if (g_uart_rx_head == g_uart_rx_tail) {
        EA = saved_ea;
        return 0U;
    }
    *value = g_uart_rx_buffer[g_uart_rx_tail];
    g_uart_rx_tail = uart1_next_index(g_uart_rx_tail);
    EA = saved_ea;
    return 1U;
}

unsigned char uart1_rx_overflow_take(void)
{
    unsigned char saved_ea;
    unsigned char overflow;

    saved_ea = EA;
    EA = 0;
    overflow = g_uart_rx_overflow;
    g_uart_rx_overflow = 0U;
    EA = saved_ea;
    return overflow;
}
