#ifndef ZX7B_COMPRESS_H
#define ZX7B_COMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional progress callback: current processed bytes, total bytes */
typedef void (*zx7b_progress_fn)(size_t current, size_t total);

/* Set an optional progress callback used by the encoder. */
void zx7b_set_progress_cb(zx7b_progress_fn cb);

/* In-memory ZX7b compression (pure C). Returns 0 on success, -1 on error. */
int zx7b_compress_c(const unsigned char *input, size_t input_size,
                    unsigned char **output, size_t *output_size);

#ifdef __cplusplus
}
#endif

#endif /* ZX7B_COMPRESS_H */
