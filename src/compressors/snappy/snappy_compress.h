#ifndef SNAPPY_COMPRESS_H
#define SNAPPY_COMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional progress callback: reports (processed,total) bytes */
typedef void (*snappy_progress_fn)(size_t processed, size_t total);

/* Set/clear the global progress callback (NULL to disable) */
void snappy_set_progress_cb(snappy_progress_fn cb);

/*
 * Simple Snappy encoder (pure C)
 * Produces a valid Snappy raw stream:
 *   [varint uncompressed_length] [literals/copies...]
 * Current implementation emits a single literal block (store-only).
 *
 * Returns 0 on success, -1 on error.
 */
int snappy_compress_c(const unsigned char *input, size_t input_size,
                      unsigned char **output, size_t *output_size);

#ifdef __cplusplus
}
#endif

#endif /* SNAPPY_COMPRESS_H */
