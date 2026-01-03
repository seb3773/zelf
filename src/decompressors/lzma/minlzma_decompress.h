#ifndef MINLZMA_DECOMPRESS_H
#define MINLZMA_DECOMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Decompress raw LZMA stream prefixed by 5-byte properties.
// src format: [props5][rc-init+encoded stream]
// Returns decompressed size on success, -1 on error.
int minlzma_decompress_c(const char *src, char *dst, int compressedSize, int dstCapacity);

#ifdef __cplusplus
}
#endif

#endif // MINLZMA_DECOMPRESS_H
