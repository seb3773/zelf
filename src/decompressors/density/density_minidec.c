#include "density_minidec.h"
#include "density_api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* For packer host build, use standard malloc/free.
 * For stub builds (CODEC_DENSITY defined), use syscall-based allocator. */
#ifdef CODEC_DENSITY
/* Minimal mmap/munmap allocator to avoid libc and .bss (same style as zstd_minidec) */
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x2
#define MAP_ANONYMOUS 0x20
#if defined(__x86_64__)
static inline void *elfz_sys_mmap(size_t length) {
  register long rax asm("rax") = 9;                           /* SYS_mmap */
  register long rdi asm("rdi") = 0;                           /* addr */
  register long rsi asm("rsi") = (long)length;                /* length */
  register long rdx asm("rdx") = PROT_READ | PROT_WRITE;      /* prot */
  register long r10 asm("r10") = MAP_PRIVATE | MAP_ANONYMOUS; /* flags */
  register long r8 asm("r8") = -1;                            /* fd */
  register long r9 asm("r9") = 0;                             /* offset */
  asm volatile("syscall"
               : "=a"(rax)
               : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8),
                 "r"(r9)
               : "rcx", "r11", "memory");
  return (void *)rax;
}
static inline long elfz_sys_munmap(void *addr, size_t length) {
  register long rax asm("rax") = 11; /* SYS_munmap */
  register long rdi asm("rdi") = (long)addr;
  register long rsi asm("rsi") = (long)length;
  asm volatile("syscall"
               : "=a"(rax)
               : "a"(rax), "D"(rdi), "S"(rsi)
               : "rcx", "r11", "memory");
  return rax;
}
#elif defined(__aarch64__)
static inline void *elfz_sys_mmap(size_t length) {
  register long x8 asm("x8") = 222;
  register long x0 asm("x0") = 0;
  register long x1 asm("x1") = (long)length;
  register long x2 asm("x2") = PROT_READ | PROT_WRITE;
  register long x3 asm("x3") = MAP_PRIVATE | MAP_ANONYMOUS;
  register long x4 asm("x4") = -1;
  register long x5 asm("x5") = 0;
  asm volatile("svc 0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8) : "memory", "cc");
  return (void *)x0;
}
static inline long elfz_sys_munmap(void *addr, size_t length) {
  register long x8 asm("x8") = 215;
  register long x0 asm("x0") = (long)addr;
  register long x1 asm("x1") = (long)length;
  asm volatile("svc 0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory", "cc");
  return x0;
}
#else
#error "Unsupported architecture for CODEC_DENSITY syscall allocator"
#endif
static inline void *elfz_malloc(size_t n) {
  const size_t align = 32; /* alignment */
  const size_t hdr = 16;   /* store mapping base (8) + mapping size (8) */
  size_t total = n + hdr + align;
  void *mapping = elfz_sys_mmap(total);
  if ((long)mapping < 0)
    return (void *)0;
  unsigned char *base = (unsigned char *)mapping;
  unsigned char *p = base + hdr;
  uintptr_t aligned = ((uintptr_t)p + (align - 1)) & ~(uintptr_t)(align - 1);
  unsigned char *user = (unsigned char *)aligned;
  unsigned char *h = user - hdr;
  ((uintptr_t *)h)[0] = (uintptr_t)mapping;
  ((size_t *)(h + sizeof(uintptr_t)))[0] = total;
  return (void *)user;
}
static inline void elfz_free(void *ptr) {
  if (!ptr)
    return;
  const size_t hdr = 16;
  unsigned char *h = (unsigned char *)ptr - hdr;
  uintptr_t mapping = ((uintptr_t *)h)[0];
  size_t total = ((size_t *)(h + sizeof(uintptr_t)))[0];
  (void)elfz_sys_munmap((void *)mapping, total);
}
#else
/* Host build: use standard libc allocators */
static inline void *elfz_malloc(size_t n) { return malloc(n); }
static inline void elfz_free(void *ptr) { free(ptr); }
#endif

#ifndef CODEC_DENSITY
#include <stdio.h>
#endif

int density_decompress_c(const unsigned char *src, size_t src_sz,
                         unsigned char *dst, size_t dst_cap) {
  if (!src || !dst)
    return -1;

  density_buffer_processing_result r = density_buffer_decompress(
      src, (uint_fast64_t)src_sz, dst, (uint_fast64_t)dst_cap, elfz_malloc,
      elfz_free);

  if (r.state != DENSITY_BUFFER_STATE_OK)
    return -100 - (int)r.state;
  if (r.bytesWritten > (uint_fast64_t)0x7fffffff)
    return -1;
  return (int)r.bytesWritten;
}

#ifdef CODEC_DENSITY
/* Stage0-safe density decompression for dynamic binaries.
 * Identical to density_decompress_c - the stage0 wrapper handles NRV2B
 * decompression of the stub separately. */
int density_decompress_stage0(const unsigned char *src, size_t src_sz,
                              unsigned char *dst, size_t dst_cap) {
  return density_decompress_c(src, src_sz, dst, dst_cap);
}
#endif
