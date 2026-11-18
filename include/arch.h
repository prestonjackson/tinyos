#ifndef TINYOS_ARCH_H
#define TINYOS_ARCH_H

#include <stdint.h>
#include <stddef.h>

/*
 * Memory-mapped I/O (MMIO) and barrier primitives.
 * Under TEST_HOST, these route to mock functions in tests/test_drivers.c
 * so all drivers can be unit-tested on any host CPU (x86_64 or arm64).
 */
#ifdef TEST_HOST

uint8_t  mock_mmio_read8(uint64_t addr);
uint16_t mock_mmio_read16(uint64_t addr);
uint32_t mock_mmio_read32(uint64_t addr);
void     mock_mmio_write8(uint64_t addr, uint8_t val);
void     mock_mmio_write16(uint64_t addr, uint16_t val);
void     mock_mmio_write32(uint64_t addr, uint32_t val);

static inline uint8_t  mmio_read8(uint64_t addr)  { return mock_mmio_read8(addr); }
static inline uint16_t mmio_read16(uint64_t addr) { return mock_mmio_read16(addr); }
static inline uint32_t mmio_read32(uint64_t addr) { return mock_mmio_read32(addr); }

static inline void mmio_write8(uint64_t addr, uint8_t val)   { mock_mmio_write8(addr, val); }
static inline void mmio_write16(uint64_t addr, uint16_t val) { mock_mmio_write16(addr, val); }
static inline void mmio_write32(uint64_t addr, uint32_t val) { mock_mmio_write32(addr, val); }

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

static inline void dma_barrier(void) {
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

#endif /* TEST_HOST */

#endif /* TINYOS_ARCH_H */
