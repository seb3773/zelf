#ifndef NZ1_DECOMPRESSOR_H
#define NZ1_DECOMPRESSOR_H

/*
 * minimalist nostdlib definitions.
 * We avoid including stdint.h/stddef.h if possible or assume they are available
 * in the freestanding environment.
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 1
#include <stddef.h>
#include <stdint.h>
#else
// Minimal definitions for freestanding
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
// Size_t definition usually requires knowing arch pointer size
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
typedef uint64_t size_t;
#else
typedef uint32_t size_t;
#endif
#endif

/**
 * @brief Decompresses NZ1 data suitable for stub integration.
 *
 * @param src Pointer to compressed data
 * @param src_len Length of compressed data
 * @param dst Pointer to destination buffer
 * @param dst_cap Capacity of destination buffer
 * @return int Positive: Decompressed size. Negative: Error code (-1).
 */
int nanozip_decompress(const void *src, int src_len, void *dst, int dst_cap);

#endif // NZ1_DECOMPRESSOR_H
