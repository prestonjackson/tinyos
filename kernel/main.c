#include <stddef.h>
#include "uefi.h"
#include "uart.h"
#include "fb.h"
#include "pci.h"
#include "virtio_blk.h"
#include "virtio_net.h"
#include "net.h"

/* Global kernel state for our console, disk, and network devices */
static fb_console_t g_fb;
static int          g_has_fb = 0;
static virtio_blk_t g_blk;
static int          g_has_blk = 0;
static virtio_net_t g_net;
static int          g_has_net = 0;

/* Mirror kernel output to both the PL011 serial TTY and the GOP framebuffer */
static void kputc(char c) {
    uart_putc(c);
    if (g_has_fb) fb_putc(&g_fb, c);
}

static void kputs(const char *s) {
    uart_puts(s);
    if (g_has_fb) fb_puts(&g_fb, s);
}

static void kput_hex(uint64_t val) {
    static const char hex[] = "0123456789ABCDEF";
    kputs("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        kputc(hex[(val >> shift) & 0xF]);
    }
}

static void kput_dec(uint64_t val) {
    char buf[21];
    int i = 0;
    if (val == 0) {
        kputc('0');
        return;
    }
    while (val > 0) {
        buf[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    while (i > 0) {
        kputc(buf[--i]);
    }
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == *b;
}

/* Sends a broadcast UDP packet to QEMU's user-net gateway (10.0.2.2:9000) */
static void send_demo_udp(void) {
    if (!g_has_net) {
        kputs("[net] No virtio-net device initialized.\n");
        return;
    }
    static uint8_t frame[128];
    const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t src_ip = htonl((10U << 24) | (0U << 16) | (2U << 8) | 15U); /* 10.0.2.15 */
    uint32_t dst_ip = htonl((10U << 24) | (0U << 16) | (2U << 8) | 2U);  /* 10.0.2.2  */
    const char *msg = "Hello from TinyOS!\n";

    uint32_t len = net_build_udp_frame(frame, g_net.mac, bcast, src_ip, dst_ip, 12345, 9000, msg, 19);
    if (virtio_net_send(&g_net, frame, len) == 0) {
        kputs("[net] Sent UDP packet (");
        kput_dec(len);
        kputs(" bytes) to 10.0.2.2:9000\n");
    } else {
        kputs("[net] UDP send failed.\n");
    }
}

/* Sends an ARP "who-has 10.0.2.2" request */
static void send_demo_arp(void) {
    if (!g_has_net) {
        kputs("[net] No virtio-net device initialized.\n");
        return;
    }
    static uint8_t frame[64];
    uint32_t src_ip = htonl((10U << 24) | (0U << 16) | (2U << 8) | 15U);
    uint32_t dst_ip = htonl((10U << 24) | (0U << 16) | (2U << 8) | 2U);

    uint32_t len = net_build_arp_request(frame, g_net.mac, src_ip, dst_ip);
    if (virtio_net_send(&g_net, frame, len) == 0) {
        kputs("[net] Sent ARP request for 10.0.2.2\n");
    } else {
        kputs("[net] ARP send failed.\n");
    }
}

static void read_sector0(void) {
    if (!g_has_blk) {
        kputs("[blk] No virtio-blk device initialized.\n");
        return;
    }
    static uint8_t sec[512];
    if (virtio_blk_rw(&g_blk, 0, sec, 1, 0) == 0) {
        kputs("[blk] Sector 0 read OK. Sig bytes [510..511]: ");
        kput_hex(((uint64_t)sec[510] << 8) | sec[511]);
        kputs("\n");
    } else {
        kputs("[blk] Sector 0 read failed.\n");
    }
}

/*
 * UEFI Application Entry Point (`efi_main`)
 * -----------------------------------------
 * Boot sequence:
 *   1. Disable UEFI's 5-minute watchdog so the VM doesn't reboot while idle.
 *   2. Query GOP framebuffer geometry BEFORE ExitBootServices (since Boot
 *      Services disappear afterward).
 *   3. Call GetMemoryMap() -> ExitBootServices() in a retry loop with ZERO
 *      allocations or ConOut prints in between (any allocation changes MapKey
 *      and causes ExitBootServices to fail).
 *   4. Take over PL011 UART, GOP framebuffer, virtio-blk, and virtio-net.
 *   5. Run a minimal interactive TTY shell.
 */
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, u"Hello from TinyOS UEFI!\r\n");

    /* 1. Capture GOP framebuffer info before ExitBootServices */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    uint64_t fb_base = 0;
    uint32_t fb_w = 0, fb_h = 0, fb_pitch = 0;
    fb_format_t fb_fmt = FB_FMT_BGRX;

    if (SystemTable->BootServices->LocateProtocol(&gop_guid, NULL, (void **)&gop) == EFI_SUCCESS &&
        gop && gop->Mode && gop->Mode->Info &&
        gop->Mode->FrameBufferBase != 0 &&
        gop->Mode->Info->PixelFormat != PixelBltOnly) {
        fb_base  = gop->Mode->FrameBufferBase;
        fb_w     = gop->Mode->Info->HorizontalResolution;
        fb_h     = gop->Mode->Info->VerticalResolution;
        fb_pitch = gop->Mode->Info->PixelsPerScanLine;
        fb_fmt   = (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
                   ? FB_FMT_RGBX : FB_FMT_BGRX;
    }

    /* 2. Exit UEFI Boot Services (static 32 KB buffer avoids heap allocation) */
    static uint8_t mmap_buf[32768];
    EFI_STATUS status = 1;
    for (int attempt = 0; attempt < 4; attempt++) {
        UINTN mmap_size = sizeof(mmap_buf);
        UINTN map_key = 0;
        UINTN desc_size = 0;
        uint32_t desc_ver = 0;

        status = SystemTable->BootServices->GetMemoryMap(
            &mmap_size, (EFI_MEMORY_DESCRIPTOR *)mmap_buf, &map_key, &desc_size, &desc_ver
        );
        if (status != EFI_SUCCESS) continue;

        status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
        if (status == EFI_SUCCESS) break;
    }

    if (status != EFI_SUCCESS) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, u"ExitBootServices failed!\r\n");
        while (1) {}
    }

    /* 3. Initialize TTY (PL011 UART) and Framebuffer ("VGA") console */
    uart_init(PL011_UART0_BASE);
    if (fb_base != 0 && fb_init(&g_fb, fb_base, fb_w, fb_h, fb_pitch, fb_fmt) == 0) {
        g_has_fb = 1;
    }

    kputs("\n====================================\n");
    kputs("  TinyOS ARM64 Kernel Initialized\n");
    kputs("====================================\n");
    if (g_has_fb) {
        kputs("[fb]  GOP framebuffer active: ");
        kput_dec(fb_w); kputs("x"); kput_dec(fb_h); kputs("\n");
    } else {
        kputs("[fb]  No GOP framebuffer (headless)\n");
    }

    /* 4. Initialize PCIe VirtIO Disk & Network drivers */
    if (virtio_blk_init(QEMU_VIRT_ECAM_BASE, &g_blk) == 0) {
        g_has_blk = 1;
        kputs("[blk] virtio-blk ready, capacity: ");
        kput_dec(g_blk.capacity_sectors);
        kputs(" sectors\n");
        read_sector0();
    } else {
        kputs("[blk] virtio-blk not found\n");
    }

    if (virtio_net_init(QEMU_VIRT_ECAM_BASE, &g_net) == 0) {
        g_has_net = 1;
        kputs("[net] virtio-net ready, MAC: ");
        for (int i = 0; i < 6; i++) {
            static const char hex[] = "0123456789ABCDEF";
            kputc(hex[(g_net.mac[i] >> 4) & 0xF]);
            kputc(hex[g_net.mac[i] & 0xF]);
            if (i < 5) kputc(':');
        }
        kputs("\n");
        send_demo_arp();
        send_demo_udp();
    } else {
        kputs("[net] virtio-net not found\n");
    }

    /* 5. Interactive TTY Shell */
    kputs("\nType 'help' for commands.\n> ");
    char line[64];
    int  pos = 0;

    while (1) {
        char c = uart_getc();
        if (c == '\r' || c == '\n') {
            kputs("\n");
            line[pos] = '\0';
            if (str_eq(line, "help")) {
                kputs("Commands: help, disk, send, arp, clear\n");
            } else if (str_eq(line, "disk")) {
                read_sector0();
            } else if (str_eq(line, "send")) {
                send_demo_udp();
            } else if (str_eq(line, "arp")) {
                send_demo_arp();
            } else if (str_eq(line, "clear")) {
                if (g_has_fb) fb_clear(&g_fb);
            } else if (pos > 0) {
                kputs("Unknown command: ");
                kputs(line);
                kputs("\n");
            }
            pos = 0;
            kputs("> ");
        } else if ((c == '\b' || c == 0x7F) && pos > 0) {
            pos--;
            kputs("\b \b");
        } else if (c >= 0x20 && c <= 0x7E && pos < 63) {
            line[pos++] = c;
            kputc(c);
        }
    }
}
