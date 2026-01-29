/*
 * lzsa2_compress.h - LZSA2 compression library
 */

#ifndef _LZSA2_COMPRESS_H
#define _LZSA2_COMPRESS_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compression flags */
#define LZSA_FLAG_FAVOR_RATIO      (1<<0)
#define LZSA_FLAG_RAW_BLOCK        (1<<1)
#define LZSA_FLAG_RAW_BACKWARD     (1<<2)

size_t lzsa2_compress_get_max_size(size_t nInputSize);

int lzsa2_compress(const unsigned char *pInputData, unsigned char *pOutBuffer,
                   int nInputSize, int nMaxOutBufferSize,
                   unsigned int nFlags, int nMinMatchSize);

#ifdef __cplusplus
}
#endif

#endif /* _LZSA2_COMPRESS_H */
