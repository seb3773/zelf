#include "zstd_minidec.h"
#include <stddef.h>
#include <stdint.h>

/* muzscat-based minimal Zstd decoder integration (no .bss writes) */
/* Config: small footprint, no legacy, no dict, no multiframes */
#define DEBUGLEVEL 0
#define ZSTD_LEGACY_SUPPORT 0
#define ZSTD_LIB_DEPRECATED 0
#define ZSTD_NO_UNUSED_FUNCTIONS 1
#define ZSTD_STRIP_ERROR_STRINGS 1
#define ZSTD_TRACE 0
#define ZSTD_DECOMPRESS_DICTIONARY 0
#define ZSTD_DECOMPRESS_MULTIFRAME 0
#define HUF_FORCE_DECOMPRESS_X1 1
#define DYNAMIC_BMI2 0
#define ZSTD_IGNORE_CUSTOMALLOC 1
#define ZSTD_ADD_UNUSED 0
#define XXH_PRIVATE_API 1

/* Make public API compile as static (in-header) functions */
#define ZSTDLIB_VISIBILITY MEM_STATIC
#define ZSTDERRORLIB_VISIBILITY MEM_STATIC

/* Minimal mmap/munmap allocator to avoid libc and .bss */
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x2
#define MAP_ANONYMOUS 0x20
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
static inline void *elfz_malloc(size_t n) {
  /* 32-byte alignment for potential vectorized paths */
  const size_t align = 32;
  const size_t hdr = 16; /* store mapping base (8) + mapping size (8) */
  size_t total = n + hdr + align;
  void *mapping = elfz_sys_mmap(total);
  if ((long)mapping < 0)
    return (void *)0;
  unsigned char *base = (unsigned char *)mapping;
  unsigned char *p = base + hdr;
  uintptr_t aligned = ((uintptr_t)p + (align - 1)) & ~(uintptr_t)(align - 1);
  unsigned char *user = (unsigned char *)aligned;
  unsigned char *h = user - hdr;
  /* store mapping base and size in header */
  ((uintptr_t *)h)[0] = (uintptr_t)mapping;
  ((size_t *)(h + sizeof(uintptr_t)))[0] = total;
  return (void *)user;
}
static inline void *elfz_calloc(size_t n) {
  /* mmap returns zeroed pages */
  return elfz_malloc(n);
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

/* Dependencies toggles used by zstd_deps.h */
#define ZSTD_DEPS_NEED_STDINT
#define ZSTD_DEPS_NEED_MATH64

/* Follow muzscat's include order to satisfy internal dependencies */
#include "entropy_common.h"
#include "fse_decompress.h"
#include "zstd_deps.h"

/* Provide customMem typedefs expected by zstd_internal.h */
typedef void *ZSTD_customMem;
#define ZSTD_defaultCMem 0
#include "zstd_internal.h"

/* Hook Zstd custom allocators to our mmap allocator (ignore customMem) */
#define ZSTD_customMalloc(size, customMem) elfz_malloc(size)
#define ZSTD_customCalloc(size, customMem) elfz_calloc(size)
#define ZSTD_customFree(ptr, customMem) elfz_free(ptr)

/* Remaining pieces */
#include "huf_decompress.h"
#include "zstd_ddict_noops.h"
#include "zstd_decompress.h"
#include "zstd_decompress_block_c.h"

int zstd_decompress_c(const unsigned char *src, size_t src_sz,
                      unsigned char *dst, size_t dst_cap) {
  if (!src || !dst)
    return -1;
  ZSTD_DCtx *dctx = (ZSTD_DCtx *)elfz_calloc(sizeof(ZSTD_DCtx));
  if (!dctx)
    return -1;
  ZSTD_initDCtx_internal(dctx);
  dctx->format = ZSTD_f_zstd1;
  /* Optional header sanity: if invalid header, fail early */
  {
    ZSTD_frameHeader zfh;
    size_t hr = ZSTD_getFrameHeader_advanced(&zfh, src, src_sz, ZSTD_f_zstd1);
    if (ZSTD_isError(hr)) {
      elfz_free(dctx);
      return -1;
    }
    // __asm__ volatile("int3"); /* TRAP_ZSTD_HDR_OK [disabled] */
  }
  /* Bufferless streaming API: feed exact chunks as requested */
  size_t total = 0;
  const unsigned char *sp = src;
  size_t ss = src_sz;
  unsigned char *dp = dst;
  size_t dd = dst_cap;
  (void)ZSTD_decompressBegin(dctx);
  // __asm__ volatile("int3"); /* TRAP_ZSTD_AFTER_BEGIN [disabled] */
  for (;;) {
    size_t const need = ZSTD_nextSrcSizeToDecompress(dctx);
    if (need == 0)
      break; /* done with frame */
    if (ss < need) {
      elfz_free(dctx);
      return -1;
    } /* truncated */
    // __asm__ volatile("int3"); /* TRAP_ZSTD_BEFORE_FIRST_CONT (and each chunk)
    // [disabled] */
    size_t const wrote = ZSTD_decompressContinue(dctx, dp, dd, sp, need);
    if (ZSTD_isError(wrote)) {
      elfz_free(dctx);
      return -1;
    }
    // __asm__ volatile("int3"); /* TRAP_ZSTD_AFTER_CONT [disabled] */
    sp += need;
    ss -= need;
    if (wrote > dd) {
      elfz_free(dctx);
      return -1;
    }
    dp += wrote;
    dd -= wrote;
    total += wrote;
  }
  // __asm__ volatile("int3"); /* TRAP_ZSTD_DONE [disabled] */
  elfz_free(dctx);
  return (int)total;
}

long long zstd_peek_content_size(const unsigned char *src, size_t src_sz) {
  ZSTD_frameHeader zfh;
  size_t r = ZSTD_getFrameHeader_advanced(&zfh, src, src_sz, ZSTD_f_zstd1);
  if (ZSTD_isError(r) || r > 0)
    return -1; /* error or not enough input */
  if (zfh.frameType == ZSTD_skippableFrame) {
    return (long long)zfh.frameContentSize;
  }
  if (zfh.frameContentSize == ZSTD_CONTENTSIZE_UNKNOWN ||
      zfh.frameContentSize == ZSTD_CONTENTSIZE_ERROR) {
    return -1;
  }
  return (long long)zfh.frameContentSize;
}
