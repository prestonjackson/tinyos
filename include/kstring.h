#ifndef TINYOS_KSTRING_H
#define TINYOS_KSTRING_H

#include <stddef.h>

/*
 * Freestanding Memory Helpers
 * ---------------------------
 * Why a custom kstring.h instead of <string.h>?
 *   When cross-compiling with `--target=aarch64-unknown-windows` and
 *   `-ffreestanding`, there is no Windows/macOS C standard library (<string.h>
 *   doesn't exist in Clang's freestanding resource dir). However, LLVM still
 *   emits calls to `memcpy`/`memset` for struct assignments and zero-init, so
 *   we provide our own implementations in lib/string.c.
 */
void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, int c, size_t n);
void* memmove(void* dst, const void* src, size_t n);
int   memcmp(const void* a, const void* b, size_t n);

#endif /* TINYOS_KSTRING_H */
