#include "lzsa2_decompress.h"
#include "../../decompressor/expand_inmem.h"
#include "../../decompressor/lib.h"

int lzsa2_decompress_get_max_size(const unsigned char *pCompressedData, int nCompressedSize) {
    if (!pCompressedData || nCompressedSize <= 0)
        return -1;

    size_t nMaxSize = lzsa_get_max_decompressed_size_inmem(pCompressedData, nCompressedSize);
    return (nMaxSize > 0) ? (int)nMaxSize : -1;
}

int lzsa2_decompress(const unsigned char *pCompressedData, unsigned char *pOutBuffer,
                     int nCompressedSize, int nMaxOutBufferSize,
                     unsigned int nFlags) {
    if (!pCompressedData || !pOutBuffer || nCompressedSize <= 0 || nMaxOutBufferSize <= 0)
        return -1;

    int nFormatVersion = 0;
    size_t nDecompressedSize = lzsa_decompress_inmem((unsigned char*)pCompressedData, pOutBuffer,
                                                      nCompressedSize, nMaxOutBufferSize,
                                                      nFlags, &nFormatVersion);

    if (nFormatVersion != 2)
        return -1;

    return (nDecompressedSize > 0 && nDecompressedSize <= nMaxOutBufferSize) ? (int)nDecompressedSize : -1;
}
