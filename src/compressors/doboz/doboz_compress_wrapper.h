/*
 * Doboz C Wrapper for Compression
 * Exposes C API for C++ Doboz Compressor class
 */

#ifndef DOBOZ_COMPRESS_WRAPPER_H
#define DOBOZ_COMPRESS_WRAPPER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get maximum compressed size for given input size
 *
 * @param input_size Size of input data in bytes
 * @return Maximum possible compressed size
 */
size_t doboz_get_max_compressed_size(size_t input_size);

/**
 * Compress data using Doboz algorithm
 *
 * @param input Input data buffer
 * @param input_size Size of input data
 * @param output Output buffer (must be at least doboz_get_max_compressed_size bytes)
 * @param output_capacity Capacity of output buffer
 * @return Actual compressed size on success, -1 on error
 */
int doboz_compress_c(const unsigned char *input, size_t input_size,
                     unsigned char *output, size_t output_capacity);

#ifdef __cplusplus
}
#endif

#endif /* DOBOZ_COMPRESS_WRAPPER_H */
