#ifndef CODEC_LZ4_H
#define CODEC_LZ4_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal LZ4 decompressor compatible with LZ4_compress_default output.
// Returns number of decompressed bytes on success, or -1 on error.
int lz4_decompress(const char* src, char* dst, int compressedSize, int dstCapacity);

#ifdef __cplusplus
}
#endif

#endif // CODEC_LZ4_H
