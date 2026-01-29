#include "lzsa2_compress.h"
#include "../../compressor/shrink_inmem.h"
#include "../../compressor/lib.h"

size_t lzsa2_compress_get_max_size(size_t nInputSize) {
    return lzsa_get_max_compressed_size_inmem(nInputSize);
}

int lzsa2_compress(const unsigned char *pInputData, unsigned char *pOutBuffer,
                   int nInputSize, int nMaxOutBufferSize,
                   unsigned int nFlags, int nMinMatchSize) {
    if (!pInputData || !pOutBuffer || nInputSize <= 0 || nMaxOutBufferSize <= 0)
        return -1;

    if (nMinMatchSize < 3) nMinMatchSize = 3;
    if (nMinMatchSize > 5) nMinMatchSize = 5;

    size_t nCompressedSize = lzsa_compress_inmem((unsigned char*)pInputData, pOutBuffer,
                                                  nInputSize, nMaxOutBufferSize,
                                                  nFlags, nMinMatchSize, 2);

    return (nCompressedSize > 0 && nCompressedSize <= nMaxOutBufferSize) ? (int)nCompressedSize : -1;
}
