# TinyOS

A minimal 64-bit ARM (`aarch64`) UEFI operating system for learning how to bootstrap an OS for cloud VMs, starting with QEMU on Apple Silicon.

## Prerequisites (macOS Apple Silicon)

```bash
brew install llvm qemu
```

## Build & Run

```bash
make run
```

This compiles `main.c` with stock `clang` (`--target=aarch64-unknown-windows`), links `esp/EFI/BOOT/BOOTAA64.EFI` with `ld.lld`, and boots it in `qemu-system-aarch64` using Apple's Hypervisor.framework (`-accel hvf`).

Press `Ctrl-A` then `X` to exit QEMU.
