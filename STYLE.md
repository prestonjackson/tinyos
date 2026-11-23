# TinyOS Code Style

High-level engineering and language conventions for TinyOS (no exceptions).
Low-level formatting (indentation, line width, braces, pointer alignment) is
enforced automatically by [.clang-format](.clang-format).

## 1. Language & Safety
- **Standard:** C23 (`-std=c2x`), zero warnings (`-Wall -Wextra -Werror`).
- **Integer types:** Always use fixed-width types (`uint8_t`, `uint16_t`,
  `uint32_t`, `uint64_t`, `uintptr_t`) from `<stdint.h>`—never `long` or
  `unsigned long` (LLP64 portability).
- **Booleans:** Use `bool`, `true`, `false` from `<stdbool.h>`.
- **Zero-initialization:** Prefer C23 empty braces (`struct foo x = {};`) for
  local and static structs.
- **Safety attributes:** Mark functions returning status codes with
  `[[nodiscard]]`, and validate hardware ABI struct sizes with `_Static_assert`.

## 2. Naming & Structure
- **Files, functions, variables:** `snake_case` (e.g., `virtio_blk_rw`).
- **Types:** `snake_case` with `_t` suffix (`fb_console_t`, `pci_device_t`),
  except UEFI spec structs which keep their spec `UPPER_CASE` names.
- **Constants & macros:** `UPPER_SNAKE_CASE`.
- **Header guards:** `#ifndef TINYOS_<NAME>_H` / `#define TINYOS_<NAME>_H` /
  `#endif /* TINYOS_<NAME>_H */`.

## 3. Comments & Hardware Access
- **Comments:** Use `/* ... */` comments explaining *why* a hardware or ABI
  choice was made.
- **MMIO:** Never dereference packed structs for hardware registers; always use
  `mmio_read{8,16,32}` and `mmio_write{8,16,32}` with `dma_barrier()`.
