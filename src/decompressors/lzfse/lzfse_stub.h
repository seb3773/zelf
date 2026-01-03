/*
 * LZFSE Decompressor Stub API
 * Minimal, nostdlib-friendly interface.
 */

#ifndef LZFSE_STUB_H
#define LZFSE_STUB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(X)
#pragma warning(disable : 4068)
#endif

/*
 * Get the required scratch buffer size to decompress using LZFSE.
 * Returns the size in bytes.
 */
size_t lzfse_decode_scratch_size(void);

/*
 * Decompress a buffer using LZFSE.
 *
 * @param dst_buffer      Pointer to destination buffer.
 * @param dst_size        Capacity of destination buffer.
 * @param src_buffer      Pointer to source (compressed) buffer.
 * @param src_size        Size of source buffer in bytes.
 * @param scratch_buffer  Pointer to scratch space. MUST NOT BE NULL.
 *                        Must be at least lzfse_decode_scratch_size() bytes.
 *
 * @return Number of bytes written to dst_buffer on success.
 *         0 on failure (error or dst buffer too small).
 *
 * Note: strict nostdlib mode. No internal malloc. scratch_buffer is mandatory.
 */
size_t lzfse_decode_buffer(uint8_t *__restrict dst_buffer, size_t dst_size,
                           const uint8_t *__restrict src_buffer,
                           size_t src_size, void *__restrict scratch_buffer);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LZFSE_STUB_H */
