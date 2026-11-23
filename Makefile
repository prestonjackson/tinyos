# TinyOS Build & QEMU Launch Configuration
# ---------------------------------------------------------------------------
# Compiler & Linker:
#   - CC: Stock Apple Clang compiles ARM64 COFF/PE object files natively.
#   - LD: Homebrew lld-link (PE/COFF linker) is required because Apple's ld64
#         only links Mach-O binaries, whereas UEFI requires PE32+ (.EFI).
CC      := clang
LD      := $(shell command -v lld-link 2>/dev/null || \
             (test -x /opt/homebrew/opt/lld/bin/lld-link && echo /opt/homebrew/opt/lld/bin/lld-link) || \
             echo /opt/homebrew/opt/llvm/bin/lld-link)
EFI_FW  := /opt/homebrew/share/qemu/edk2-aarch64-code.fd

# Freestanding UEFI Compiler & Linker Flags:
#   --target=aarch64-unknown-windows : Emit ARM64 PE/COFF objects for UEFI ABI
#   -ffreestanding -fno-builtin      : No OS libc; use our own memcpy/memset
#   -fno-stack-protector             : No OS stack-canary runtime (__stack_chk_fail)
#   -mgeneral-regs-only              : Avoid NEON/FP registers before OS enables them
#   -mno-stack-arg-probe             : Avoid Windows __chkstk probes on stack frames
CFLAGS  := -std=c2x --target=aarch64-unknown-windows -ffreestanding -fno-builtin \
           -fno-stack-protector -mgeneral-regs-only -mno-stack-arg-probe \
           -Iinclude -O2 -Wall -Wextra -Werror
LDFLAGS := -subsystem:efi_application -entry:efi_main

SRCS := lib/string.c \
        drivers/uart.c \
        drivers/fb.c \
        drivers/pci.c \
        drivers/virtio_pci.c \
        drivers/virtio_blk.c \
        drivers/virtio_net.c \
        net/net.c \
        kernel/main.c

OBJS := $(patsubst %.c,build/%.o,$(SRCS))

all: esp/EFI/BOOT/BOOTAA64.EFI

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

esp/EFI/BOOT/BOOTAA64.EFI: $(OBJS)
	@mkdir -p esp/EFI/BOOT
	$(LD) $(LDFLAGS) $(OBJS) -out:$@

disk.img:
	dd if=/dev/zero of=disk.img bs=1M count=4

# QEMU Launch Flags:
#   -M virt                         : Generic ARM64 virtual machine (PL011 + PCIe)
#   -cpu host -accel hvf            : Run natively on Apple Silicon M2 via Hypervisor.framework
#   -m 256M                         : Allocate 256 MB guest RAM
#   -display cocoa,zoom-to-fit=on   : macOS GUI window that scales framebuffer when resized
#   -serial stdio                   : Connect PL011 UART0 (0x09000000) to your terminal
#   -device ramfb                   : Linear RAM framebuffer (keeps working after ExitBootServices)
#   -bios $(EFI_FW)                 : EDK2 ARM64 UEFI firmware
#   -drive format=raw,file=fat:rw:esp : Expose local esp/ folder as a virtual FAT32 boot drive
#   -device virtio-blk-pci          : Attach 4 MB disk.img over PCIe for virtio_blk driver
#   -device virtio-net-pci,romfile= : Attach user-mode NIC over PCIe (romfile= skips x64 iPXE ROM)
run: all disk.img
	qemu-system-aarch64 \
		-M virt \
		-cpu host \
		-accel hvf \
		-m 256M \
		-display cocoa,zoom-to-fit=on \
		-serial stdio \
		-device ramfb \
		-bios $(EFI_FW) \
		-drive format=raw,file=fat:rw:esp \
		-drive if=none,id=hd0,format=raw,file=disk.img \
		-device virtio-blk-pci,drive=hd0 \
		-netdev user,id=net0 \
		-device virtio-net-pci,netdev=net0,romfile=

test:
	python3 tests/test_suite.py

clean:
	rm -rf build esp disk.img
