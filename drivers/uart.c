#include "uart.h"
#include "arch.h"

/*
 * ARM PL011 UART Register Offsets
 *   UART_DR   (+0x00): Data Register (write to TX FIFO, read from RX FIFO)
 *   UART_FR   (+0x18): Flag Register (TXFF bit 5 = TX full, RXFE bit 4 = empty)
 *   UART_LCRH (+0x2C): Line Control (8N1 + FIFO enable)
 *   UART_CR   (+0x30): Control Register (UARTEN bit 0, TXE bit 8, RXE bit 9)
 */
#define UART_DR   0x00
#define UART_FR   0x18
#define UART_LCRH 0x2C
#define UART_CR   0x30

#define UART_FR_TXFF (1U << 5)
#define UART_FR_RXFE (1U << 4)

static uint64_t g_uart_base = PL011_UART0_BASE;

/*
 * Configures PL011 for 8N1 with hardware FIFOs enabled.
 * Why skip baud-rate divisor setup?
 *   QEMU's virtual PL011 ignores baud rate and UEFI already clocked it, so
 *   re-enabling 8N1 + FIFOs is sufficient without hardcoding a ref clock.
 */
void uart_init(uint64_t base) {
  g_uart_base = base;
  mmio_write32(g_uart_base + UART_CR, 0);
  mmio_write32(g_uart_base + UART_LCRH, (3U << 5) | (1U << 4));
  mmio_write32(g_uart_base + UART_CR, (1U << 9) | (1U << 8) | (1U << 0));
}

void uart_putc(char c) {
  /* Poll until the hardware transmit FIFO has room for another byte */
  while (mmio_read32(g_uart_base + UART_FR) & UART_FR_TXFF) {}
  mmio_write32(g_uart_base + UART_DR, (uint32_t)(uint8_t)c);
}

void uart_puts(const char* s) {
  while (*s) {
    /* Serial terminals expect CRLF (\r\n) to return to column 0 on newline */
    if (*s == '\n') uart_putc('\r');
    uart_putc(*s++);
  }
}

void uart_put_hex(uint64_t val) {
  static const char hex[] = "0123456789ABCDEF";
  uart_puts("0x");
  for (int shift = 60; shift >= 0; shift -= 4) {
    uart_putc(hex[(val >> shift) & 0xF]);
  }
}

void uart_put_dec(uint64_t val) {
  char buf[21];
  int i = 0;
  if (val == 0) {
    uart_putc('0');
    return;
  }
  while (val > 0) {
    buf[i++] = (char)('0' + (val % 10));
    val /= 10;
  }
  while (i > 0) {
    uart_putc(buf[--i]);
  }
}

bool uart_rx_ready(void) {
  return (mmio_read32(g_uart_base + UART_FR) & UART_FR_RXFE) == 0;
}

char uart_getc(void) {
  while (!uart_rx_ready()) {}
  return (char)(mmio_read32(g_uart_base + UART_DR) & 0xFF);
}
