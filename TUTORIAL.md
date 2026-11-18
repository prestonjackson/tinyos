# TinyOS Architecture & Driver Walkthrough

Every file in TinyOS is intentionally small, pure C, and free of external dependencies. Here is how the pieces fit together from power-on to sending a network packet.

---

## 1. Bootstrapping with UEFI (`kernel/main.c`, `include/uefi.h`)
1. QEMU's ARM64 UEFI firmware (`edk2-aarch64-code.fd`) loads `esp/EFI/BOOT/BOOTAA64.EFI` into 64-bit memory and calls `efi_main(ImageHandle, SystemTable)`.
2. Before giving up UEFI services, `efi_main`:
   - Disables the 5-minute UEFI watchdog timer.
   - Locates `EFI_GRAPHICS_OUTPUT_PROTOCOL` (GOP) to grab the linear framebuffer address (`FrameBufferBase`), resolution, scanline pitch (`PixelsPerScanLine`), and pixel format (`BGRX` or `RGBX`).
   - Calls `GetMemoryMap()` + `ExitBootServices()` in a tight retry loop.
3. After `ExitBootServices()` returns `EFI_SUCCESS`, UEFI is gone and TinyOS owns the hardware.

> **Why `-device ramfb` instead of `virtio-gpu-pci`?**
> `virtio-gpu-pci` relies on UEFI's timer to flush pixels to the screen, so it freezes after `ExitBootServices()`. QEMU's `ramfb` exposes a simple linear framebuffer in RAM that keeps updating when your kernel writes to it directly.

---

## 2. TTY Serial Driver (`drivers/uart.c`)
- QEMU `virt` (and ARM64 cloud VMs) expose an **ARM PL011 UART** at `0x09000000`.
- Unlike x86's `outb`/`inb` I/O ports, ARM64 uses **Memory-Mapped I/O (MMIO)** via `mmio_read32` / `mmio_write32` in `include/arch.h`.
- `uart_putc()` waits for the `TXFF` (Transmit FIFO Full) bit in `UART_FR` (`+0x18`) to clear, then writes a byte to `UART_DR` (`+0x00`).

---

## 3. Graphics Console ("VGA" Equivalent) (`drivers/fb.c`)
- Uses the GOP `FrameBufferBase` captured during boot.
- Renders printable ASCII characters (`0x20`–`0x7E`) using a built-in 8×8 monochrome bitmap font (`font8x8`), handling both `BGRX` and `RGBX` pixel formats and scrolling when the cursor reaches the bottom row.

---

## 4. PCIe ECAM & VirtIO 1.0 Transport (`drivers/pci.c`, `drivers/virtio_pci.c`)
- QEMU `virt` maps PCIe configuration space (ECAM) at `0x4010000000`.
- `pci_find_virtio()` scans bus 0 for Vendor `0x1AF4` (matching both transitional `0x1000`/`0x1001` and modern `0x1041`/`0x1042` IDs), enables **Memory Space + Bus Master** in the PCI Command register, and decodes **64-bit BARs** (which EDK2 places in high memory at `0x8000000000+`).
- `virtio_pci_probe()` walks the PCI capability list (`cap_id == 0x09`) to locate `common_cfg`, `notify_cfg` (plus `notify_off_multiplier`), `isr_cfg`, and `device_cfg`.
- `virtio_pci_init_features()` negotiates `VIRTIO_F_VERSION_1` (bit 32) and sets `FEATURES_OK`.
- `virtio_pci_setup_queue()` allocates a 4096-byte-aligned split virtqueue (`desc`, `avail`, `used` rings) and uses `dmb sy` (`dma_barrier()`) before ringing the doorbell.

---

## 5. Disk & Network Drivers (`drivers/virtio_blk.c`, `drivers/virtio_net.c`, `net/net.c`)
- **`virtio_blk.c`**: Submits a 3-descriptor chain on Queue 0:
  1. 16-byte `virtio_blk_req_t` (type + sector)
  2. 512-byte-per-sector data buffer
  3. 1-byte device-written status
- **`virtio_net.c`**: Uses **Queue 0 for RX** and **Queue 1 for TX**. Every transmitted frame is preceded by a 12-byte zeroed `virtio_net_hdr_t` descriptor.
- **`net/net.c`**: Builds raw Ethernet + ARP (`0x0806`) and Ethernet + IPv4 + UDP (`0x0800`, proto `17`) frames with RFC 1071 IPv4 header checksums.

---

## 6. Testing (`tests/test_drivers.c`, `tests/test_suite.py`)
Run `make test` anytime to verify:
1. Clean `clang` + `lld-link` ARM64 UEFI build (`-Wall -Wextra -Werror`).
2. Valid PE32+ EFI headers (`MZ`, `PE\0\0`, Machine `0xAA64`, Subsystem `10`, zero DLL imports).
3. Native host C unit tests for UART, framebuffer stride/colors, 64-bit BAR PCIe/VirtIO negotiation, `virtio-blk`, `virtio-net`, and ARP/UDP packet layout.
4. Automated QEMU + `.pcap` smoke test (when `qemu-system-aarch64` is installed).
