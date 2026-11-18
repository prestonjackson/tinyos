CC      := clang
LD      := $(shell command -v lld-link 2>/dev/null || \
             (test -x /opt/homebrew/opt/lld/bin/lld-link && echo /opt/homebrew/opt/lld/bin/lld-link) || \
             echo /opt/homebrew/opt/llvm/bin/lld-link)
EFI_FW  := /opt/homebrew/share/qemu/edk2-aarch64-code.fd

CFLAGS  := --target=aarch64-unknown-windows -ffreestanding -fno-builtin \
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

run: all disk.img
	qemu-system-aarch64 \
		-M virt \
		-cpu host \
		-accel hvf \
		-m 256M \
		-serial stdio \
		-device ramfb \
		-bios $(EFI_FW) \
		-drive format=raw,file=fat:rw:esp \
		-drive if=none,id=hd0,format=raw,file=disk.img \
		-device virtio-blk-pci,drive=hd0 \
		-netdev user,id=net0 \
		-device virtio-net-pci,netdev=net0

test:
	python3 tests/test_suite.py

clean:
	rm -rf build esp disk.img
