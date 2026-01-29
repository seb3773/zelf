#ifndef LZAV_COMPRESS_H
#define LZAV_COMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LZAV in-memory compression (pure C)
 * - input: pointer to input buffer
 * - input_size: size of input in bytes
 * - output: [out] malloc-allocated compressed buffer (caller must free)
 * - output_size: [out] compressed size in bytes
 * Returns 0 on success, -1 on error.
 */
int lzav_compress_c(const unsigned char *input, size_t input_size,
                    unsigned char **output, size_t *output_size);

#ifdef __cplusplus
}
#endif

#endif /* LZAV_COMPRESS_H */
