#include "virtio.h"
#include "arch.h"
#include "kstring.h"

/*
 * Walks the PCIe capability linked list (starting at config offset 0x34) to
 * locate VirtIO 1.0 vendor-specific capabilities (cap_id == 0x09).
 * Each capability points to a (BAR, offset) window for:
 *   - common_cfg : device status, feature negotiation, queue setup
 *   - notify_cfg : doorbell MMIO base + notify_off_multiplier
 *   - isr_cfg    : interrupt status
 *   - device_cfg : device-specific config (e.g., disk capacity or MAC address)
 */
int virtio_pci_probe(uint64_t ecam_base, pci_device_t *pci, virtio_pci_dev_t *vdev) {
    vdev->pci            = *pci;
    vdev->common_cfg     = 0;
    vdev->notify_base    = 0;
    vdev->notify_off_mul = 0;
    vdev->isr_cfg        = 0;
    vdev->device_cfg     = 0;

    /* Check PCI Status bit 4 (Capabilities List present) */
    uint16_t status = pci_read16(ecam_base, pci->bus, pci->dev, pci->func, 0x06);
    if ((status & (1U << 4)) == 0) return -1;

    /* Cap iteration at 48 to guard against malformed cyclic capability lists */
    uint8_t cap_ptr = pci_read8(ecam_base, pci->bus, pci->dev, pci->func, 0x34) & 0xFCU;
    for (int iter = 0; iter < 48 && cap_ptr != 0; iter++) {
        uint8_t cap_id   = pci_read8(ecam_base, pci->bus, pci->dev, pci->func, cap_ptr);
        uint8_t cap_next = pci_read8(ecam_base, pci->bus, pci->dev, pci->func, (uint16_t)(cap_ptr + 1)) & 0xFCU;

        if (cap_id == 0x09) {
            uint8_t  cfg_type = pci_read8(ecam_base, pci->bus, pci->dev, pci->func, (uint16_t)(cap_ptr + 3));
            uint8_t  bar_idx  = pci_read8(ecam_base, pci->bus, pci->dev, pci->func, (uint16_t)(cap_ptr + 4));
            uint32_t offset   = pci_read32(ecam_base, pci->bus, pci->dev, pci->func, (uint16_t)(cap_ptr + 8));

            if (bar_idx < 6 && pci->bar[bar_idx] != 0) {
                uint64_t addr = pci->bar[bar_idx] + (uint64_t)offset;
                if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG && vdev->common_cfg == 0) {
                    vdev->common_cfg = addr;
                } else if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG && vdev->notify_base == 0) {
                    vdev->notify_base    = addr;
                    /* Notify cap stores a 32-bit doorbell stride multiplier at +16 */
                    vdev->notify_off_mul = pci_read32(ecam_base, pci->bus, pci->dev, pci->func, (uint16_t)(cap_ptr + 16));
                } else if (cfg_type == VIRTIO_PCI_CAP_ISR_CFG && vdev->isr_cfg == 0) {
                    vdev->isr_cfg = addr;
                } else if (cfg_type == VIRTIO_PCI_CAP_DEVICE_CFG && vdev->device_cfg == 0) {
                    vdev->device_cfg = addr;
                }
            }
        }
        cap_ptr = cap_next;
    }

    if (vdev->common_cfg == 0 || vdev->notify_base == 0) {
        return -1;
    }
    return 0;
}

/*
 * Performs the mandatory VirtIO 1.0 initialization & feature handshake:
 *   1. Reset device (write 0 to status, poll until 0).
 *   2. Set ACKNOWLEDGE | DRIVER.
 *   3. Intersect device features with ONLY the features our driver implements
 *      (never blindly echo features like EVENT_IDX or MRG_RXBUF that we don't handle).
 *   4. Set FEATURES_OK and re-read status to confirm the device accepted them.
 */
int virtio_pci_init_features(virtio_pci_dev_t *vdev, uint64_t driver_features) {
    uint64_t cfg = vdev->common_cfg;

    mmio_write8(cfg + VIRTIO_COMMON_STATUS, 0);
    for (int i = 0; i < 10000; i++) {
        if (mmio_read8(cfg + VIRTIO_COMMON_STATUS) == 0) break;
    }

    mmio_write8(cfg + VIRTIO_COMMON_STATUS, VIRTIO_STATUS_ACK);
    mmio_write8(cfg + VIRTIO_COMMON_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    mmio_write32(cfg + VIRTIO_COMMON_DFSELECT, 0);
    uint32_t dev_lo = mmio_read32(cfg + VIRTIO_COMMON_DF);
    mmio_write32(cfg + VIRTIO_COMMON_DFSELECT, 1);
    uint32_t dev_hi = mmio_read32(cfg + VIRTIO_COMMON_DF);

    uint64_t dev_feat = ((uint64_t)dev_hi << 32) | (uint64_t)dev_lo;
    uint64_t wanted   = (driver_features | VIRTIO_F_VERSION_1) & dev_feat;

    if ((wanted & VIRTIO_F_VERSION_1) == 0) {
        mmio_write8(cfg + VIRTIO_COMMON_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    mmio_write32(cfg + VIRTIO_COMMON_GFSELECT, 0);
    mmio_write32(cfg + VIRTIO_COMMON_GF, (uint32_t)(wanted & 0xFFFFFFFFU));
    mmio_write32(cfg + VIRTIO_COMMON_GFSELECT, 1);
    mmio_write32(cfg + VIRTIO_COMMON_GF, (uint32_t)(wanted >> 32));

    uint8_t st = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;
    mmio_write8(cfg + VIRTIO_COMMON_STATUS, st);
    if ((mmio_read8(cfg + VIRTIO_COMMON_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0) {
        mmio_write8(cfg + VIRTIO_COMMON_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }
    return 0;
}

void virtio_pci_set_driver_ok(virtio_pci_dev_t *vdev) {
    uint64_t cfg = vdev->common_cfg;
    uint8_t st = mmio_read8(cfg + VIRTIO_COMMON_STATUS);
    mmio_write8(cfg + VIRTIO_COMMON_STATUS, (uint8_t)(st | VIRTIO_STATUS_DRIVER_OK));
}

/*
 * Configures one split virtqueue (`q_idx`) using a caller-supplied `virtq_mem_t`.
 * Why write 64-bit physical ring addresses as two 32-bit MMIO stores?
 *   Some PCIe bridges do not support single 64-bit MMIO writes to config regs;
 *   splitting lo/hi 32-bit writes is universally safe per the VirtIO spec.
 */
int virtio_pci_setup_queue(virtio_pci_dev_t *vdev, uint16_t q_idx, virtq_t *vq, virtq_mem_t *mem) {
    uint64_t cfg = vdev->common_cfg;

    mmio_write16(cfg + VIRTIO_COMMON_Q_SELECT, q_idx);
    uint16_t max_q = mmio_read16(cfg + VIRTIO_COMMON_Q_SIZE);
    if (max_q == 0) return -1;

    uint16_t q_size = (max_q > VIRTQ_MAX_SIZE) ? VIRTQ_MAX_SIZE : max_q;
    memset(mem, 0, sizeof(*mem));

    vq->mem           = mem;
    vq->size          = q_size;
    vq->queue_idx     = q_idx;
    vq->notify_off    = mmio_read16(cfg + VIRTIO_COMMON_Q_NOFF);
    vq->last_used_idx = 0;
    vq->next_desc     = 0;

    /* Because UEFI identity-maps RAM, virtual pointers equal physical DMA addresses */
    uint64_t desc_pa  = (uint64_t)(uintptr_t)&mem->desc[0];
    uint64_t avail_pa = (uint64_t)(uintptr_t)&mem->avail;
    uint64_t used_pa  = (uint64_t)(uintptr_t)&mem->used;

    mmio_write16(cfg + VIRTIO_COMMON_Q_SIZE, q_size);

    mmio_write32(cfg + VIRTIO_COMMON_Q_DESCLO,  (uint32_t)(desc_pa & 0xFFFFFFFFU));
    mmio_write32(cfg + VIRTIO_COMMON_Q_DESCHI,  (uint32_t)(desc_pa >> 32));
    mmio_write32(cfg + VIRTIO_COMMON_Q_AVAILLO, (uint32_t)(avail_pa & 0xFFFFFFFFU));
    mmio_write32(cfg + VIRTIO_COMMON_Q_AVAILHI, (uint32_t)(avail_pa >> 32));
    mmio_write32(cfg + VIRTIO_COMMON_Q_USEDLO,  (uint32_t)(used_pa & 0xFFFFFFFFU));
    mmio_write32(cfg + VIRTIO_COMMON_Q_USEDHI,  (uint32_t)(used_pa >> 32));

    dma_barrier();
    mmio_write16(cfg + VIRTIO_COMMON_Q_ENABLE, 1);
    return 0;
}

/* Rings the device doorbell for `vq` after flushing prior ring writes */
void virtio_pci_notify(virtio_pci_dev_t *vdev, virtq_t *vq) {
    dma_barrier();
    uint64_t doorbell = vdev->notify_base + (uint64_t)vq->notify_off * (uint64_t)vdev->notify_off_mul;
    mmio_write16(doorbell, vq->queue_idx);
}
