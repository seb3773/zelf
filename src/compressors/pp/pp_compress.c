#include "pp_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// In-memory PowerPacker compressor (no external tool)
// Prefer the reference-like encoder for bitstream conformance
extern int pp_compress_mem_ref(const unsigned char* src, unsigned int len,
                               unsigned char** out, unsigned int* out_len,
                               int efficiency, int old_version);

int pp_compress_c(const unsigned char *input, size_t input_size,
                  unsigned char **output, size_t *output_size) {
    if (!input || !output || !output_size || input_size == 0) {
        return -1;
    }
    unsigned char *out = NULL; unsigned int out_len = 0;
    int rc = pp_compress_mem_ref(input, (unsigned int)input_size, &out, &out_len, 3 /*eff=Good*/, 0 /*new*/);
    if (rc != 0 || out == NULL || out_len == 0) {
        return -1;
    }
    *output = out;
    *output_size = (size_t)out_len;
    return 0;
}
