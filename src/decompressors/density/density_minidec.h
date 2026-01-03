#ifndef DENSITY_MINIDEC_H
#define DENSITY_MINIDEC_H

#include <stddef.h>

/*
 * Minimal Density (Lion) container decoder entry point for stubs.
 * - No integrity check (SpookyHash fully compiled out)
 * - No dynamic allocation via libc: uses internal mmap-based allocator
 * - Returns number of bytes written on success, -1 on error
 */
int density_decompress_c(const unsigned char *src, size_t src_sz,
                         unsigned char *dst, size_t dst_cap);

#ifdef CODEC_DENSITY
/*
 * Stage0-safe density decompression that doesn't corrupt destination address.
 * Uses a modified allocator that avoids writing metadata before the dst buffer.
 */
int density_decompress_stage0(const unsigned char *src, size_t src_sz,
                              unsigned char *dst, size_t dst_cap);
#endif

#endif /* DENSITY_MINIDEC_H */
