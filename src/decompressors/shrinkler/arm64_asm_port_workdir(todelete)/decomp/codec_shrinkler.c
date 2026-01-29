#include <stddef.h>
#include "codec_shrinkler.h"

/*
 * Shrinkler decompressor placeholder.
 * TODO: implement real Shrinkler decoder.
 */
int shrinkler_decompress_c(const char* src, char* dst, int compressedSize, int dstCapacity)
{
    if (!src || !dst) return -1;
    if (compressedSize <= 0 || dstCapacity <= 0) return -1;
    /* Not implemented yet */
    return -2;
}
