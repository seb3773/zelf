/*
 * Apultra Decompressor - Standalone Implementation Header
 */

#ifndef APULTRA_DECOMPRESS_H
#define APULTRA_DECOMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decompress apultra-compressed data
 *
 * Parameters:
 *   pInputData         - Input compressed buffer
 *   pOutData           - Output decompressed buffer
 *   nInputSize         - Size of compressed data
 *   nMaxOutBufferSize  - Capacity of output buffer
 *   nDictionarySize    - Dictionary size (usually 0)
 *   nFlags             - Flags (usually 0)
 *
 * Returns:
 *   Decompressed size on success, 0 on error
 */
size_t apultra_decompress(const unsigned char *pInputData, unsigned char *pOutData,
                          size_t nInputSize, size_t nMaxOutBufferSize,
                          size_t nDictionarySize, unsigned int nFlags);

#ifdef __cplusplus
}
#endif

#endif /* APULTRA_DECOMPRESS_H */
