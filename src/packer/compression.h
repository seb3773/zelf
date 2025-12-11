#ifndef COMPRESSION_H
#define COMPRESSION_H
#include "packer_defs.h"

int compress_data_with_codec(const char *codec, const unsigned char *input,
                             size_t input_size, unsigned char **output,
                             int *output_size);

#endif
