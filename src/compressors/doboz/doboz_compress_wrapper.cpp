/*
 * Doboz C Wrapper for Compression
 * Implementation
 */

#include "doboz_compress_wrapper.h"
#include "Compressor.h"

extern "C" {

size_t doboz_get_max_compressed_size(size_t input_size) {
    return doboz::Compressor::getMaxCompressedSize(input_size);
}

int doboz_compress_c(const unsigned char *input, size_t input_size,
                     unsigned char *output, size_t output_capacity) {
    if (!input || !output || input_size == 0 || output_capacity == 0) {
        return -1;
    }

    doboz::Compressor compressor;
    size_t compressed_size = 0;

    doboz::Result result = compressor.compress(
        input, input_size,
        output, output_capacity,
        compressed_size
    );

    if (result != doboz::RESULT_OK) {
        return -1;
    }

    return (int)compressed_size;
}

} // extern "C"
