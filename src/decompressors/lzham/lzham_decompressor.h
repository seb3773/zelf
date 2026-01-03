// lzham_decompressor.h - Pure C (C99) LZHAM decompressor API (stream +
// one-shot) Target: Linux x86_64. Objective: minimal compiled size, no C++
// usage at all. This API is independent from upstream headers and does not
// require lzham.h. Default mode: UNBUFFERED ONLY (output buffer must hold
// entire decompressed stream) for lower RAM and fewer copies.
//
// Constraints and assumptions:
// - Complexity: O(n) per call w.r.t. processed bytes.
// - Alignment: none beyond standard C; caller can align buffers as desired.
// - Thread-safety: one stream per thread.
// - No dynamic allocation: caller MUST provide a workspace buffer (nostdlib
// friendly).

#ifndef LZHAM_MIN_C_WRAP_DECOMPRESSOR_H
#define LZHAM_MIN_C_WRAP_DECOMPRESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// Minimal constants kept compatible with LZHAM's bitstream.
enum { LZHAM_MIN_DICT_SIZE_LOG2 = 15, LZHAM_MAX_DICT_SIZE_LOG2_X64 = 29 };

// Decompress flags (subset)
enum {
  LZHAMD_FLAG_OUTPUT_UNBUFFERED =
      1u
      << 0, // output buffer is the whole destination (single contiguous buffer)
  LZHAMD_FLAG_READ_ZLIB_STREAM =
      1u << 2 // parse zlib header/footer for LZHAM streams
};

// Status codes (subset, values chosen for C-only variant)
typedef enum lzhamd_status_e {
  LZHAMD_STATUS_NOT_FINISHED = 0,
  LZHAMD_STATUS_HAS_MORE_OUTPUT,
  LZHAMD_STATUS_NEEDS_MORE_INPUT,
  LZHAMD_STATUS_SUCCESS = 100,
  LZHAMD_STATUS_FAILED_INITIALIZING = -1,
  LZHAMD_STATUS_FAILED_DEST_BUF_TOO_SMALL = -2,
  LZHAMD_STATUS_FAILED_EXPECTED_MORE_RAW_BYTES = -3,
  LZHAMD_STATUS_FAILED_BAD_CODE = -4,
  LZHAMD_STATUS_FAILED_BAD_RAW_BLOCK = -6,
  LZHAMD_STATUS_FAILED_BAD_COMP_BLOCK_SYNC_CHECK = -7,
  LZHAMD_STATUS_FAILED_BAD_ZLIB_HEADER = -8,
  LZHAMD_STATUS_FAILED_NEED_SEED_BYTES = -9,
  LZHAMD_STATUS_FAILED_BAD_SEED_BYTES = -10,
  LZHAMD_STATUS_FAILED_BAD_SYNC_BLOCK = -11,
  LZHAMD_STATUS_INVALID_PARAMETER = -12
} lzhamd_status_t;

// Opaque stream handle for decompression.
struct lzhamd_stream;
typedef struct lzhamd_stream lzhamd_stream_t;

// Parameters for stream creation (must match compression settings: dict size
// and table update config).
typedef struct lzhamd_params_s {
  uint32_t struct_size;
  uint32_t dict_size_log2;    // [15..29] on x64
  uint32_t table_update_rate; // 0=default, or [1..20]
  uint32_t decompress_flags;  // OR of LZHAMD_FLAG_*
  uint32_t num_seed_bytes;    // optional seed dictionary size
  const void *pSeed_bytes;    // optional seed dictionary pointer
  // Advanced (set 0 unless you know what you're doing):
  uint32_t table_max_update_interval;
  uint32_t table_update_interval_slow_rate;
  // Workspace (required, no malloc): provide a scratch buffer for internal
  // tables/state
  void *workspace;       // non-NULL
  size_t workspace_size; // >= LZHAMD_WORKSPACE_MIN
} lzhamd_params_t;

// Recommended workspace size (safe upper bound for Huffman tables, decode
// state, bit I/O, etc.). Tunable later if needed; choose 64 KiB to keep code
// minimal and avoid dynamic allocations.
#ifndef LZHAMD_WORKSPACE_MIN
#define LZHAMD_WORKSPACE_MIN (64u * 1024u)
#endif

// Create a decompressor stream.
// returns: stream handle or NULL on failure
lzhamd_stream_t *lzhamd_create(const lzhamd_params_t *p);

// Quickly reinitialize an existing decompressor with same parameters.
lzhamd_stream_t *lzhamd_reset(lzhamd_stream_t *s, const lzhamd_params_t *p);

// Streaming decompression call.
// On input: *pIn_size = available input bytes, *pOut_size = available output
// capacity. On output: *pIn_size = consumed bytes, *pOut_size = produced bytes.
// finish_flag: set to non-zero when no more input will be supplied.
lzhamd_status_t lzhamd_decompress(lzhamd_stream_t *s, const uint8_t *in_ptr,
                                  size_t *pIn_size, uint8_t *out_ptr,
                                  size_t *pOut_size, int finish_flag);

// Destroy stream.
void lzhamd_destroy(lzhamd_stream_t *s);

// One-shot decompression. On input: *pDst_len = capacity. On output: actual
// size.
lzhamd_status_t lzhamd_decompress_memory(const lzhamd_params_t *p, uint8_t *dst,
                                         size_t *pDst_len, const uint8_t *src,
                                         size_t src_len);

// Convenience initializer: fills params with safe defaults.
// - dict_size_log2: if 0, defaults to 23 (8 MiB). Must be within [15..29].
// - decompress_flags: sets UNBUFFERED by default.
// - workspace is required (no malloc).
static inline void lzhamd_params_init_default(lzhamd_params_t *p,
                                              void *workspace,
                                              size_t workspace_size,
                                              uint32_t dict_size_log2) {
  if (!p)
    return;
  p->struct_size = (uint32_t)sizeof(lzhamd_params_t);
  p->dict_size_log2 = dict_size_log2 ? dict_size_log2 : 23u; // 8 MiB default
  if (p->dict_size_log2 < LZHAM_MIN_DICT_SIZE_LOG2)
    p->dict_size_log2 = LZHAM_MIN_DICT_SIZE_LOG2;
  if (p->dict_size_log2 > LZHAM_MAX_DICT_SIZE_LOG2_X64)
    p->dict_size_log2 = LZHAM_MAX_DICT_SIZE_LOG2_X64;
  p->table_update_rate = 0u;
  p->decompress_flags = LZHAMD_FLAG_OUTPUT_UNBUFFERED;
  p->num_seed_bytes = 0u;
  p->pSeed_bytes = 0;
  p->table_max_update_interval = 0u;
  p->table_update_interval_slow_rate = 0u;
  p->workspace = workspace;
  p->workspace_size = workspace_size;
}

#ifdef __cplusplus
}
#endif

#endif // LZHAM_MIN_C_WRAP_DECOMPRESSOR_H
