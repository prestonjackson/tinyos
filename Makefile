CC      := clang
LD      := /opt/homebrew/opt/llvm/bin/ld.lld
EFI_FW  := /opt/homebrew/share/qemu/edk2-aarch64-code.fd

CFLAGS  := --target=aarch64-unknown-windows -ffreestanding -fno-stack-protector -O2 -Wall -Wextra
LDFLAGS := -subsystem:efi_application -entry:efi_main

all: esp/EFI/BOOT/BOOTAA64.EFI

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

esp/EFI/BOOT/BOOTAA64.EFI: main.o
	mkdir -p esp/EFI/BOOT
	$(LD) $(LDFLAGS) $< -o $@

run: all
	qemu-system-aarch64 \
		-M virt \
		-cpu host \
		-accel hvf \
		-m 256M \
		-nographic \
		-bios $(EFI_FW) \
		-drive format=raw,file=fat:rw:esp

clean:
	rm -rf main.o esp
