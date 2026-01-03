#ifndef DENSITY_COMPRESS_WRAPPER_H
#define DENSITY_COMPRESS_WRAPPER_H

#include <stddef.h>

/*
 * Density (Lion) compressor wrapper for the packer.
 * API normalized across codecs (returns 0 on success, allocates *out).
 * Uses Lion algorithm + DEFAULT block type (no integrity check).
 */
int density_compress_c(const unsigned char *in, size_t in_sz,
                       unsigned char **out, size_t *out_sz);

#endif /* DENSITY_COMPRESS_WRAPPER_H */
