#include "pci.h"
#include "arch.h"

static uint64_t ecam_addr(uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off) {
    return ecam_base |
           ((uint64_t)bus  << 20) |
           ((uint64_t)dev  << 15) |
           ((uint64_t)func << 12) |
           (uint64_t)(off & 0xFFFU);
}

uint8_t pci_read8(uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off) {
    return mmio_read8(ecam_addr(ecam_base, bus, dev, func, off));
}

uint16_t pci_read16(uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off) {
    return mmio_read16(ecam_addr(ecam_base, bus, dev, func, off));
}

uint32_t pci_read32(uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off) {
    return mmio_read32(ecam_addr(ecam_base, bus, dev, func, off));
}

void pci_write16(uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off, uint16_t val) {
    mmio_write16(ecam_addr(ecam_base, bus, dev, func, off), val);
}

void pci_write32(uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off, uint32_t val) {
    mmio_write32(ecam_addr(ecam_base, bus, dev, func, off), val);
}

int pci_find_virtio(uint64_t ecam_base, uint16_t trans_id, uint16_t mod_id, pci_device_t *out) {
    for (uint8_t dev = 0; dev < 32; dev++) {
        uint16_t vendor = pci_read16(ecam_base, 0, dev, 0, 0x00);
        if (vendor == 0xFFFF || vendor != PCI_VENDOR_VIRTIO) continue;

        uint16_t device = pci_read16(ecam_base, 0, dev, 0, 0x02);
        if (device != trans_id && device != mod_id) continue;

        out->bus       = 0;
        out->dev       = dev;
        out->func      = 0;
        out->vendor_id = vendor;
        out->device_id = device;

        /* Enable Memory Space (bit 1) + Bus Master (bit 2) */
        uint16_t cmd = pci_read16(ecam_base, 0, dev, 0, 0x04);
        pci_write16(ecam_base, 0, dev, 0, 0x04, (uint16_t)(cmd | 0x0006U));

        /* Read 6 BARs, handling 64-bit MMIO BARs */
        for (int i = 0; i < 6; i++) {
            out->bar[i] = 0;
        }
        for (int i = 0; i < 6; i++) {
            uint32_t lo = pci_read32(ecam_base, 0, dev, 0, (uint16_t)(0x10 + i * 4));
            if (lo & 0x1U) {
                /* I/O BAR (unused on ARM64 MMIO) */
                out->bar[i] = (uint64_t)(lo & ~0x3U);
            } else if ((lo & 0x6U) == 0x4U && i + 1 < 6) {
                /* 64-bit MMIO BAR */
                uint32_t hi = pci_read32(ecam_base, 0, dev, 0, (uint16_t)(0x10 + (i + 1) * 4));
                out->bar[i] = ((uint64_t)hi << 32) | (uint64_t)(lo & ~0xFULL);
                i++; /* Skip upper half slot */
            } else {
                /* 32-bit MMIO BAR */
                out->bar[i] = (uint64_t)(lo & ~0xFULL);
            }
        }
        return 0;
    }
    return -1;
}
