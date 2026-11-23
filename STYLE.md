# TinyOS Code Style

Simple, uniform rules for all C and header files in TinyOS (no exceptions).

## 1. Formatting
- **Indentation:** 2 spaces (no tabs).
- **Line length:** 80 characters maximum (enforced by `make test`).
- **Braces:** K&R / attached opening brace (`if (...) {`), space after control keywords (`if`, `for`, `while`, `switch`).
- **Pointers:** Attached to the variable name (`uint8_t *buf`, `const char *s`).

## 2. Language & Types
- **Standard:** C23 (`-std=c2x`).
- **Warnings:** Zero warnings allowed (`-Wall -Wextra -Werror`).
- **Integer types:** Always use fixed-width types (`uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `uintptr_t`) from `<stdint.h>`—never `long` or `unsigned long` (LLP64 portability).
- **Booleans:** Use `bool`, `true`, `false` from `<stdbool.h>`.
- **Zero-initialization:** Prefer C23 empty braces (`struct foo x = {};`) for local/static structs.
- **Safety attributes:** Mark functions returning status codes with `[[nodiscard]]`, and validate hardware struct sizes with `_Static_assert`.

## 3. Naming & Structure
- **Files, functions, variables:** `snake_case` (e.g., `virtio_blk_rw`, `uart_putc`).
- **Types:** `snake_case` with `_t` suffix (`fb_console_t`, `pci_device_t`), except UEFI spec structs which keep their spec `UPPER_CASE` names (`EFI_SYSTEM_TABLE`).
- **Constants & macros:** `UPPER_SNAKE_CASE`.
- **Header guards:** `#ifndef TINYOS_<NAME>_H` / `#define TINYOS_<NAME>_H` / `#endif /* TINYOS_<NAME>_H */`.

## 4. Comments & Hardware Access
- **Comments:** Use `/* ... */` block and inline comments explaining *why* a hardware/ABI choice was made.
- **MMIO:** Never dereference packed structs for hardware registers; always use `mmio_read{8,16,32}` and `mmio_write{8,16,32}` with `dma_barrier()`.
