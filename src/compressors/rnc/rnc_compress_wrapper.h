#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

int rnc_compress_m1(const unsigned char *in, size_t in_sz, unsigned char **out,
                    size_t *out_sz);

#ifdef __cplusplus
}
#endif
