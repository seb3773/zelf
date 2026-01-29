#ifndef STUB_UTILS_H
#define STUB_UTILS_H

#include "stub_defs.h"

__attribute__((visibility("hidden"))) extern void *
memcpy(void *dest, const void *src, size_t n);
__attribute__((visibility("hidden"))) extern void *memset(void *dest, int c,
                                                          size_t n);

static inline int simple_strlen(const char *str) {
  int len = 0;
  while (str[len] != '\0') {
    len++;
  }
  return len;
}

static inline int pf_to_prot(unsigned pf) {
  int prot = 0;
  if (pf & 4)
    prot |= PROT_READ;
  if (pf & 2)
    prot |= PROT_WRITE;
  if (pf & 1)
    prot |= PROT_EXEC;
  return prot;
}

#define simple_hex_string(a, b) ((void)0)
#define simple_decimal_string(a, b) ((void)0)

static inline void *simple_memcpy(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  for (size_t i = 0; i < n; i++)
    d[i] = s[i];
  return dst;
}

static inline void *simple_memset(void *dst, int c, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  unsigned char v = (unsigned char)c;
  for (size_t i = 0; i < n; i++)
    d[i] = v;
  return dst;
}

#endif // STUB_UTILS_H
