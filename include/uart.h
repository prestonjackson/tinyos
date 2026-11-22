#ifndef TINYOS_UART_H
#define TINYOS_UART_H

#include <stdint.h>

/*
 * ARM PL011 UART0 base address on QEMU `virt` machine.
 * Why PL011 instead of x86 COM1 (0x3F8)?
 *   ARM64 has no x86 I/O port instructions (`inb`/`outb`); peripherals are
 *   memory-mapped. QEMU `virt` (and ARM64 cloud VMs) place a PL011 serial
 *   controller at 0x09000000, which QEMU connects to `-serial stdio`.
 */
#define PL011_UART0_BASE 0x09000000ULL

void uart_init(uint64_t base);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_hex(uint64_t val);
void uart_put_dec(uint64_t val);
int  uart_rx_ready(void);
char uart_getc(void);

#endif /* TINYOS_UART_H */
