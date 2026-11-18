#include "virtio_net.h"
#include "arch.h"
#include "kstring.h"

static virtq_mem_t g_rx_qmem;
static virtq_mem_t g_tx_qmem;

#define RX_BUF_SIZE 2048
static uint8_t g_rx_bufs[8][RX_BUF_SIZE];

int virtio_net_init(uint64_t ecam_base, virtio_net_t *net) {
    pci_device_t pci;
    if (pci_find_virtio(ecam_base, PCI_DEV_VIRTIO_NET_TRANS, PCI_DEV_VIRTIO_NET_MOD, &pci) != 0) {
        return -1;
    }
    if (virtio_pci_probe(ecam_base, &pci, &net->vdev) != 0) {
        return -1;
    }
    if (virtio_pci_init_features(&net->vdev, VIRTIO_F_VERSION_1 | VIRTIO_NET_F_MAC) != 0) {
        return -1;
    }

    /* Queue 0 = RX, Queue 1 = TX */
    if (virtio_pci_setup_queue(&net->vdev, 0, &net->rx_q, &g_rx_qmem) != 0) {
        return -1;
    }
    if (virtio_pci_setup_queue(&net->vdev, 1, &net->tx_q, &g_tx_qmem) != 0) {
        return -1;
    }

    /* Read MAC address from device_cfg (bytes 0..5) */
    for (int i = 0; i < 6; i++) {
        net->mac[i] = (net->vdev.device_cfg != 0)
            ? mmio_read8(net->vdev.device_cfg + (uint64_t)i)
            : (uint8_t)(0x52 + i);
    }

    /* Post 8 RX buffers onto Queue 0 */
    uint16_t n_rx = (net->rx_q.size < 8) ? net->rx_q.size : 8;
    for (uint16_t i = 0; i < n_rx; i++) {
        net->rx_q.mem->desc[i].addr  = (uint64_t)(uintptr_t)g_rx_bufs[i];
        net->rx_q.mem->desc[i].len   = RX_BUF_SIZE;
        net->rx_q.mem->desc[i].flags = VRING_DESC_F_WRITE;
        net->rx_q.mem->desc[i].next  = 0;
        net->rx_q.mem->avail.ring[i] = i;
    }
    dma_barrier();
    net->rx_q.mem->avail.idx = n_rx;

    virtio_pci_set_driver_ok(&net->vdev);
    virtio_pci_notify(&net->vdev, &net->rx_q);
    return 0;
}

int virtio_net_send(virtio_net_t *net, const void *frame, uint32_t len) {
    if (!net || !frame || len == 0 || len > 1514) return -1;

    static virtio_net_hdr_t hdr;
    static uint8_t tx_buf[1536];

    memset(&hdr, 0, sizeof(hdr));
    memcpy(tx_buf, frame, len);

    virtq_t *vq = &net->tx_q;
    uint16_t d0 = vq->next_desc % vq->size;
    uint16_t d1 = (uint16_t)((d0 + 1) % vq->size);
    vq->next_desc = (uint16_t)((d0 + 2) % vq->size);

    /* Descriptor 0: 12-byte virtio_net_hdr */
    vq->mem->desc[d0].addr  = (uint64_t)(uintptr_t)&hdr;
    vq->mem->desc[d0].len   = sizeof(hdr);
    vq->mem->desc[d0].flags = VRING_DESC_F_NEXT;
    vq->mem->desc[d0].next  = d1;

    /* Descriptor 1: Ethernet frame */
    vq->mem->desc[d1].addr  = (uint64_t)(uintptr_t)tx_buf;
    vq->mem->desc[d1].len   = len;
    vq->mem->desc[d1].flags = 0;
    vq->mem->desc[d1].next  = 0;

    uint16_t avail_slot = vq->mem->avail.idx % vq->size;
    vq->mem->avail.ring[avail_slot] = d0;
    dma_barrier();
    vq->mem->avail.idx++;

    virtio_pci_notify(&net->vdev, vq);

    for (uint32_t spins = 0; spins < 10000000U; spins++) {
        dma_barrier();
        if (vq->mem->used.idx != vq->last_used_idx) {
            vq->last_used_idx++;
            return 0;
        }
    }
    return -1;
}

int virtio_net_recv(virtio_net_t *net, void *buf, uint32_t max_len) {
    virtq_t *vq = &net->rx_q;
    dma_barrier();
    if (vq->mem->used.idx == vq->last_used_idx) {
        return 0; /* No packet waiting */
    }

    uint16_t used_slot = vq->last_used_idx % vq->size;
    uint16_t desc_id   = (uint16_t)vq->mem->used.ring[used_slot].id;
    uint32_t total_len = vq->mem->used.ring[used_slot].len;
    vq->last_used_idx++;

    uint32_t hdr_sz  = (uint32_t)sizeof(virtio_net_hdr_t);
    int      copied  = 0;
    if (total_len > hdr_sz) {
        uint32_t frame_len = total_len - hdr_sz;
        if (frame_len > max_len) frame_len = max_len;
        memcpy(buf, g_rx_bufs[desc_id] + hdr_sz, frame_len);
        copied = (int)frame_len;
    }

    /* Re-post descriptor to RX available ring */
    uint16_t avail_slot = vq->mem->avail.idx % vq->size;
    vq->mem->avail.ring[avail_slot] = desc_id;
    dma_barrier();
    vq->mem->avail.idx++;
    virtio_pci_notify(&net->vdev, vq);

    return copied;
}
