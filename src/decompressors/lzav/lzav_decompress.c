/*
 * LZAV Decompressor - Wrapper Implementation
 * Uses LZAV header-only library
 */

#include "lzav_decompress.h"
#define LZAV_DECOMPRESSOR_ONLY
#include "../../compressors/lzav/lzav.h"

/*
 * Decompress LZAV-compressed data
 * Compatible API: int func(const char* src, char* dst, int srcSize, int dstCapacity)
 */
int lzav_decompress_c(const char* src, char* dst, int srcSize, int dstCapacity) {
    if (!src || !dst || srcSize <= 0 || dstCapacity <= 0) {
        return -1;
    }

    // Call LZAV's partial decompression function (doesn't require exact size)
    // int lzav_decompress_partial(const void* src, void* dst, int srcl, int dstl)
    int result = lzav_decompress_partial(src, dst, srcSize, dstCapacity);

    // lzav_decompress_partial returns <0 on error, or decompressed size
    if (result < 0) {
        return -1;
    }

    return result;
}
