#ifndef CODEC_SHRINKLER_H
#define CODEC_SHRINKLER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Shrinkler decompressor API for stub side.
// Returns number of decompressed bytes on success, <0 on error.
// NOTE: Placeholder implementation for now; real decoder to be added.
int shrinkler_decompress_c(const char* src, char* dst, int compressedSize, int dstCapacity);

#ifdef __cplusplus
}
#endif

#endif // CODEC_SHRINKLER_H
