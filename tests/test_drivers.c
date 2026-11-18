#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "arch.h"
#include "uart.h"
#include "fb.h"
#include "pci.h"
#include "virtio.h"
#include "virtio_blk.h"
#include "virtio_net.h"
#include "net.h"

/* --- Mock MMIO Engine --- */
#define MOCK_ECAM_BASE  0x4010000000ULL
#define MOCK_BAR4_BASE  0x8000000000ULL /* Tests 64-bit highmem BAR! */

static uint8_t  g_ecam[4096];
static uint8_t  g_bar4[4096];
static char     g_uart_tx_log[1024];
static int      g_uart_tx_len = 0;
static uint8_t  g_uart_rx_char = 'Z';
static uint32_t g_last_notify_off = 0;
static uint16_t g_last_notify_val = 0xFFFF;
static uint32_t g_df_select = 0;
static uint32_t g_gf_select = 0;
static uint64_t g_gf_written = 0;
static uint16_t g_q_select = 0;

static void reset_mocks(void) {
    memset(g_ecam, 0xFF, sizeof(g_ecam));
    memset(g_bar4, 0, sizeof(g_bar4));
    memset(g_uart_tx_log, 0, sizeof(g_uart_tx_log));
    g_uart_tx_len     = 0;
    g_last_notify_off = 0;
    g_last_notify_val = 0xFFFF;
    g_df_select       = 0;
    g_gf_select       = 0;
    g_gf_written      = 0;
    g_q_select        = 0;
}

static uint8_t *map_mock_addr(uint64_t addr) {
    if (addr >= MOCK_ECAM_BASE && addr < MOCK_ECAM_BASE + sizeof(g_ecam)) {
        return &g_ecam[addr - MOCK_ECAM_BASE];
    }
    if (addr >= MOCK_BAR4_BASE && addr < MOCK_BAR4_BASE + sizeof(g_bar4)) {
        return &g_bar4[addr - MOCK_BAR4_BASE];
    }
    return NULL;
}

uint8_t mock_mmio_read8(uint64_t addr) {
    uint8_t *p = map_mock_addr(addr);
    return p ? *p : 0xFF;
}

uint16_t mock_mmio_read16(uint64_t addr) {
    uint8_t *p = map_mock_addr(addr);
    if (!p) return 0xFFFF;
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}

uint32_t mock_mmio_read32(uint64_t addr) {
    if (addr == PL011_UART0_BASE + 0x18) return 0; /* TX not full, RX not empty */
    if (addr == PL011_UART0_BASE + 0x00) return g_uart_rx_char;
    if (addr == MOCK_BAR4_BASE + VIRTIO_COMMON_DF) {
        return (g_df_select == 0) ? (uint32_t)VIRTIO_NET_F_MAC : 1U; /* bit 32 = VIRTIO_F_VERSION_1 */
    }
    uint8_t *p = map_mock_addr(addr);
    if (!p) return 0xFFFFFFFFU;
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

void mock_mmio_write8(uint64_t addr, uint8_t val) {
    uint8_t *p = map_mock_addr(addr);
    if (p) *p = val;
}

void mock_mmio_write16(uint64_t addr, uint16_t val) {
    if (addr >= MOCK_BAR4_BASE + 0x100 && addr < MOCK_BAR4_BASE + 0x200) {
        g_last_notify_off = (uint32_t)(addr - (MOCK_BAR4_BASE + 0x100));
        g_last_notify_val = val;
        return;
    }
    if (addr == MOCK_BAR4_BASE + VIRTIO_COMMON_Q_SELECT) {
        g_q_select = val;
        /* Set Q_SIZE = 16 and Q_NOFF = q_select */
        uint16_t qsz = 16;
        memcpy(&g_bar4[VIRTIO_COMMON_Q_SIZE], &qsz, 2);
        memcpy(&g_bar4[VIRTIO_COMMON_Q_NOFF], &g_q_select, 2);
    }
    uint8_t *p = map_mock_addr(addr);
    if (p) memcpy(p, &val, 2);
}

void mock_mmio_write32(uint64_t addr, uint32_t val) {
    if (addr == PL011_UART0_BASE + 0x00) {
        if (g_uart_tx_len < (int)sizeof(g_uart_tx_log) - 1) {
            g_uart_tx_log[g_uart_tx_len++] = (char)(val & 0xFF);
        }
        return;
    }
    if (addr >= PL011_UART0_BASE && addr < PL011_UART0_BASE + 0x100) return;
    if (addr == MOCK_BAR4_BASE + VIRTIO_COMMON_DFSELECT) { g_df_select = val; return; }
    if (addr == MOCK_BAR4_BASE + VIRTIO_COMMON_GFSELECT) { g_gf_select = val; return; }
    if (addr == MOCK_BAR4_BASE + VIRTIO_COMMON_GF) {
        if (g_gf_select == 0) g_gf_written = (g_gf_written & ~0xFFFFFFFFULL) | val;
        else                  g_gf_written = (g_gf_written & 0xFFFFFFFFULL) | ((uint64_t)val << 32);
        return;
    }
    uint8_t *p = map_mock_addr(addr);
    if (p) memcpy(p, &val, 4);
}

/* --- Helper to populate a mock VirtIO PCI device at dev=0 --- */
static void setup_mock_virtio_pci(uint16_t dev_id) {
    reset_mocks();
    /* Vendor = 0x1AF4, Device = dev_id */
    uint16_t vid = PCI_VENDOR_VIRTIO;
    uint16_t cmd = 0x0000;
    uint16_t st  = 0x0010; /* Capabilities list bit */
    memcpy(&g_ecam[0x00], &vid, 2);
    memcpy(&g_ecam[0x02], &dev_id, 2);
    memcpy(&g_ecam[0x04], &cmd, 2);
    memcpy(&g_ecam[0x06], &st, 2);

    /* 64-bit BAR4 at offset 0x20 -> MOCK_BAR4_BASE */
    uint32_t bar4_lo = (uint32_t)(MOCK_BAR4_BASE & 0xFFFFFFFFU) | 0x4U;
    uint32_t bar4_hi = (uint32_t)(MOCK_BAR4_BASE >> 32);
    memcpy(&g_ecam[0x20], &bar4_lo, 4);
    memcpy(&g_ecam[0x24], &bar4_hi, 4);

    /* Cap pointer at 0x34 -> 0x40 */
    g_ecam[0x34] = 0x40;

    /* Cap 1 @ 0x40: COMMON_CFG (type=1, bar=4, off=0x000) -> next 0x50 */
    g_ecam[0x40] = 0x09; g_ecam[0x41] = 0x50; g_ecam[0x43] = 1; g_ecam[0x44] = 4;
    uint32_t off0 = 0x000; memcpy(&g_ecam[0x48], &off0, 4);

    /* Cap 2 @ 0x50: NOTIFY_CFG (type=2, bar=4, off=0x100, mul=4) -> next 0x68 */
    g_ecam[0x50] = 0x09; g_ecam[0x51] = 0x68; g_ecam[0x53] = 2; g_ecam[0x54] = 4;
    uint32_t off1 = 0x100, mul = 4;
    memcpy(&g_ecam[0x58], &off1, 4);
    memcpy(&g_ecam[0x60], &mul, 4);

    /* Cap 3 @ 0x68: DEVICE_CFG (type=4, bar=4, off=0x200) -> next 0x00 */
    g_ecam[0x68] = 0x09; g_ecam[0x69] = 0x00; g_ecam[0x6B] = 4; g_ecam[0x6C] = 4;
    uint32_t off2 = 0x200; memcpy(&g_ecam[0x70], &off2, 4);
}

/* --- Tests --- */
static void test_uart(void) {
    reset_mocks();
    uart_init(PL011_UART0_BASE);
    uart_puts("Hi\n");
    assert(strcmp(g_uart_tx_log, "Hi\r\n") == 0);
    g_uart_tx_len = 0;
    memset(g_uart_tx_log, 0, sizeof(g_uart_tx_log));
    uart_put_hex(0xCAFE);
    assert(strcmp(g_uart_tx_log, "0x000000000000CAFE") == 0);
    assert(uart_getc() == 'Z');
    printf("[PASS] test_uart\n");
}

static void test_fb(void) {
    uint32_t pixels[128 * 32]; /* width=64, pitch=128 (tests stride > width!) */
    fb_console_t fb;
    assert(fb_init(&fb, (uint64_t)(uintptr_t)pixels, 64, 32, 128, FB_FMT_RGBX) == 0);
    fb_puts(&fb, "A\nB");
    /* Verify 'A' drew foreground pixels in top-left 8x8 and cursor moved to row 1, col 1 */
    assert(fb.cursor_row == 1 && fb.cursor_col == 1);
    int fg_count = 0;
    uint32_t bg_val = pixels[127]; /* padding pixel */
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (pixels[y * 128 + x] != bg_val) fg_count++;
        }
    }
    assert(fg_count > 10);
    printf("[PASS] test_fb\n");
}

static void test_pci_and_virtio_net(void) {
    setup_mock_virtio_pci(PCI_DEV_VIRTIO_NET_TRANS); /* Transitional 0x1000 */
    /* Mock MAC 52:54:00:12:34:56 at device_cfg (0x200) */
    uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    memcpy(&g_bar4[0x200], mac, 6);

    virtio_net_t net;
    assert(virtio_net_init(MOCK_ECAM_BASE, &net) == 0);
    /* Verify Bus Master + Memory Space enabled */
    uint16_t cmd;
    memcpy(&cmd, &g_ecam[0x04], 2);
    assert((cmd & 0x06) == 0x06);
    /* Verify 64-bit BAR4 decoded properly */
    assert(net.vdev.pci.bar[4] == MOCK_BAR4_BASE);
    /* Verify VIRTIO_F_VERSION_1 + VIRTIO_NET_F_MAC negotiated */
    assert(g_gf_written == (VIRTIO_F_VERSION_1 | VIRTIO_NET_F_MAC));
    /* Verify MAC read */
    assert(memcmp(net.mac, mac, 6) == 0);
    /* Verify RX queue (0) notified */
    assert(g_last_notify_val == 0);

    /* Simulate TX completion by pre-advancing used.idx */
    net.tx_q.mem->used.idx = 1;
    uint8_t dummy[60] = {0};
    assert(virtio_net_send(&net, dummy, 60) == 0);
    /* Verify TX used Queue 1 (offset 1 * mul 4 = 4) and 12-byte virtio_net_hdr */
    assert(g_last_notify_val == 1);
    assert(g_last_notify_off == 4);
    assert(net.tx_q.mem->desc[0].len == 12);
    assert(net.tx_q.mem->desc[1].len == 60);
    printf("[PASS] test_pci_and_virtio_net\n");
}

static void test_virtio_blk(void) {
    setup_mock_virtio_pci(PCI_DEV_VIRTIO_BLK_MOD); /* Modern 0x1042 */
    uint64_t cap = 2048;
    memcpy(&g_bar4[0x200], &cap, 8);

    virtio_blk_t blk;
    assert(virtio_blk_init(MOCK_ECAM_BASE, &blk) == 0);
    assert(blk.capacity_sectors == 2048);
    assert(sizeof(virtio_blk_req_t) == 16);
    printf("[PASS] test_virtio_blk\n");
}

static void test_net_packets(void) {
    uint8_t src_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t src_ip = htonl(0x0A00020FU); /* 10.0.2.15 */
    uint32_t dst_ip = htonl(0x0A000202U); /* 10.0.2.2 */

    uint8_t arp[64];
    uint32_t arp_len = net_build_arp_request(arp, src_mac, src_ip, dst_ip);
    assert(arp_len == 42);
    assert(arp[12] == 0x08 && arp[13] == 0x06); /* ARP */
    assert(arp[20] == 0x00 && arp[21] == 0x01); /* Request */

    uint8_t udp[128];
    const char *msg = "PING";
    uint32_t udp_len = net_build_udp_frame(udp, src_mac, dst_mac, src_ip, dst_ip, 1234, 9000, msg, 4);
    assert(udp_len == 14 + 20 + 8 + 4);
    assert(udp[12] == 0x08 && udp[13] == 0x00); /* IPv4 */
    assert(udp[23] == 17);                      /* UDP */
    assert(net_checksum(udp + 14, 20) == 0);    /* Valid IPv4 header checksum */
    assert(memcmp(udp + 42, "PING", 4) == 0);
    printf("[PASS] test_net_packets\n");
}

int main(void) {
    test_uart();
    test_fb();
    test_pci_and_virtio_net();
    test_virtio_blk();
    test_net_packets();
    printf("All C driver unit tests passed!\n");
    return 0;
}
