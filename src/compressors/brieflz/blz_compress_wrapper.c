// Force O3 optimization for compression (performance critical)
#pragma GCC optimize("O3")

#include "blz_compress_wrapper.h"
#include "brieflz.h"
#include <stdlib.h>
#include <string.h>

/*
 * blz_compress_c()
 * Returns 0 on success; *out is allocated and must be free()'d by caller.
 * Uses BriefLZ level 10 (optimal/slow) as requested.
 */
int blz_compress_c(const unsigned char *in, size_t in_sz, unsigned char **out,
                   size_t *out_sz) {
  /*
   * Complexity: O(n) average with BriefLZ heuristics
   * Dependencies: brieflz.h (blz_pack_level, blz_workmem_size_level,
   * blz_max_packed_size) Alignment: none specific
   */
  if (!in || in_sz == 0 || !out || !out_sz)
    return -1;

  const int level = 10; /* optimal/slow */

  size_t dst_cap = blz_max_packed_size(in_sz);
  unsigned char *dst = (unsigned char *)malloc(dst_cap ? dst_cap : 1);
  if (!dst)
    return -1;

  size_t wm_sz = blz_workmem_size_level(in_sz, level);
  void *workmem = NULL;
  if (wm_sz != (size_t)-1 && wm_sz > 0) {
    workmem = malloc(wm_sz);
    if (!workmem) {
      free(dst);
      return -1;
    }
  }

  unsigned long packed = blz_pack_level((const void *)in, (void *)dst,
                                        (unsigned long)in_sz, workmem, level);
  if (workmem)
    free(workmem);
  if (packed == BLZ_ERROR || packed == 0 || packed > dst_cap) {
    free(dst);
    return -1;
  }

  /* shrink to exact size */
  unsigned char *shrink = (unsigned char *)realloc(dst, packed);
  if (!shrink) {
    /* keep original if realloc fails */
    shrink = dst;
  }
  *out = shrink;
  *out_sz = (size_t)packed;
  return 0;
}
