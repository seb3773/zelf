/*
 * lzsa2_decompress.h - LZSA2 decompression library (size-optimized)
 */

#ifndef _LZSA2_DECOMPRESS_H
#define _LZSA2_DECOMPRESS_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LZSA_FLAG_RAW_BLOCK        (1<<1)
#define LZSA_FLAG_RAW_BACKWARD     (1<<2)

int lzsa2_decompress_get_max_size(const unsigned char *pCompressedData, int nCompressedSize);

int lzsa2_decompress(const unsigned char *pCompressedData, unsigned char *pOutBuffer,
                     int nCompressedSize, int nMaxOutBufferSize,
                     unsigned int nFlags);

#ifdef __cplusplus
}
#endif

#endif /* _LZSA2_DECOMPRESS_H */
