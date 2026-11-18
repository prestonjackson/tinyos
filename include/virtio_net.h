#ifndef TINYOS_VIRTIO_NET_H
#define TINYOS_VIRTIO_NET_H

#include <stdint.h>
#include "virtio.h"

#define VIRTIO_NET_F_MAC (1ULL << 5)

/* VirtIO 1.0 network header is always 12 bytes */
typedef struct {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed)) virtio_net_hdr_t;

typedef struct {
    virtio_pci_dev_t vdev;
    virtq_t          rx_q;
    virtq_t          tx_q;
    uint8_t          mac[6];
} virtio_net_t;

int virtio_net_init(uint64_t ecam_base, virtio_net_t *net);
int virtio_net_send(virtio_net_t *net, const void *frame, uint32_t len);
int virtio_net_recv(virtio_net_t *net, void *buf, uint32_t max_len);

#endif /* TINYOS_VIRTIO_NET_H */
