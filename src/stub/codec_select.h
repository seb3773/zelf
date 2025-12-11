#ifndef CODEC_SELECT_H
#define CODEC_SELECT_H

/* Codec selection shim for dynamic/static stubs.
 * Select with -DCODEC_APULTRA or -DCODEC_ZX7B or -DCODEC_ZX0 (else defaults to
 * LZ4).
 *
 * Exposes a uniform symbol:
 *   int lz4_decompress(const char* src, char* dst, int compressedSize, int
 * dstCapacity); which will invoke the chosen backend (LZ4, Apultra, ZX7B,
 * ZX0...).
 */

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
  /* apultra returns 0 on error */
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
  return r; /* propagate negative codes for diagnostics */
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
#else
#include "codec_lz4.h"
#endif

#endif /* CODEC_SELECT_H */
