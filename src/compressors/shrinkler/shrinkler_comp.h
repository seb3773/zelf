#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simple C API to compress a memory buffer using Shrinkler (data mode).
// - Fixed references (-r) to 400000 as requested.
// - Parity context disabled (byte-oriented), matching our decompressor.
// The output format is Shrinkler data file with header (magic 'Shri' + metadata).
// Returns 1 on success, 0 on failure. out_buf is allocated with malloc and must be free()'d by caller.

typedef struct {
    int iterations;      // default 3
    int length_margin;   // default 3
    int skip_length;     // default 3000
    int effort;          // default 300
    int same_length;     // default 30
} shrinkler_comp_params;

int shrinkler_compress_memory(const uint8_t* in,
                              size_t in_size,
                              uint8_t** out_buf,
                              size_t* out_size,
                              const shrinkler_comp_params* opt);

// Optional: register a progress callback receiving absolute percentage [0..100].
// Pass NULL to disable. The callback is invoked from the Shrinkler compressor thread.
void shrinkler_set_progress_cb(void (*cb)(int pct));

#ifdef __cplusplus
}
#endif
