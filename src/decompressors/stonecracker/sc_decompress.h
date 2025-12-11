#ifndef SC_DECOMPRESS_H
#define SC_DECOMPRESS_H

#include <stddef.h>
#include <stdint.h>

// Decompress input buffer to output buffer.
// Returns size of decompressed data, or 0 on error.
// output_capacity must be sufficient (use header info to determine).
size_t sc_decompress(const uint8_t *input, size_t input_size, uint8_t *output,
                     size_t output_capacity);

// Helper to get raw size from header
uint32_t sc_get_raw_size(const uint8_t *input, size_t input_size);

#endif
