#ifndef ZX0_DECOMPRESS_H
#define ZX0_DECOMPRESS_H

/*
 * ZX0 decompressor (forward-buffer API)
 * Decompresses ZX0 stream to caller-provided output buffer.
 * Returns number of bytes written on success, -1 on error.
 */
int zx0_decompress_to(const unsigned char *in, int in_size,
                      unsigned char *out, int out_max);

#endif /* ZX0_DECOMPRESS_H */
