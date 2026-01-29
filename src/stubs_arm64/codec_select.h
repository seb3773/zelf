#ifndef CODEC_SELECT_H
#define CODEC_SELECT_H

#ifdef CODEC_ZX7B
#include "zx7b_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  return zx7b_decompress((const unsigned char *)src, compressedSize,
                         (unsigned char *)dst, dstCapacity);
}
#elif defined(CODEC_ZX0)
#include "zx0_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  return zx0_decompress_to((const unsigned char *)src, compressedSize,
                           (unsigned char *)dst, dstCapacity);
}
#elif defined(CODEC_BRIEFLZ)
#include "brieflz_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  return brieflz_decompress_to((const unsigned char *)src, compressedSize,
                               (unsigned char *)dst, dstCapacity);
}
#elif defined(CODEC_EXO)
#include "exo_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  return exo_decompress((const unsigned char *)src, compressedSize,
                        (unsigned char *)dst, dstCapacity);
}
#elif defined(CODEC_SNAPPY)
#include "snappy_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  size_t r = snappy_decompress_simple(
      (const unsigned char *)src, (size_t)compressedSize, (unsigned char *)dst,
      (size_t)dstCapacity);
  return r ? (int)r : -1;
}
#elif defined(CODEC_DOBOZ)
#include "doboz_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  int r =
      doboz_decompress(src, (size_t)compressedSize, dst, (size_t)dstCapacity);
  return r >= 0 ? r : -1;
}
#elif defined(CODEC_QLZ)
#include "quicklz.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  (void)compressedSize;
  (void)dstCapacity;
  qlz_state_decompress state = {0};
  size_t r = qlz_decompress(src, dst, &state);
  return r > 0 ? (int)r : -1;
}
#elif defined(CODEC_LZAV)
#include "lzav_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  int r = lzav_decompress_c(src, dst, (int)compressedSize, (int)dstCapacity);
  return r >= 0 ? r : -1;
}
#elif defined(CODEC_PP)
#include "powerpacker_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  int r = powerpacker_decompress(
      (const unsigned char *)src, (unsigned int)compressedSize,
      (unsigned char *)dst, (unsigned int)dstCapacity);
  return r > 0 ? r : -1;
}
#elif defined(CODEC_APULTRA)
#include "apultra_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  size_t r =
      apultra_decompress((const unsigned char *)src, (unsigned char *)dst,
                         (size_t)compressedSize, (size_t)dstCapacity, 0, 0);
  return r ? (int)r : -1;
}
#elif defined(CODEC_ZSTD)
#include "zstd_minidec.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  int r = zstd_decompress_c((const unsigned char *)src, (size_t)compressedSize,
                            (unsigned char *)dst, (size_t)dstCapacity);
  return r >= 0 ? r : -1;
}
#elif defined(CODEC_LZMA)
#include "minlzma_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  int r = minlzma_decompress_c(src, dst, compressedSize, dstCapacity);
  return r;
}
#elif defined(CODEC_SHRINKLER)
#include "codec_shrinkler.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  return shrinkler_decompress_c(src, dst, compressedSize, dstCapacity);
}
#elif defined(CODEC_SC)
#include "sc_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  size_t r = sc_decompress((const uint8_t *)src, (size_t)compressedSize,
                           (uint8_t *)dst, (size_t)dstCapacity);
  return r ? (int)r : -1;
}
#elif defined(CODEC_LZSA)
#include "lzsa2_decompress.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  return lzsa2_decompress((const unsigned char *)src, (unsigned char *)dst,
                          compressedSize, dstCapacity, 0);
}
#elif defined(CODEC_DENSITY)
#include "density_minidec.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  int r = density_decompress_c((const unsigned char *)src, (size_t)compressedSize,
                               (unsigned char *)dst, (size_t)dstCapacity);
  return r >= 0 ? r : -1;
}
#elif defined(CODEC_RNC)
#include "rnc_minidec.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  return rnc_decompress_to((const unsigned char *)src, compressedSize,
                           (unsigned char *)dst, dstCapacity);
}
#elif defined(CODEC_LZFSE)
#include "lzfse_stub.h"
#include "stub_syscalls.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  size_t ws_sz = lzfse_decode_scratch_size();
  size_t ws_map = (ws_sz + 0xFFFu) & ~(size_t)0xFFFu;
  if (ws_map == 0)
    ws_map = 4096u;

  void *workspace =
      (void *)z_syscall_mmap(NULL, ws_map, 3 /* PROT_READ|PROT_WRITE */,
                             34 /* MAP_PRIVATE|MAP_ANONYMOUS */, -1, 0);
  if ((long)workspace < 0)
    return -1;

  size_t r = lzfse_decode_buffer((uint8_t *)dst, (size_t)dstCapacity,
                                 (const uint8_t *)src, (size_t)compressedSize,
                                 workspace);
  z_syscall_munmap(workspace, ws_map);
  return r ? (int)r : -1;
}
#elif defined(CODEC_CSC)
#include "csc_dec.h"
#include "stub_syscalls.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  intptr_t ws = CSC_Dec_GetWorkspaceSize((const void *)src, (size_t)compressedSize);
  if (ws < 0)
    return -1;

  size_t ws_sz = (size_t)ws;
  size_t ws_map = (ws_sz + 0xFFFu) & ~(size_t)0xFFFu;
  if (ws_map == 0)
    ws_map = 4096u;

  void *workspace =
      (void *)z_syscall_mmap(NULL, ws_map, 3 /* PROT_READ|PROT_WRITE */,
                             34 /* MAP_PRIVATE|MAP_ANONYMOUS */, -1, 0);
  if ((long)workspace < 0)
    return -1;

  intptr_t r = CSC_Dec_Decompress((const void *)src, (size_t)compressedSize,
                                  (void *)dst, (size_t)dstCapacity, workspace);
  z_syscall_munmap(workspace, ws_map);
  return (r >= 0) ? (int)r : -1;
}
#elif defined(CODEC_NZ1)
#include "nz1_decompressor.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  return nanozip_decompress((const void *)src, compressedSize, (void *)dst,
                            dstCapacity);
}
#elif defined(CODEC_LZHAM)
#include "lzham_decompressor.h"
#include "stub_syscalls.h"
#include <stddef.h>
static inline int lz4_decompress(const char *src, char *dst, int compressedSize,
                                 int dstCapacity) {
  size_t ws_sz = 512 * 1024;
  void *workspace =
      (void *)z_syscall_mmap(NULL, ws_sz, 3 /* PROT_READ|PROT_WRITE */,
                             34 /* MAP_PRIVATE|MAP_ANONYMOUS */, -1, 0);
  if ((long)workspace < 0)
    return -1;

  lzhamd_params_t p;
  lzhamd_params_init_default(&p, workspace, ws_sz, 29);
  size_t val_dst_len = (size_t)dstCapacity;
  lzhamd_status_t status =
      lzhamd_decompress_memory(&p, (uint8_t *)dst, &val_dst_len,
                               (const uint8_t *)src, (size_t)compressedSize);
  z_syscall_munmap(workspace, ws_sz);

  if (status != LZHAMD_STATUS_SUCCESS) {
    int ec = (status < 0) ? (50 - (int)status) : (150 + (int)status);
    z_syscall_exit(ec);
  }

  return (status == LZHAMD_STATUS_SUCCESS) ? (int)val_dst_len : -1;
}
#else
#include "codec_lz4.h"
#endif

#endif /* CODEC_SELECT_H */
