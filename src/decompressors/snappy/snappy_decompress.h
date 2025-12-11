/*
 * Minimal Snappy decompressor in C - Header
 */

#ifndef SNAPPY_DECOMPRESS_H
#define SNAPPY_DECOMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decompress Snappy-compressed data.
 *
 * src: compressed data
 * src_len: compressed data size
 * dst: output buffer (must be large enough, use snappy_get_uncompressed_length)
 * dst_len: output buffer size
 *
 * Returns: number of decompressed bytes, or 0 on error
 */
size_t snappy_decompress_simple(const unsigned char* src, size_t src_len,
                                unsigned char* dst, size_t dst_len);

/*
 * Get uncompressed size from Snappy header.
 *
 * src: compressed data
 * src_len: compressed data size
 *
 * Returns: uncompressed size, or 0 on error
 */
size_t snappy_get_uncompressed_length(const unsigned char* src, size_t src_len);

#ifdef __cplusplus
}
#endif

#endif /* SNAPPY_DECOMPRESS_H */
