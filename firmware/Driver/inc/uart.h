#ifndef STC8G1K08_UART_H
#define STC8G1K08_UART_H

void uart1_init(void);
void uart1_putc(char value);
void uart1_puts(const char *text);
void uart1_put_u32(unsigned long value);

#endif
