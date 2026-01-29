// lzhamd_huffman.h - Pure C Huffman utilities for LZHAM decompressor
// C99. No C++.
#ifndef LZHAMD_HUFFMAN_H
#define LZHAMD_HUFFMAN_H

#include "lzhamd_symbol_codec.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { LZHAMD_HUFF_MAX_SYMS = 1024 };
enum { LZHAMD_HUFF_MAX_CODE_SIZE = 16 };

typedef struct {
  uint32_t num_codes[33];
} lzhamd_code_size_hist_t;

static inline void lzhamd_csh_clear(lzhamd_code_size_hist_t *h) {
  for (int i = 0; i < 33; i++)
    h->num_codes[i] = 0;
}
void lzhamd_csh_init_from_codes(lzhamd_code_size_hist_t *h, uint32_t num_syms,
                                const uint8_t *codesizes);
void lzhamd_csh_init_pair(lzhamd_code_size_hist_t *h, uint32_t code_size0,
                          uint32_t total_syms0, uint32_t code_size1,
                          uint32_t total_syms1);

// Compute code sizes from frequencies using Moffat/Katajainen
// "calculate_minimum_redundancy". scratch must be large enough; use
// lzhamd_huffman_scratch_size() to size it.
uint32_t lzhamd_huffman_scratch_size(void);
int lzhamd_generate_huffman_codes(void *scratch, uint32_t num_syms,
                                  const uint16_t *freq, uint8_t *codesizes,
                                  uint32_t *pMaxCodeSize, uint32_t *pTotalFreq,
                                  lzhamd_code_size_hist_t *out_hist);

int lzhamd_limit_max_code_size(uint32_t num_syms, uint8_t *codesizes,
                               uint32_t max_code_size);

// Decoder tables

// Workspace-backed memory context (no malloc).
typedef struct {
  uint8_t *cur;
  uint8_t *end;
} lzhamd_memctx_t;

static inline void lzhamd_memctx_init(lzhamd_memctx_t *m, void *buf,
                                      size_t size) {
  m->cur = (uint8_t *)buf;
  m->end = m->cur + size;
}

static inline void *lzhamd_memctx_alloc(lzhamd_memctx_t *m, size_t size,
                                        size_t align) {
  uintptr_t p = (uintptr_t)m->cur;
  uintptr_t a = (align ? align : 1) - 1;
  p = (p + a) & ~a;
  if ((uint8_t *)p + size > m->end)
    return 0;
  m->cur = (uint8_t *)p + size;
  return (void *)p;
}

typedef struct {
  uint32_t num_syms;
  uint32_t total_used_syms;
  uint32_t table_bits;
  uint32_t table_shift;
  uint32_t table_max_code;
  uint32_t decode_start_code_size;
  uint8_t min_code_size;
  uint8_t max_code_size;
  uint32_t max_codes[LZHAMD_HUFF_MAX_CODE_SIZE + 1];
  int val_ptrs[LZHAMD_HUFF_MAX_CODE_SIZE + 1];
  uint32_t cur_lookup_size;
  uint32_t *lookup; // [1<<table_bits], each entry: symbol | (code_size<<16)
  uint32_t cur_sorted_size;
  uint16_t *sorted_symbol_order; // size total_used_syms rounded up to power of
                                 // 2, <= num_syms
} lzhamd_decoder_tables_t;

void lzhamd_dt_init(lzhamd_decoder_tables_t *dt);
void lzhamd_dt_release(
    lzhamd_decoder_tables_t *dt); // no-op for workspace-backed usage
int lzhamd_generate_decoder_tables(uint32_t num_syms, const uint8_t *codesizes,
                                   lzhamd_decoder_tables_t *dt,
                                   uint32_t table_bits,
                                   const lzhamd_code_size_hist_t *hist,
                                   int sym_freq_all_ones, lzhamd_memctx_t *mem);

// Decode a single symbol using prebuilt decoder tables and the bitreader.
// Returns symbol in [0..num_syms-1]. Consumes the required number of bits.
static inline uint32_t
lzhamd_huff_decode_symbol(const lzhamd_decoder_tables_t *dt,
                          lzhamd_bitreader_t *br) {
  // Fast path: table lookup
  if (dt->table_bits) {
    uint32_t key = lzhamd_br_peek_bits(br, dt->table_bits);
    uint32_t e = dt->lookup[key];
    if (e != 0xFFFFFFFFU) {
      uint32_t csize = e >> 16;
      uint32_t sym = e & 0xFFFFU;
      lzhamd_br_drop_bits(br, csize);
      return sym;
    }
  }
  // Slow path: incremental by code size using max_codes[] and val_ptrs[]
  // IMPORTANT: k is computed ONCE and stays constant throughout the loop
  // (matching C++ upstream)
  uint32_t k =
      lzhamd_br_peek_bits(br, 16) + 1U; // upstream uses +1 for comparison
  uint32_t len = dt->decode_start_code_size;
  while (k > dt->max_codes[len - 1])
    len++;
  uint32_t code = lzhamd_br_peek_bits(br, 16);
  uint32_t idx = (uint32_t)dt->val_ptrs[len - 1] + (code >> (16 - len));
  lzhamd_br_drop_bits(br, len);
  return dt->sorted_symbol_order[idx];
}

#ifdef __cplusplus
}
#endif

#endif // LZHAMD_HUFFMAN_H
