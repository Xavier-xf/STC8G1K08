#ifndef STC8G1K08_UART_H
#define STC8G1K08_UART_H

#define UART1_RX_BUFFER_SIZE 64U

void uart1_init(void);
void uart1_putc(char value);
void uart1_puts(const char *text);
void uart1_put_u32(unsigned long value);
unsigned char uart1_read_byte(unsigned char *value);
unsigned char uart1_rx_overflow_take(void);

#endif
