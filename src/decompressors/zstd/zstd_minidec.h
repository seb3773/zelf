#ifndef ZSTD_MINIDEC_H
#define ZSTD_MINIDEC_H

#include <stddef.h>

/* Minimal C API used by stubs to decompress a Zstd frame.
 * Returns the number of bytes written to dst (>=0) or -1 on error.
 */
int zstd_decompress_c(const unsigned char *src, size_t src_sz,
                      unsigned char *dst, size_t dst_cap);

/* Peek frame content size if available; returns >=0 size, or -1 if unknown/error */
long long zstd_peek_content_size(const unsigned char *src, size_t src_sz);

/* Provide an external arena buffer to avoid .bss usage in the stub.
 * Must be called before zstd_decompress_c() when building a stub.
 */
void zstd_minidec_set_arena(void *base, size_t size);

#endif /* ZSTD_MINIDEC_H */
