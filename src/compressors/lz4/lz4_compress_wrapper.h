#ifndef LZ4_COMPRESS_WRAPPER_H
#define LZ4_COMPRESS_WRAPPER_H

#include <stddef.h>

/*
 * Simple LZ4 (block) compressor wrapper using LZ4HC at max level.
 * - Allocates *out and sets *out_sz on success; caller must free(*out).
 * - Returns 0 on success, non-zero on error.
 * - Produces raw LZ4 block compatible with our stub's lz4_decompress().
 */
int lz4_compress_c(const unsigned char *in, size_t in_sz,
                   unsigned char **out, size_t *out_sz);

/* Optional progress callback: processed,total */
typedef void (*lz4_progress_cb_t)(size_t processed, size_t total);
void lz4_set_progress_cb(lz4_progress_cb_t cb);

/* Optional self-verify: decompress into a temp buffer and compare with input.
 * Returns 0 on success (match), non-zero on error or mismatch.
 */
int lz4_verify_c(const unsigned char *compressed, size_t comp_sz,
                 const unsigned char *original, size_t orig_sz);

#endif /* LZ4_COMPRESS_WRAPPER_H */
