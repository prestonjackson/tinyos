#include "net.h"
#include "kstring.h"

/*
 * RFC 1071 Internet Checksum (used by IPv4, ICMP, and TCP/UDP).
 * Sums 16-bit big-endian words with end-around carry, then one's complement.
 */
uint16_t net_checksum(const void *data, uint32_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t sum = 0;

  for (uint32_t i = 0; i + 1 < len; i += 2) {
    sum += ((uint32_t)p[i] << 8) | (uint32_t)p[i + 1];
  }
  if (len & 1U) {
    sum += (uint32_t)p[len - 1] << 8;
  }
  while (sum >> 16) {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  return (uint16_t)(~sum & 0xFFFFU);
}

/*
 * Constructs a complete Ethernet + IPv4 + UDP frame in big-endian order.
 * Why UDP first?
 *   UDP has no connection handshake (unlike TCP), so we can send a real,
 *   Wireshark-valid packet immediately after virtio-net initializes.
 */
uint32_t net_build_udp_frame(
    uint8_t *out_buf,
    const uint8_t src_mac[6],
    const uint8_t dst_mac[6],
    uint32_t src_ip_be,
    uint32_t dst_ip_be,
    uint16_t src_port,
    uint16_t dst_port,
    const void *payload,
    uint16_t payload_len) {
  /* 1. Ethernet header (14 bytes) */
  memcpy(out_buf + 0, dst_mac, 6);
  memcpy(out_buf + 6, src_mac, 6);
  out_buf[12] = 0x08; /* EtherType: IPv4 (0x0800) */
  out_buf[13] = 0x00;

  /* 2. IPv4 header (20 bytes, IHL=5) */
  uint8_t *ip = out_buf + 14;
  uint16_t udp_len = (uint16_t)(8 + payload_len);
  uint16_t ip_len  = (uint16_t)(20 + udp_len);

  memset(ip, 0, 20);
  ip[0] = 0x45; /* Version=4, Header Length=5 (20 bytes) */
  ip[2] = (uint8_t)(ip_len >> 8);
  ip[3] = (uint8_t)(ip_len & 0xFF);
  ip[8] = 64;   /* TTL */
  ip[9] = 17;   /* Protocol: UDP (17) */
  memcpy(ip + 12, &src_ip_be, 4);
  memcpy(ip + 16, &dst_ip_be, 4);

  uint16_t csum = net_checksum(ip, 20);
  ip[10] = (uint8_t)(csum >> 8);
  ip[11] = (uint8_t)(csum & 0xFF);

  /* 3. UDP header (8 bytes) + payload */
  uint8_t *udp = ip + 20;
  udp[0] = (uint8_t)(src_port >> 8);
  udp[1] = (uint8_t)(src_port & 0xFF);
  udp[2] = (uint8_t)(dst_port >> 8);
  udp[3] = (uint8_t)(dst_port & 0xFF);
  udp[4] = (uint8_t)(udp_len >> 8);
  udp[5] = (uint8_t)(udp_len & 0xFF);
  udp[6] = 0; /* IPv4 allows UDP checksum = 0 (omitted) */
  udp[7] = 0;

  if (payload_len > 0 && payload != NULL) {
    memcpy(udp + 8, payload, payload_len);
  }

  return 14U + (uint32_t)ip_len;
}

/* Constructs a 42-byte Ethernet broadcast ARP "who-has" request */
uint32_t net_build_arp_request(
    uint8_t *out_buf,
    const uint8_t src_mac[6],
    uint32_t src_ip_be,
    uint32_t target_ip_be) {
  memset(out_buf + 0, 0xFF, 6); /* Broadcast dst MAC */
  memcpy(out_buf + 6, src_mac, 6);
  out_buf[12] = 0x08; /* EtherType: ARP (0x0806) */
  out_buf[13] = 0x06;

  uint8_t *arp = out_buf + 14;
  arp[0] = 0x00; arp[1] = 0x01; /* HTYPE: Ethernet (1) */
  arp[2] = 0x08; arp[3] = 0x00; /* PTYPE: IPv4 (0x0800) */
  arp[4] = 6;                   /* HLEN: 6-byte MAC */
  arp[5] = 4;                   /* PLEN: 4-byte IPv4 */
  arp[6] = 0x00; arp[7] = 0x01; /* OPER: ARP Request (1) */
  memcpy(arp + 8,  src_mac, 6);
  memcpy(arp + 14, &src_ip_be, 4);
  memset(arp + 18, 0, 6);       /* Target MAC unknown */
  memcpy(arp + 24, &target_ip_be, 4);

  return 42U;
}
