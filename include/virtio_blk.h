#ifndef TINYOS_VIRTIO_BLK_H
#define TINYOS_VIRTIO_BLK_H

#include <stdint.h>
#include "virtio.h"

#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed)) virtio_blk_req_t;

typedef struct {
    virtio_pci_dev_t vdev;
    virtq_t          vq;
    uint64_t         capacity_sectors;
} virtio_blk_t;

int virtio_blk_init(uint64_t ecam_base, virtio_blk_t *blk);
int virtio_blk_rw(virtio_blk_t *blk, uint64_t sector, void *buf, uint32_t count, int is_write);

#endif /* TINYOS_VIRTIO_BLK_H */
