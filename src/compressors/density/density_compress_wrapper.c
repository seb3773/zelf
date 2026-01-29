#include "density_compress_wrapper.h"
#include "density_api.h"
#include <stdlib.h>
#include <string.h>

/* Host-side allocator hooks for Density (packer runtime). We deliberately pass
 * explicit malloc/free because our Density stream implementation does not
 * fallback to libc when mem_alloc is NULL (stub constraint). */
static void *host_alloc(size_t sz) { return malloc(sz ? sz : 1); }
static void host_free(void *p) { if (p) free(p); }

/* Simple grow-and-retry wrapper around Density buffer API, Lion + DEFAULT block */
int density_compress_c(const unsigned char *in, size_t in_sz,
                       unsigned char **out, size_t *out_sz) {
  if (!in || !out || !out_sz)
    return -1;
  *out = NULL;
  *out_sz = 0;

  size_t cap = in_sz + (in_sz >> 3) + 65536u;
  if (cap < DENSITY_MINIMUM_OUTPUT_BUFFER_SIZE)
    cap = DENSITY_MINIMUM_OUTPUT_BUFFER_SIZE;

  unsigned char *buf = (unsigned char *)malloc(cap);
  if (!buf)
    return -1;

  for (int tries = 0; tries < 6; ++tries) {
    density_buffer_processing_result r = density_buffer_compress(
        in, (uint_fast64_t)in_sz, buf, (uint_fast64_t)cap,
        DENSITY_COMPRESSION_MODE_LION_ALGORITHM,
        DENSITY_BLOCK_TYPE_DEFAULT, host_alloc, host_free);
    if (r.state == DENSITY_BUFFER_STATE_OK && r.bytesRead == (uint_fast64_t)in_sz) {
      size_t used = (size_t)r.bytesWritten;
      unsigned char *shr = (unsigned char *)malloc(used ? used : 1);
      if (!shr) {
        free(buf);
        return -1;
      }
      memcpy(shr, buf, used);
      free(buf);
      *out = shr;
      *out_sz = used;
      return 0;
    }

    if (r.state != DENSITY_BUFFER_STATE_ERROR_OUTPUT_BUFFER_TOO_SMALL &&
        r.state != DENSITY_BUFFER_STATE_OK) {
      free(buf);
      return -1;
    }
    size_t new_cap = cap + (cap >> 1) + 65536u;
    unsigned char *nb = (unsigned char *)realloc(buf, new_cap);
    if (!nb) {
      free(buf);
      return -1;
    }
    buf = nb;
    cap = new_cap;
  }

  free(buf);
  return -1;
}
