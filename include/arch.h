#ifndef TINYOS_ARCH_H
#define TINYOS_ARCH_H

#include <stddef.h>
#include <stdint.h>

/*
 * Hardware Abstraction: Memory-Mapped I/O (MMIO) and Memory Barriers
 * -------------------------------------------------------------------
 * Why explicit mmio_read/write helpers instead of C packed structs?
 *   1. On ARM64, hardware registers live in "Device" memory, not normal RAM.
 *      If you access a packed struct, Clang may emit unaligned loads/stores
 *      or merge two 32-bit writes into a single 64-bit `stp`, which can fault
 *      on PCIe/MMIO bridges.
 *   2. Explicit `volatile` width-exact accesses guarantee the CPU emits the
 *      exact 8-, 16-, or 32-bit bus transaction the hardware expects.
 *
 * Why #ifdef TEST_HOST?
 *   When compiling unit tests on a developer's Mac or Linux CI machine, there
 *   is no real PL011 UART or PCIe ECAM at 0x4010000000. Defining TEST_HOST
 *   redirects MMIO calls to mock functions in tests/test_drivers.c and
 *   replaces ARM64's `dmb sy` instruction with a compiler memory fence.
 */
#ifdef TEST_HOST

uint8_t  mock_mmio_read8(uint64_t addr);
uint16_t mock_mmio_read16(uint64_t addr);
uint32_t mock_mmio_read32(uint64_t addr);
void     mock_mmio_write8(uint64_t addr, uint8_t val);
void     mock_mmio_write16(uint64_t addr, uint16_t val);
void     mock_mmio_write32(uint64_t addr, uint32_t val);

static inline uint8_t mmio_read8(uint64_t addr) {
  return mock_mmio_read8(addr);
}

static inline uint16_t mmio_read16(uint64_t addr) {
  return mock_mmio_read16(addr);
}

static inline uint32_t mmio_read32(uint64_t addr) {
  return mock_mmio_read32(addr);
}

static inline void mmio_write8(uint64_t addr, uint8_t val) {
  mock_mmio_write8(addr, val);
}

static inline void mmio_write16(uint64_t addr, uint16_t val) {
  mock_mmio_write16(addr, val);
}

static inline void mmio_write32(uint64_t addr, uint32_t val) {
  mock_mmio_write32(addr, val);
}

static inline void dma_barrier(void) {
  __asm__ volatile("" ::: "memory");
}

#else

static inline uint8_t mmio_read8(uint64_t addr) {
  return *(volatile uint8_t *)(uintptr_t)addr;
}

static inline uint16_t mmio_read16(uint64_t addr) {
  return *(volatile uint16_t *)(uintptr_t)addr;
}

static inline uint32_t mmio_read32(uint64_t addr) {
  return *(volatile uint32_t *)(uintptr_t)addr;
}

static inline void mmio_write8(uint64_t addr, uint8_t val) {
  *(volatile uint8_t *)(uintptr_t)addr = val;
}

static inline void mmio_write16(uint64_t addr, uint16_t val) {
  *(volatile uint16_t *)(uintptr_t)addr = val;
}

static inline void mmio_write32(uint64_t addr, uint32_t val) {
  *(volatile uint32_t *)(uintptr_t)addr = val;
}

/*
 * Full system Data Memory Barrier (`dmb sy`).
 * Why: ARM64 is weakly ordered—the CPU can reorder writes to DMA ring buffers
 * in RAM relative to the MMIO doorbell write that tells QEMU/hardware to read
 * them. `dmb sy` forces all prior RAM writes to become visible to devices
 * before the doorbell write executes.
 */
static inline void dma_barrier(void) {
#if defined(__aarch64__)
  __asm__ volatile("dmb sy" ::: "memory");
#else
  __asm__ volatile("" ::: "memory");
#endif
}

#endif /* TEST_HOST */

#endif /* TINYOS_ARCH_H */
