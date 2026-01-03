#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

// Optional: register a progress callback receiving (processed,total)
void zx0_set_progress_cb(void (*cb)(size_t processed, size_t total));

// Simple C wrapper for ZX0 optimized compressor.
// Returns 0 on success, -1 on failure. Output buffer is malloc()'d.
int zx0_compress_c(const unsigned char *in, size_t in_sz,
                   unsigned char **out, size_t *out_sz);

#ifdef __cplusplus
}
#endif
