#include <stdlib.h>
#include <string.h>
#include <lz4.h>
#include <lz4hc.h>
#include "lz4_compress_wrapper.h"

static lz4_progress_cb_t g_cb = 0;

void lz4_set_progress_cb(lz4_progress_cb_t cb) {
    g_cb = cb;
}

int lz4_compress_c(const unsigned char *in, size_t in_sz,
                   unsigned char **out, size_t *out_sz) {
    if (!in || in_sz == 0 || !out || !out_sz) return -1;
    int max = LZ4_compressBound((int)in_sz);
    if (max <= 0) return -1;
    unsigned char *buf = (unsigned char*)malloc((size_t)max);
    if (!buf) return -1;
    if (g_cb) g_cb(0, in_sz);
    int n = LZ4_compress_HC((const char*)in, (char*)buf, (int)in_sz, max, LZ4HC_CLEVEL_MAX);
    if (g_cb) g_cb(in_sz, in_sz);
    if (n <= 0) { free(buf); return -1; }
    *out = buf;
    *out_sz = (size_t)n;
    return 0;
}

int lz4_verify_c(const unsigned char *compressed, size_t comp_sz,
                 const unsigned char *original, size_t orig_sz) {
    if (!compressed || !original || comp_sz == 0 || orig_sz == 0) return -1;
    unsigned char *tmp = (unsigned char*)malloc(orig_sz);
    if (!tmp) return -1;
    int dec = LZ4_decompress_safe((const char*)compressed, (char*)tmp, (int)comp_sz, (int)orig_sz);
    int rc = 0;
    if (dec != (int)orig_sz || memcmp(tmp, original, orig_sz) != 0) rc = -1;
    free(tmp);
    return rc;
}
