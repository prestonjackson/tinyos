#ifndef TINYOS_PCI_H
#define TINYOS_PCI_H

#include <stdint.h>

/*
 * PCIe ECAM (Enhanced Configuration Access Mechanism)
 * ---------------------------------------------------
 * Why ECAM (0x4010000000) instead of x86 ports 0xCF8/0xCFC?
 *   ARM64 uses memory-mapped PCIe configuration space. On QEMU `-M virt`,
 *   ECAM starts at 0x4010000000, where each (bus, dev, func, reg) maps
 *   linearly to a 4 KB memory window.
 *
 * Why match both transitional (0x1000/0x1001) and modern (0x1041/0x1042) IDs?
 *   QEMU's default `-device virtio-blk-pci` and `-device virtio-net-pci`
 *   expose transitional PCI Device IDs (0x1001, 0x1000), while `disable-legacy`
 *   or cloud hypervisors expose modern IDs (0x1042, 0x1041). Both support the
 *   VirtIO 1.0 PCI capability transport we use.
 */
#define QEMU_VIRT_ECAM_BASE 0x4010000000ULL

#define PCI_VENDOR_VIRTIO 0x1AF4

#define PCI_DEV_VIRTIO_NET_TRANS 0x1000
#define PCI_DEV_VIRTIO_BLK_TRANS 0x1001
#define PCI_DEV_VIRTIO_NET_MOD   0x1041
#define PCI_DEV_VIRTIO_BLK_MOD   0x1042

typedef struct {
  uint8_t  bus;
  uint8_t  dev;
  uint8_t  func;
  uint16_t vendor_id;
  uint16_t device_id;
  uint64_t bar[6]; /* 64-bit: EDK2 places prefetchable BARs above 4 GB */
} pci_device_t;

uint8_t  pci_read8(
    uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off);
uint16_t pci_read16(
    uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off);
uint32_t pci_read32(
    uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func, uint16_t off);
void     pci_write16(
    uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func,
    uint16_t off, uint16_t val);
void     pci_write32(
    uint64_t ecam_base, uint8_t bus, uint8_t dev, uint8_t func,
    uint16_t off, uint32_t val);

/*
 * Scans bus 0 for a matching VirtIO device, enables Memory Space + Bus Master
 * in the PCI Command register, and decodes all 32-bit/64-bit BARs.
 * Returns 0 on success, -1 if not found.
 */
[[nodiscard]] int pci_find_virtio(
    uint64_t ecam_base, uint16_t trans_id, uint16_t mod_id, pci_device_t* out);

#endif /* TINYOS_PCI_H */
