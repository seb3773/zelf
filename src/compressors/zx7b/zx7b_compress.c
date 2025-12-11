/*
 * ZX7b in-memory compression (pure C wrapper)
 * Builds on zx7b_lib.c optimizer/emitter.
 */
// Force O3 optimization for compression (performance critical)
#pragma GCC optimize("O3")

#include "zx7b_compress.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Global progress callback for zx7b_lib.c */
zx7b_progress_fn g_zx7b_progress_cb = NULL;

void zx7b_set_progress_cb(zx7b_progress_fn cb) { g_zx7b_progress_cb = cb; }

/* Forward decls from zx7b_lib.c */
typedef struct optimal_t {
  size_t bits;
  int offset;
  int len;
} Optimal;

Optimal *optimize(unsigned char *input_data, size_t input_size);
unsigned char *compress(Optimal *optimal, unsigned char *input_data,
                        size_t input_size, size_t *output_size);

int zx7b_compress_c(const unsigned char *input, size_t input_size,
                    unsigned char **output, size_t *output_size) {
  if (!input || !output || !output_size || input_size == 0) {
    return -1;
  }

  unsigned char *input_copy = (unsigned char *)malloc(input_size);
  if (!input_copy)
    return -1;
  memcpy(input_copy, input, input_size);

  /* Reverse input for backwards compression */
  for (size_t i = 0; i < input_size / 2; i++) {
    unsigned char tmp = input_copy[i];
    input_copy[i] = input_copy[input_size - 1 - i];
    input_copy[input_size - 1 - i] = tmp;
  }

  Optimal *optimal = optimize(input_copy, input_size);
  if (!optimal) {
    free(input_copy);
    return -1;
  }

  size_t comp_size = 0;
  unsigned char *compressed =
      compress(optimal, input_copy, input_size, &comp_size);

  free(optimal);
  free(input_copy);

  if (!compressed)
    return -1;

  /* Reverse output to normal order */
  for (size_t i = 0; i < comp_size / 2; i++) {
    unsigned char tmp = compressed[i];
    compressed[i] = compressed[comp_size - 1 - i];
    compressed[comp_size - 1 - i] = tmp;
  }

  *output = compressed;
  *output_size = comp_size;
  return 0;
}
