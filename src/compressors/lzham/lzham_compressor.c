// lzham_compressor.c - Minimal C wrapper for LZHAM compressor
// Target: Linux x86_64, C99. See lzham_compressor.h for API.
// Security/robustness: validates pointers, no unbounded libc calls.
// Performance: thin wrapper around LZHAM's C API. No extra copies.

#include "lzham_compressor.h"
#include <stdlib.h>

struct lzhamc_stream {
  lzham_compress_state_ptr st;
};

static void lzhamc_fill_defaults(lzham_compress_params *p,
                                 uint32_t dict_size_log2,
                                 uint32_t table_update_rate,
                                 int max_helper_threads,
                                 uint32_t compress_flags) {
  if (!p)
    return;
  p->m_struct_size = (uint32_t)sizeof(lzham_compress_params);
  p->m_dict_size_log2 = dict_size_log2;
  p->m_level = LZHAM_COMP_LEVEL_UBER;
  p->m_table_update_rate = table_update_rate;
  p->m_max_helper_threads = max_helper_threads;
  // Enable Extreme Parsing and Tradeoff Decompression Rate for Ratio by default
  p->m_compress_flags =
      compress_flags | 2 /* LZHAM_COMP_FLAG_EXTREME_PARSING */ |
      16 /* LZHAM_COMP_FLAG_TRADEOFF_DECOMPRESSION_RATE_FOR_COMP_RATIO */;
  p->m_num_seed_bytes = 0U;
  p->m_pSeed_bytes = 0;
  p->m_table_max_update_interval = 0U;
  p->m_table_update_interval_slow_rate = 0U;
  p->m_extreme_parsing_max_best_arrivals = 8U;
  p->m_fast_bytes = 258U;
}

lzhamc_stream_t *lzhamc_create(uint32_t dict_size_log2,
                               uint32_t table_update_rate,
                               int max_helper_threads,
                               uint32_t compress_flags) {
  lzham_compress_params p;
  lzhamc_fill_defaults(&p, dict_size_log2, table_update_rate,
                       max_helper_threads, compress_flags);
  lzham_compress_state_ptr st = lzham_compress_init(&p);
  if (!st)
    return 0;
  lzhamc_stream_t *s = (lzhamc_stream_t *)malloc(sizeof(lzhamc_stream_t));
  if (!s) {
    (void)lzham_compress_deinit(st);
    return 0;
  }
  s->st = st;
  return s;
}

lzhamc_stream_t *lzhamc_create_default(uint32_t dict_size_log2) {
  return lzhamc_create(dict_size_log2, 0U, -1, 0U);
}

lzhamc_stream_t *lzhamc_reset(lzhamc_stream_t *s) {
  if (!s)
    return 0;
  if (!s->st)
    return 0;
  return lzham_compress_reinit(s->st) ? s : 0;
}

lzham_compress_status_t lzhamc_compress(lzhamc_stream_t *s,
                                        const uint8_t *in_ptr, size_t *pIn_size,
                                        uint8_t *out_ptr, size_t *pOut_size,
                                        int finish_flag) {
  if (!s || !s->st || !pIn_size || !pOut_size)
    return LZHAM_COMP_STATUS_INVALID_PARAMETER;
  const lzham_uint8 *pin = (const lzham_uint8 *)in_ptr;
  lzham_uint8 *pout = (lzham_uint8 *)out_ptr;
  return lzham_compress(s->st, pin, pIn_size, pout, pOut_size,
                        (lzham_bool)(finish_flag != 0));
}

lzham_compress_status_t lzhamc_compress2(lzhamc_stream_t *s,
                                         const uint8_t *in_ptr,
                                         size_t *pIn_size, uint8_t *out_ptr,
                                         size_t *pOut_size,
                                         lzham_flush_t flush_type) {
  if (!s || !s->st || !pIn_size || !pOut_size)
    return LZHAM_COMP_STATUS_INVALID_PARAMETER;
  const lzham_uint8 *pin = (const lzham_uint8 *)in_ptr;
  lzham_uint8 *pout = (lzham_uint8 *)out_ptr;
  return lzham_compress2(s->st, pin, pIn_size, pout, pOut_size, flush_type);
}

uint32_t lzhamc_destroy(lzhamc_stream_t *s) {
  if (!s)
    return 0U;
  uint32_t ad = 0U;
  if (s->st)
    ad = lzham_compress_deinit(s->st);
  free(s);
  return ad;
}

lzham_compress_status_t lzhamc_compress_memory(uint32_t dict_size_log2,
                                               const uint8_t *src,
                                               size_t src_len, uint8_t *dst,
                                               size_t *pDst_len,
                                               uint32_t *pAdler32) {
  if (!src || !dst || !pDst_len)
    return LZHAM_COMP_STATUS_INVALID_PARAMETER;
  lzham_compress_params p;
  lzhamc_fill_defaults(&p, dict_size_log2, 0U, -1, 0U);
  return lzham_compress_memory(&p, (lzham_uint8 *)dst, pDst_len,
                               (const lzham_uint8 *)src, src_len, pAdler32);
}
