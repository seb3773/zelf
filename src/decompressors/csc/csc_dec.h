#ifndef _CSC_DEC_H_
#define _CSC_DEC_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns the required workspace size in bytes to decompress the given data.
 * Returns -1 if the header is invalid or src_len is too small.
 */
intptr_t CSC_Dec_GetWorkspaceSize(const void *src, size_t src_len);

/*
 * Decompresses the data.
 * src: Compressed data pointer
 * src_len: Compressed data size
 * dst: Destination buffer
 * dst_cap: Destination buffer capacity
 * workmem: Workspace memory (must be at least CSC_Dec_GetWorkspaceSize() bytes)
 *
 * Returns the number of bytes written to dst (>= 0), or -1 on error.
 */
intptr_t CSC_Dec_Decompress(const void *src, size_t src_len, void *dst,
                            size_t dst_cap, void *workmem);

#ifdef __cplusplus
}
#endif

#endif
