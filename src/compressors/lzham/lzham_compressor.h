// lzham_compressor.h - Minimal C wrapper for LZHAM compressor (UBER-only, stream + one-shot)
// This header is C99-only and targets Linux x86_64.
// Dependencies: include path to LZHAM public headers ("lzham.h").
// Notes:
// - All functions validate input pointers.
// - No dynamic logging; any debug prints must be commented out in code per project rules.
// - This wrapper aims for minimal surface area; it delegates to LZHAM's C API.
// - UBER compression level only (maximum compression).
//
// Constraints and assumptions:
// - Complexity: O(n) per call w.r.t. processed bytes.
// - Alignment: no special alignment required beyond LZHAM's own.
// - Thread-safety: each stream instance is not thread-safe; create one per thread.
// - Must be linked with LZHAM compressor objects from this repo.

#ifndef LZHAM_MIN_C_WRAP_COMPRESSOR_H
#define LZHAM_MIN_C_WRAP_COMPRESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "lzham.h"

// Opaque stream handle for compression.
struct lzhamc_stream;

typedef struct lzhamc_stream lzhamc_stream_t;

// Create a compressor stream.
// params:
//  dict_size_log2: see LZHAM_MIN_DICT_SIZE_LOG2..LZHAM_MAX_DICT_SIZE_LOG2_X64
//  table_update_rate: 0=default or [1,LZHAM_FASTEST_TABLE_UPDATE_RATE]
//  max_helper_threads: -1=max practical, 0=single-threaded
//  compress_flags: bitmask of lzham_compress_flags (0 for default)
// returns: stream handle or NULL on failure
lzhamc_stream_t* lzhamc_create(uint32_t dict_size_log2,
                               uint32_t table_update_rate,
                               int max_helper_threads,
                               uint32_t compress_flags);

// Convenience: create with defaults (UBER level, table_update_rate=0, max_helper_threads=-1, flags=0).
lzhamc_stream_t* lzhamc_create_default(uint32_t dict_size_log2);

// Reset an existing compressor without reallocating state (may improve perf across multiple streams).
// returns non-NULL on success, NULL on failure
lzhamc_stream_t* lzhamc_reset(lzhamc_stream_t* s);

// Streaming compression call.
// On input: *pIn_size = available input bytes, *pOut_size = available output bytes.
// On output: *pIn_size = consumed bytes, *pOut_size = produced bytes.
// finish_flag: set to non-zero to indicate no more input will be supplied.
// returns: lzham_compress_status_t
lzham_compress_status_t lzhamc_compress(lzhamc_stream_t* s,
                                        const uint8_t* in_ptr, size_t* pIn_size,
                                        uint8_t* out_ptr, size_t* pOut_size,
                                        int finish_flag);

// Streaming compression with flush control.
lzham_compress_status_t lzhamc_compress2(lzhamc_stream_t* s,
                                         const uint8_t* in_ptr, size_t* pIn_size,
                                         uint8_t* out_ptr, size_t* pOut_size,
                                         lzham_flush_t flush_type);

// Destroy stream and return adler32 of source data (valid only on success cases).
uint32_t lzhamc_destroy(lzhamc_stream_t* s);

// One-shot convenience (wraps lzham_compress_memory).
// On input: *pDst_len = destination capacity.
// On output: *pDst_len = actual compressed length.
// returns: lzham_compress_status_t
lzham_compress_status_t lzhamc_compress_memory(uint32_t dict_size_log2,
                                               const uint8_t* src, size_t src_len,
                                               uint8_t* dst, size_t* pDst_len,
                                               uint32_t* pAdler32);

#ifdef __cplusplus
}
#endif

#endif // LZHAM_MIN_C_WRAP_COMPRESSOR_H
