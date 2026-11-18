#ifndef TINYOS_UART_H
#define TINYOS_UART_H

#include <stdint.h>

#define PL011_UART0_BASE 0x09000000ULL

void uart_init(uint64_t base);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_hex(uint64_t val);
void uart_put_dec(uint64_t val);
int  uart_rx_ready(void);
char uart_getc(void);

#endif /* TINYOS_UART_H */
