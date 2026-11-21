#!/usr/bin/env python3
"""
TinyOS Automated Test Suite
1. Verifies clean ARM64 UEFI cross-compilation (`make`).
2. Validates PE32+ EFI headers of `esp/EFI/BOOT/BOOTAA64.EFI`.
3. Compiles and runs host-side C unit tests (`tests/test_drivers.c`).
4. Runs an optional QEMU + `.pcap` smoke test if `qemu-system-aarch64` is installed.
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EFI_PATH = os.path.join(ROOT, "esp", "EFI", "BOOT", "BOOTAA64.EFI")


def run(cmd, cwd=ROOT):
    print(f"+ {' '.join(cmd)}")
    subprocess.run(cmd, cwd=cwd, check=True)


def verify_pe32_efi(path):
    with open(path, "rb") as f:
        data = f.read()

    assert data[:2] == b"MZ", "Missing MZ DOS header"
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe_off:pe_off + 4] == b"PE\x00\x00", "Missing PE signature"

    coff_off = pe_off + 4
    machine, num_sections, _, _, _, opt_size, _ = struct.unpack_from("<HHIIIHH", data, coff_off)
    assert machine == 0xAA64, f"Expected ARM64 machine 0xAA64, got {hex(machine)}"
    assert num_sections >= 1, "Expected at least 1 section"

    opt_off = coff_off + 20
    magic = struct.unpack_from("<H", data, opt_off)[0]
    assert magic == 0x020B, f"Expected PE32+ magic 0x020B, got {hex(magic)}"

    entry_rva = struct.unpack_from("<I", data, opt_off + 16)[0]
    assert entry_rva != 0, "AddressOfEntryPoint must be non-zero"

    subsystem = struct.unpack_from("<H", data, opt_off + 68)[0]
    assert subsystem == 10, f"Expected EFI_APPLICATION subsystem (10), got {subsystem}"

    # Verify zero DLL imports (Import Directory is DataDirectory[1] at opt_off + 120)
    import_rva, import_size = struct.unpack_from("<II", data, opt_off + 120)
    assert import_rva == 0 and import_size == 0, "Freestanding EFI must have zero DLL imports"
    print(f"[PASS] PE32+ EFI header valid ({len(data)} bytes, entry={hex(entry_rva)})")


def run_c_unit_tests():
    with tempfile.TemporaryDirectory() as tmp:
        bin_path = os.path.join(tmp, "test_drivers")
        cmd = [
            "clang", "-DTEST_HOST", "-Iinclude", "-O2", "-Wall", "-Wextra", "-Werror",
            "tests/test_drivers.c",
            "drivers/uart.c", "drivers/fb.c", "drivers/pci.c",
            "drivers/virtio_pci.c", "drivers/virtio_blk.c", "drivers/virtio_net.c",
            "net/net.c",
            "-o", bin_path,
        ]
        run(cmd)
        run([bin_path])


def maybe_run_qemu_smoke():
    qemu = shutil.which("qemu-system-aarch64")
    fw_candidates = [
        "/opt/homebrew/share/qemu/edk2-aarch64-code.fd",
        "/usr/share/AAVMF/AAVMF_CODE.fd",
        "/usr/share/qemu-efi-aarch64/QEMU_EFI.fd",
    ]
    fw = next((p for p in fw_candidates if os.path.exists(p)), None)
    if not qemu or not fw:
        print("[SKIP] QEMU smoke test (qemu-system-aarch64 or UEFI firmware not installed here)")
        return

    with tempfile.TemporaryDirectory() as tmp:
        disk = os.path.join(tmp, "disk.img")
        pcap = os.path.join(tmp, "net.pcap")
        with open(disk, "wb") as f:
            f.write(b"\x00" * (1024 * 1024))

        accel_args = ["-cpu", "host", "-accel", "hvf"] if sys.platform == "darwin" else ["-cpu", "cortex-a72"]
        cmd = [
            qemu, "-M", "virt", *accel_args, "-m", "256M",
            "-display", "none", "-serial", "stdio",
            "-bios", fw,
            "-drive", f"format=raw,file=fat:rw:{os.path.join(ROOT, 'esp')}",
            "-drive", f"if=none,id=hd0,format=raw,file={disk}",
            "-device", "virtio-blk-pci,drive=hd0",
            "-netdev", "user,id=n0",
            "-device", "virtio-net-pci,netdev=n0,romfile=",
            "-object", f"filter-dump,id=f1,netdev=n0,file={pcap}",
        ]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            out, _ = proc.communicate(timeout=15)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, _ = proc.communicate()

        assert "TinyOS ARM64 Kernel Initialized" in out, f"Boot marker missing:\n{out}"
        assert "virtio-blk ready" in out, f"virtio-blk marker missing:\n{out}"
        assert "Sent UDP packet" in out, f"UDP TX marker missing:\n{out}"
        assert os.path.exists(pcap) and os.path.getsize(pcap) > 24, "Expected captured packets in pcap"
        print("[PASS] QEMU smoke test & pcap packet capture verified")


def main():
    run(["make", "clean"])
    run(["make"])
    verify_pe32_efi(EFI_PATH)
    run_c_unit_tests()
    maybe_run_qemu_smoke()
    print("\nALL TESTS PASSED!")


if __name__ == "__main__":
    sys.exit(main())
