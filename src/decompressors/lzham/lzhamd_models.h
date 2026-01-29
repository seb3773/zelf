// lzhamd_models.h - Quasi-adaptive Huffman model (pure C) for LZHAM decoder
// C99, Linux x86_64, nostdlib friendly (workspace-backed). Minimal size
// oriented.
#ifndef LZHAMD_MODELS_H
#define LZHAMD_MODELS_H

#include "lzhamd_huffman.h"
#include "lzhamd_internal.h"
#include "lzhamd_symbol_codec.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Adaptive Huffman model state. Uses lzhamd_memctx_t for table/scratch
// allocations.
typedef struct lzhamd_model_s {
  uint32_t num_syms;
  uint16_t sym_freq[LZHAMD_HUFF_MAX_SYMS];
  uint32_t total_count;
  uint16_t max_update_interval;
  uint16_t adapt_rate;
  uint32_t update_cycle;         // uint32 to match C++, can exceed 65535!
  uint32_t symbols_until_update; // uint32 to match C++
  uint32_t max_cycle;         // uint32 to match C++, max value is 32767 though
  uint8_t decoder_table_bits; // hint for lookup table size

  lzhamd_decoder_tables_t dt; // decoder tables

  // Workspace reference
  lzhamd_memctx_t *mem;
  // Pre-allocated scratch buffers for build_tables (allocated once during init)
  int *scratch_buf;       // LZHAMD_HUFF_MAX_SYMS * 3 ints
  uint8_t *codesizes_buf; // LZHAMD_HUFF_MAX_SYMS bytes
  uint16_t *sorted_buf;   // LZHAMD_HUFF_MAX_SYMS uint16s
} lzhamd_model_t;

// Initialize model with "all ones" frequencies and build initial tables.
// Returns 1 on success, 0 on failure.
int lzhamd_model_init(lzhamd_model_t *m, uint32_t num_syms,
                      uint32_t max_update_interval, uint32_t adapt_rate,
                      lzhamd_memctx_t *mem);

// Reset frequencies to 1 and rebuild tables immediately.
int lzhamd_model_reset_all(lzhamd_model_t *m);

// Decode a symbol and update the model (freqs/update cycle/tables).
// Returns decoded symbol in [0..num_syms-1]. 0xFFFFFFFF on failure.
uint32_t lzhamd_model_decode(lzhamd_model_t *m, lzhamd_bitreader_t *br);

// Force update cycle reset (used on sync/flush), keeping current frequencies.
void lzhamd_model_reset_update_rate(lzhamd_model_t *m);

#ifdef __cplusplus
}
#endif

#endif // LZHAMD_MODELS_H
