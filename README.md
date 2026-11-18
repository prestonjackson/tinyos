# TinyOS

A minimal 64-bit ARM (`aarch64`) UEFI operating system for learning how to bootstrap an OS for cloud VMs, starting with QEMU on Apple Silicon.

## Features
- **UEFI Bootstrap (`kernel/main.c`)**: Pure C `efi_main` with `ExitBootServices()` handoff (no EDK2/assembly required).
- **TTY Console (`drivers/uart.c`)**: ARM PL011 UART serial driver (`0x09000000`) + interactive shell (`help`, `disk`, `send`, `arp`, `clear`).
- **Graphics Console (`drivers/fb.c`)**: UEFI GOP linear framebuffer + 8×8 bitmap font console (`-device ramfb`).
- **Disk Driver (`drivers/virtio_blk.c`)**: VirtIO 1.0 PCIe block driver.
- **Network Driver (`drivers/virtio_net.c`, `net/net.c`)**: VirtIO 1.0 PCIe NIC driver + Ethernet/ARP/IPv4/UDP packet builder.
- **Test Suite (`tests/test_suite.py`, `tests/test_drivers.c`)**: PE32+ header checks, host C unit tests with mocked MMIO, and QEMU smoke test.

## Prerequisites (macOS Apple Silicon)

```bash
brew install lld qemu
```

## Build, Test & Run

```bash
make test   # Cross-compile, validate PE32+ headers, run C unit tests (+ QEMU smoke test)
make run    # Boot in QEMU with Apple Hypervisor.framework (-accel hvf), TTY, ramfb, disk, and net
```

Press `Ctrl-A` then `X` in the terminal to exit QEMU. See [TUTORIAL.md](TUTORIAL.md) for a walkthrough of every file.
