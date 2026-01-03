#ifndef SC_COMPRESS_H
#define SC_COMPRESS_H

#include <stddef.h>
#include <stdint.h>

// Compress input buffer to output buffer.
// Returns size of compressed data, or 0 on error.
// output_buffer must be large enough (e.g. input_size * 1.2 + 1024).
size_t sc_compress(const uint8_t *input, size_t input_size, uint8_t *output,
                   size_t output_capacity);

#endif
