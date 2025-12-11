#ifndef STUB_UTILS_H
#define STUB_UTILS_H

#include "stub_defs.h"

// Declare external symbols with hidden visibility to avoid PLT generation
// This is critical for -fPIC build without .plt/.got sections
__attribute__((visibility("hidden"))) extern void *
memcpy(void *dest, const void *src, size_t n);
__attribute__((visibility("hidden"))) extern void *memset(void *dest, int c,
                                                          size_t n);

// Simple strlen pour nostdlib
static inline int simple_strlen(const char *str) {
  int len = 0;
  while (str[len] != '\0') {
    len++;
  }
  return len;
}

// Helper PF→PROT
static inline int pf_to_prot(unsigned pf) {
  int prot = 0; // PF_X=1, PF_W=2, PF_R=4
  if (pf & 4)
    prot |= PROT_READ;
  if (pf & 2)
    prot |= PROT_WRITE;
  if (pf & 1)
    prot |= PROT_EXEC;
  return prot;
}

// Macros de debug (no-op en prod)
#define simple_hex_string(a, b) ((void)0)
#define simple_decimal_string(a, b) ((void)0)

// Use __builtin_memcpy. If it emits a call, it will use our hidden memcpy.
static inline void *simple_memcpy(void *dst, const void *src, size_t n) {
  return __builtin_memcpy(dst, src, n);
}

// Use __builtin_memset
static inline void *simple_memset(void *dst, int c, size_t n) {
  return __builtin_memset(dst, c, n);
}

#endif // STUB_UTILS_H
