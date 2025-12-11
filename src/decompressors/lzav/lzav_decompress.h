/* LZAV Decompressor - Wrapper Header
 * Compatible API for benchmarking framework
 */
#ifndef LZAV_DECOMPRESS_H
#define LZAV_DECOMPRESS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decompress LZAV-compressed data
 * Compatible API: int func(const char* src, char* dst, int srcSize, int dstCapacity)
 * Returns: decompressed size on success, -1 on error
 */
int lzav_decompress_c(const char* src, char* dst, int srcSize, int dstCapacity);

#ifdef __cplusplus
}
#endif

#endif
