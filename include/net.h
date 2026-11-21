#ifndef TINYOS_NET_H
#define TINYOS_NET_H

#include <stdint.h>

#ifdef htons
#undef htons
#endif
#ifdef htonl
#undef htonl
#endif

static inline uint16_t htons(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static inline uint32_t htonl(uint32_t v) {
    return ((v & 0x000000FFU) << 24) |
           ((v & 0x0000FF00U) << 8)  |
           ((v & 0x00FF0000U) >> 8)  |
           ((v & 0xFF000000U) >> 24);
}

uint16_t net_checksum(const void *data, uint32_t len);

/* Build a complete Ethernet + IPv4 + UDP frame into out_buf. Returns frame length. */
uint32_t net_build_udp_frame(
    uint8_t *out_buf,
    const uint8_t src_mac[6],
    const uint8_t dst_mac[6],
    uint32_t src_ip_be,
    uint32_t dst_ip_be,
    uint16_t src_port,
    uint16_t dst_port,
    const void *payload,
    uint16_t payload_len
);

/* Build a 42-byte Ethernet + ARP request frame into out_buf. Returns 42. */
uint32_t net_build_arp_request(
    uint8_t *out_buf,
    const uint8_t src_mac[6],
    uint32_t src_ip_be,
    uint32_t target_ip_be
);

#endif /* TINYOS_NET_H */
