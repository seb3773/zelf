// Force O3 optimization for compression (performance critical)
#pragma GCC optimize("O3")

#include "zx0_compress_wrapper.h"
#include "zx0_internal.h"
#include <stdlib.h>
#include <string.h>

#define MAX_OFFSET_ZX0 32640

zx0_progress_fn zx0_progress_cb_global = 0;

void zx0_set_progress_cb(void (*cb)(size_t processed, size_t total)) {
  zx0_progress_cb_global = cb;
}

int zx0_compress_c(const unsigned char *in, size_t in_sz, unsigned char **out,
                   size_t *out_sz) {
  if (!in || in_sz == 0 || !out || !out_sz)
    return -1;

  unsigned char *data = (unsigned char *)malloc(in_sz);
  if (!data)
    return -1;
  memcpy(data, in, in_sz);

  /* Progress start */
  zx0_emit_progress(0, in_sz);

  BLOCK *optimal_chain = zx0_optimize(data, (int)in_sz, 0, MAX_OFFSET_ZX0);

  int out_size_i = 0, delta = 0;
  unsigned char *compressed = zx0_compress(optimal_chain, data, (int)in_sz, 0,
                                           0, 1, &out_size_i, &delta);

  free(data);

  if (!compressed || out_size_i <= 0)
    return -1;

  *out_sz = (size_t)out_size_i;
  *out = compressed;

  /* Progress end */
  zx0_emit_progress(in_sz, in_sz);

  return 0;
}
