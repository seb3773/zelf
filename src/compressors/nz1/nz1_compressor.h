#ifndef NZ1_COMPRESSOR_H
#define NZ1_COMPRESSOR_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Compresses input data using NanoZip algorithm.
 * @param input Pointer to the input data.
 * @param in_size Size of the input data.
 * @param output Pointer to the output buffer for compressed data.
 * @param out_size Maximum size of the output buffer.
 * @param window_size Desired compression window size (0 for default).
 * @return Size of the compressed data, or 0 on failure.
 */
size_t nanozip_compress(const uint8_t *input, size_t in_size, uint8_t *output,
                        size_t out_size, int window_size);

#endif // NZ1_COMPRESSOR_H
