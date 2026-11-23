#ifndef TINYOS_VIRTIO_BLK_H
#define TINYOS_VIRTIO_BLK_H

#include <stdbool.h>
#include <stdint.h>
#include "virtio.h"

/*
 * VirtIO Block Device Driver
 * --------------------------
 * Why virtio-blk instead of ATA/AHCI?
 *   ARM64 QEMU `virt` and cloud VMs (AWS Nitro, GCP) have no legacy IDE/ATA
 *   ports; virtual disks are exposed as PCIe `virtio-blk` devices.
 */
#define VIRTIO_BLK_T_IN  0 /* Read sector(s) from disk */
#define VIRTIO_BLK_T_OUT 1 /* Write sector(s) to disk */

/* 16-byte request header placed in Descriptor 0 of every block request */
typedef struct {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
} __attribute__((packed)) virtio_blk_req_t;

_Static_assert(sizeof(virtio_blk_req_t) == 16, "virtio_blk_req_t must be 16B");

typedef struct {
  virtio_pci_dev_t vdev;
  virtq_t          vq;
  uint64_t         capacity_sectors; /* Total 512-byte sectors */
} virtio_blk_t;

[[nodiscard]] int virtio_blk_init(uint64_t ecam_base, virtio_blk_t *blk);
[[nodiscard]] int virtio_blk_rw(
    virtio_blk_t *blk, uint64_t sector, void *buf, uint32_t count,
    bool is_write);

#endif /* TINYOS_VIRTIO_BLK_H */
