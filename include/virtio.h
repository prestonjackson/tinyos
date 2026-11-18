#ifndef TINYOS_VIRTIO_H
#define TINYOS_VIRTIO_H

#include <stdint.h>
#include "pci.h"

/* VirtIO Device Status bits */
#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED      128

/* VirtIO Feature bits */
#define VIRTIO_F_VERSION_1 (1ULL << 32)

/* VirtIO PCI Capability cfg_type values */
#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG    3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4

/* VirtIO Common Config register offsets */
#define VIRTIO_COMMON_DFSELECT   0x00
#define VIRTIO_COMMON_DF         0x04
#define VIRTIO_COMMON_GFSELECT   0x08
#define VIRTIO_COMMON_GF         0x0C
#define VIRTIO_COMMON_NUMQ       0x12
#define VIRTIO_COMMON_STATUS     0x14
#define VIRTIO_COMMON_Q_SELECT   0x16
#define VIRTIO_COMMON_Q_SIZE     0x18
#define VIRTIO_COMMON_Q_ENABLE   0x1C
#define VIRTIO_COMMON_Q_NOFF     0x1E
#define VIRTIO_COMMON_Q_DESCLO   0x20
#define VIRTIO_COMMON_Q_DESCHI   0x24
#define VIRTIO_COMMON_Q_AVAILLO  0x28
#define VIRTIO_COMMON_Q_AVAILHI  0x2C
#define VIRTIO_COMMON_Q_USEDLO   0x30
#define VIRTIO_COMMON_Q_USEDHI   0x34

/* Virtqueue descriptor flags */
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define VIRTQ_MAX_SIZE 64

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) vring_desc_t;

typedef struct {
    volatile uint16_t flags;
    volatile uint16_t idx;
    volatile uint16_t ring[VIRTQ_MAX_SIZE];
} __attribute__((packed)) vring_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) vring_used_elem_t;

typedef struct {
    volatile uint16_t flags;
    volatile uint16_t idx;
    volatile vring_used_elem_t ring[VIRTQ_MAX_SIZE];
} __attribute__((packed)) vring_used_t;

/* Single 4096-byte aligned memory block holding one split virtqueue */
typedef struct {
    vring_desc_t  desc[VIRTQ_MAX_SIZE];
    uint8_t       _pad1[4096 - sizeof(vring_desc_t) * VIRTQ_MAX_SIZE];
    vring_avail_t avail;
    uint8_t       _pad2[4096 - sizeof(vring_avail_t)];
    vring_used_t  used;
    uint8_t       _pad3[4096 - sizeof(vring_used_t)];
} __attribute__((aligned(4096))) virtq_mem_t;

typedef struct {
    virtq_mem_t *mem;
    uint16_t     size;
    uint16_t     queue_idx;
    uint16_t     notify_off;
    uint16_t     last_used_idx;
    uint16_t     next_desc;
} virtq_t;

typedef struct {
    pci_device_t pci;
    uint64_t     common_cfg;
    uint64_t     notify_base;
    uint32_t     notify_off_mul;
    uint64_t     isr_cfg;
    uint64_t     device_cfg;
} virtio_pci_dev_t;

int  virtio_pci_probe(uint64_t ecam_base, pci_device_t *pci, virtio_pci_dev_t *vdev);
int  virtio_pci_init_features(virtio_pci_dev_t *vdev, uint64_t driver_features);
void virtio_pci_set_driver_ok(virtio_pci_dev_t *vdev);
int  virtio_pci_setup_queue(virtio_pci_dev_t *vdev, uint16_t q_idx, virtq_t *vq, virtq_mem_t *mem);
void virtio_pci_notify(virtio_pci_dev_t *vdev, virtq_t *vq);

#endif /* TINYOS_VIRTIO_H */
