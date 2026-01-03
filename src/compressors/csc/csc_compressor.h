#ifndef _CSC_COMPRESSOR_H_
#define _CSC_COMPRESSOR_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Compression levels: 1 (fastest/worst) to 5 (slowest/best). Default: 2.
// Dictionary size: in bytes. Default: 64MB (64*1024*1024).
typedef struct {
    int level;
    uint32_t dict_size;
} CSCCompressOptions;

// Simple buffer-to-buffer compression.
// Returns compressed size, or 0 on failure.
// If dst_capacity is too small, returns 0.
// Recommended dst_capacity: src_len + 64KB + src_len/8 roughly.
size_t CSC_Compress(const void *src, size_t src_len, void *dst, size_t dst_capacity, const CSCCompressOptions *opts);

#ifdef __cplusplus
}
#endif

#endif
