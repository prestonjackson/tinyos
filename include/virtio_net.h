#ifndef TINYOS_VIRTIO_NET_H
#define TINYOS_VIRTIO_NET_H

#include <stdint.h>
#include "virtio.h"

/* Feature bit 5: Device provides its MAC address in device_cfg[0..5] */
#define VIRTIO_NET_F_MAC (1ULL << 5)

/*
 * VirtIO 1.0 Network Header (12 bytes)
 * Why 12 bytes instead of 10?
 *   Legacy VirtIO used a 10-byte header, but negotiating VIRTIO_F_VERSION_1
 *   adds `uint16_t num_buffers` at the end (12 bytes total). A 10-byte struct
 *   would misalign every Ethernet frame by 2 bytes.
 */
typedef struct {
  uint8_t  flags;
  uint8_t  gso_type;
  uint16_t hdr_len;
  uint16_t gso_size;
  uint16_t csum_start;
  uint16_t csum_offset;
  uint16_t num_buffers;
} __attribute__((packed)) virtio_net_hdr_t;

_Static_assert(sizeof(virtio_net_hdr_t) == 12, "virtio_net_hdr_t must be 12B");

typedef struct {
  virtio_pci_dev_t vdev;
  virtq_t          rx_q; /* Queue 0 is always RX in virtio-net */
  virtq_t          tx_q; /* Queue 1 is always TX in virtio-net */
  uint8_t          mac[6];
} virtio_net_t;

[[nodiscard]] int virtio_net_init(uint64_t ecam_base, virtio_net_t *net);
[[nodiscard]] int virtio_net_send(
    virtio_net_t *net, const void *frame, uint32_t len);
int virtio_net_recv(virtio_net_t *net, void *buf, uint32_t max_len);

#endif /* TINYOS_VIRTIO_NET_H */
