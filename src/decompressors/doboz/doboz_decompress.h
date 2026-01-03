/*
 * Doboz Decompressor - Standalone API
 */

#ifndef DOBOZ_DECOMPRESS_H
#define DOBOZ_DECOMPRESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Get uncompressed size from doboz compressed data header
 *
 * Parameters:
 *   src     - Input compressed buffer
 *   srcSize - Size of compressed data
 *
 * Returns:
 *   Uncompressed size, or 0 on error
 */
static inline size_t doboz_get_uncompressed_size(const void* src, size_t srcSize) {
    const uint8_t* input = (const uint8_t*)src;

    if (srcSize < 1) return 0;

    uint32_t attributes = *input++;
    int sizeCodedSize = ((attributes >> 3) & 7) + 1;
    int headerSize = 1 + 2 * sizeCodedSize;

    if (srcSize < (size_t)headerSize) return 0;

    uint64_t uncompressedSize;
    switch (sizeCodedSize) {
        case 1:
            uncompressedSize = *(const uint8_t*)input;
            break;
        case 2:
            uncompressedSize = *(const uint16_t*)input;
            break;
        case 4:
            uncompressedSize = *(const uint32_t*)input;
            break;
        case 8:
            uncompressedSize = *(const uint64_t*)input;
            break;
        default:
            return 0;
    }

    return (size_t)uncompressedSize;
}

/*
 * Decompress doboz-compressed data
 *
 * Parameters:
 *   src         - Input compressed buffer
 *   dst         - Output decompressed buffer
 *   srcSize     - Size of compressed data
 *   dstCapacity - Capacity of output buffer
 *
 * Returns:
 *   >= 0 : Decompressed size (success)
 *   < 0  : Error code
 */
int doboz_decompress_v2(const char* src, char* dst, int srcSize, int dstCapacity);

/* Compatibility wrapper */
static inline int doboz_decompress(const void* src, size_t srcSize, void* dst, size_t dstCapacity) {
    return doboz_decompress_v2((const char*)src, (char*)dst, (int)srcSize, (int)dstCapacity);
}

#ifdef __cplusplus
}
#endif

#endif /* DOBOZ_DECOMPRESS_H */
