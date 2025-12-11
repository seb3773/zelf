#ifndef BLZ_COMPRESS_WRAPPER_H
#define BLZ_COMPRESS_WRAPPER_H

#include <stddef.h>

/*
 * BriefLZ compressor wrapper for the packer.
 * API normalized across codecs (returns 0 on success, allocates *out).
 * Compression level hardcoded to 10 (optimal, slower, best ratio).
 */
int blz_compress_c(const unsigned char *in, size_t in_sz,
                   unsigned char **out, size_t *out_sz);

#endif /* BLZ_COMPRESS_WRAPPER_H */
