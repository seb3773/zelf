#ifndef PP_COMPRESS_H
#define PP_COMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int pp_compress_c(const unsigned char *input, size_t input_size,
                  unsigned char **output, size_t *output_size);

#ifdef __cplusplus
}
#endif

#endif
