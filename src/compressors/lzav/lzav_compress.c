/*
 * LZAV in-memory compression (pure C wrapper)
 * Uses header-only LZAV under src/compressors/lzav/lzav.h (local to repo)
 */
#include <stdlib.h>
#include <string.h>
#include "lzav_compress.h"
#include "lzav.h"  /* local under ./src/compressors/lzav */

int lzav_compress_c(const unsigned char *input, size_t input_size,
                    unsigned char **output, size_t *output_size)
{
    if (!input || input_size == 0 || !output || !output_size) {
        return -1;
    }

    if (input_size > (size_t)0x7fffffff) {
        return -1;
    }

    int src_len = (int)input_size;
    int bound = lzav_compress_bound_hi(src_len);
    if (bound <= 0) return -1;

    unsigned char *out = (unsigned char*)malloc((size_t)bound);
    if (!out) return -1;

    int clen = lzav_compress_hi((const void*)input, (void*)out, src_len, bound);
    if (clen <= 0) {
        free(out);
        return -1;
    }

    *output = out;
    *output_size = (size_t)clen;
    return 0;
}
