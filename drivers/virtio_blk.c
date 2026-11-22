#include "virtio_blk.h"
#include "arch.h"

/* Static 4 KB-aligned ring memory (avoids needing a dynamic page allocator yet) */
static virtq_mem_t g_blk_qmem;

int virtio_blk_init(uint64_t ecam_base, virtio_blk_t *blk) {
    pci_device_t pci;
    if (pci_find_virtio(ecam_base, PCI_DEV_VIRTIO_BLK_TRANS, PCI_DEV_VIRTIO_BLK_MOD, &pci) != 0) {
        return -1;
    }
    if (virtio_pci_probe(ecam_base, &pci, &blk->vdev) != 0) {
        return -1;
    }
    if (virtio_pci_init_features(&blk->vdev, VIRTIO_F_VERSION_1) != 0) {
        return -1;
    }
    if (virtio_pci_setup_queue(&blk->vdev, 0, &blk->vq, &g_blk_qmem) != 0) {
        return -1;
    }

    /* Device capacity (in 512-byte sectors) sits at device_cfg + 0 (64-bit) */
    if (blk->vdev.device_cfg != 0) {
        uint32_t cap_lo = mmio_read32(blk->vdev.device_cfg + 0);
        uint32_t cap_hi = mmio_read32(blk->vdev.device_cfg + 4);
        blk->capacity_sectors = ((uint64_t)cap_hi << 32) | (uint64_t)cap_lo;
    } else {
        blk->capacity_sectors = 0;
    }

    virtio_pci_set_driver_ok(&blk->vdev);
    return 0;
}

/*
 * Performs a synchronous sector read or write using a 3-descriptor chain:
 *   desc[0]: 16-byte virtio_blk_req_t (type + sector, device-readable)
 *   desc[1]: `count * 512` byte data buffer (writable by device on read)
 *   desc[2]: 1-byte status byte (device writes 0 on success)
 * Why poll `used.idx` instead of using interrupts?
 *   Polling keeps the driver ~50 lines and avoids GIC interrupt controller
 *   setup during early OS bring-up.
 */
int virtio_blk_rw(virtio_blk_t *blk, uint64_t sector, void *buf, uint32_t count, int is_write) {
    if (!blk || !buf || count == 0) return -1;

    static virtio_blk_req_t req;
    static volatile uint8_t status;

    req.type     = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    req.reserved = 0;
    req.sector   = sector;
    status       = 0xFF;

    virtq_t *vq = &blk->vq;
    uint16_t d0 = vq->next_desc % vq->size;
    uint16_t d1 = (uint16_t)((d0 + 1) % vq->size);
    uint16_t d2 = (uint16_t)((d0 + 2) % vq->size);
    vq->next_desc = (uint16_t)((d0 + 3) % vq->size);

    vq->mem->desc[d0].addr  = (uint64_t)(uintptr_t)&req;
    vq->mem->desc[d0].len   = sizeof(req);
    vq->mem->desc[d0].flags = VRING_DESC_F_NEXT;
    vq->mem->desc[d0].next  = d1;

    vq->mem->desc[d1].addr  = (uint64_t)(uintptr_t)buf;
    vq->mem->desc[d1].len   = count * 512U;
    vq->mem->desc[d1].flags = (uint16_t)(VRING_DESC_F_NEXT | (is_write ? 0 : VRING_DESC_F_WRITE));
    vq->mem->desc[d1].next  = d2;

    vq->mem->desc[d2].addr  = (uint64_t)(uintptr_t)&status;
    vq->mem->desc[d2].len   = 1;
    vq->mem->desc[d2].flags = VRING_DESC_F_WRITE;
    vq->mem->desc[d2].next  = 0;

    uint16_t avail_slot = vq->mem->avail.idx % vq->size;
    vq->mem->avail.ring[avail_slot] = d0;
    dma_barrier();
    vq->mem->avail.idx++;

    virtio_pci_notify(&blk->vdev, vq);

    for (uint32_t spins = 0; spins < 10000000U; spins++) {
        dma_barrier();
        if (vq->mem->used.idx != vq->last_used_idx) {
            vq->last_used_idx++;
            return (status == 0) ? 0 : -1;
        }
    }
    return -1;
}
