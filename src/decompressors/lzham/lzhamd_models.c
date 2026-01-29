// lzhamd_models.c - Quasi-adaptive Huffman model (pure C) for LZHAM decoder
// C99, nostdlib-friendly. Uses caller-provided workspace via lzhamd_memctx_t.

#include "lzhamd_models.h"

#ifdef LZHAM_DEBUG
#include <stdio.h>
static void lzhamd_dump_huff_tables(const lzhamd_decoder_tables_t *dt, int id) {
  fprintf(stderr, "[LZHAM_DBG] Table Dump Model %d (bits=%d):\n", id,
          dt->table_bits);
  for (int i = 0; i < 16; i++) {
    if (dt->max_codes[i] == 0)
      continue;
    fprintf(stderr, "  Len %d: max=%08X val_ptr=%d\n", i + 1, dt->max_codes[i],
            dt->val_ptrs[i]);
  }
}
#endif

static inline uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }
static inline uint32_t max_u32(uint32_t a, uint32_t b) { return a > b ? a : b; }

// Forward declaration
static void lzhamd_model_rescale(lzhamd_model_t *m);

// Build Huffman code sizes from current frequencies and generate decoder
// tables. NOTE: scratch buffers allocated from workspace to avoid stack
// overflow in static stubs
// Rewritten from C++ quasi_adaptive_huffman_data_model::update_tables()
// Lines 565-685 of lzham_symbol_codec.cpp
static int lzhamd_model_build_tables(lzhamd_model_t *m, int force_update_cycle,
                                     int sym_freq_all_ones) {
  if (!m || !m->mem)
    return 0;

  // Line 568: m_total_count += m_update_cycle
  m->total_count += m->update_cycle;

  // Lines 571-572: while (m_total_count >= 32768) rescale()
  while (m->total_count >= 32768)
    lzhamd_model_rescale(m);

  // Line 574: uint max_code_size = 0
  uint32_t max_code_size = 0;

  // Lines 576-577: code_size_histogram
  lzhamd_code_size_hist_t code_size_hist;
  lzhamd_csh_clear(&code_size_hist);

  // Lines 579-594: Shortcut if sym_freq_all_ones
  if (sym_freq_all_ones && (m->num_syms >= 2)) {
    // Line 582: uint base_code_size = math::floor_log2i(m_total_syms)
    uint32_t base_code_size = 0;
    uint32_t t = m->num_syms;
    t >>= 1;
    while (t) {
      base_code_size++;
      t >>= 1;
    }

    // Line 583-586
    uint32_t num_left = m->num_syms - (1u << base_code_size);
    num_left *= 2;
    if (num_left > m->num_syms)
      num_left = m->num_syms;

    // Lines 588-589: memset code_sizes (C++ doesn't zero first!)
    memset(&m->codesizes_buf[0], base_code_size + 1, num_left);
    memset(&m->codesizes_buf[num_left], base_code_size, m->num_syms - num_left);

    // Line 591
    lzhamd_csh_init_pair(&code_size_hist, base_code_size,
                         m->num_syms - num_left, base_code_size + 1, num_left);

    // Line 593
    max_code_size = base_code_size + (num_left ? 1 : 0);
  }

  // Line 596: bool status = false
  int status = 0;

  // Lines 597-629: if (!max_code_size) generate huffman codes
  if (!max_code_size) {
    // Use pre-allocated scratch buffer
    void *pTables = m->scratch_buf;

    // Line 602: uint total_freq = 0
    uint32_t total_freq = 0;

    // Line 603: generate_huffman_codes
    status = lzhamd_generate_huffman_codes(pTables, m->num_syms, m->sym_freq,
                                           m->codesizes_buf, &max_code_size,
                                           &total_freq, &code_size_hist);

    // Lines 606-610: validation
    if (!status || (total_freq != m->total_count))
      return 0;
  }

  // Lines 612-628: limit max_code_size (applies to BOTH shortcut and generated
  // codes!)
  if (max_code_size > LZHAMD_HUFF_MAX_CODE_SIZE) {
    status = lzhamd_limit_max_code_size(m->num_syms, m->codesizes_buf,
                                        LZHAMD_HUFF_MAX_CODE_SIZE);
    if (!status)
      return 0;

    // Lines 622-627: rebuild histogram after limiting
    lzhamd_csh_clear(&code_size_hist);
    lzhamd_csh_init_from_codes(&code_size_hist, m->num_syms, m->codesizes_buf);

    for (max_code_size = LZHAMD_HUFF_MAX_CODE_SIZE; max_code_size >= 1;
         max_code_size--) {
      if (code_size_hist.num_codes[max_code_size])
        break;
    }
  }

  // Lines 650-660: Update cycle calculation
  if (force_update_cycle >= 0) {
    m->symbols_until_update = m->update_cycle = (uint32_t)force_update_cycle;
  } else {
    // Line 654: m_update_cycle = (31U + m_update_cycle * MAX(32U, adapt_rate))
    // >> 5U
    uint32_t adapt = m->adapt_rate ? m->adapt_rate : 64;
    adapt = max_u32(32, adapt);
    m->update_cycle = (31U + m->update_cycle * adapt) >> 5U;

    // Lines 656-657
    if (m->update_cycle > m->max_cycle)
      m->update_cycle = m->max_cycle;

    // Line 659
    m->symbols_until_update = m->update_cycle;
  }

  // Lines 666-675: Generate decoder tables with cost heuristic
  uint32_t actual_table_bits = m->decoder_table_bits;

  // Lines 669-672: Cost calculation (C++ uses floor_log2i, we use
  // __builtin_clz)
  uint32_t cost_to_use_table = (1u << actual_table_bits) + 64;
  uint32_t cost_to_not_use_table =
      m->symbols_until_update *
      ((m->num_syms > 1) ? (32 - __builtin_clz(m->num_syms - 1)) : 0);
  if (cost_to_not_use_table <= cost_to_use_table)
    actual_table_bits = 0;

  // Line 674: generate_decoder_tables
  lzhamd_dt_init(&m->dt);
  m->dt.sorted_symbol_order = m->sorted_buf;
  m->dt.cur_sorted_size = LZHAMD_HUFF_MAX_SYMS;

  status = lzhamd_generate_decoder_tables(m->num_syms, m->codesizes_buf, &m->dt,
                                          actual_table_bits, &code_size_hist,
                                          sym_freq_all_ones, m->mem);
#ifdef LZHAM_DEBUG
  if (status)
    lzhamd_dump_huff_tables(&m->dt, m->num_syms);
#endif

  // Lines 677-682
  if (!status)
    return 0;

  // Line 684
  return 1;
}

int lzhamd_model_init(lzhamd_model_t *m, uint32_t num_syms,
                      uint32_t max_update_interval, uint32_t adapt_rate,
                      lzhamd_memctx_t *mem) {
  if (!m || !mem || num_syms == 0 || num_syms > LZHAMD_HUFF_MAX_SYMS) {
    return 0;
  }
  m->num_syms = num_syms;
  for (uint32_t i = 0; i < num_syms; i++)
    m->sym_freq[i] = 1;
  m->total_count = 0;
  m->adapt_rate = (uint16_t)(adapt_rate ? adapt_rate : 64);
  m->max_update_interval =
      (uint16_t)(max_update_interval ? max_update_interval : 64);
  m->max_cycle = min_u32(32767u, (max_u32(24u, num_syms) + 6u) *
                                     (uint32_t)m->max_update_interval);
  m->update_cycle = 0;
  m->symbols_until_update = 0;
  m->decoder_table_bits = 0;
  m->mem = mem;
  // Allocate scratch buffers once per model (reused across build_tables calls)
  m->scratch_buf =
      (int *)lzhamd_memctx_alloc(mem, lzhamd_huffman_scratch_size(), 4);
  m->codesizes_buf =
      (uint8_t *)lzhamd_memctx_alloc(mem, LZHAMD_HUFF_MAX_SYMS, 1);
  m->sorted_buf = (uint16_t *)lzhamd_memctx_alloc(
      mem, LZHAMD_HUFF_MAX_SYMS * sizeof(uint16_t), 2);

  if (!m->scratch_buf || !m->codesizes_buf || !m->sorted_buf)
    return 0;

  // C++ init logic match
  m->update_cycle = num_syms;
  uint32_t initial_limit = 16;
  if (m->max_cycle < 16)
    initial_limit = m->max_cycle;

  if (!lzhamd_model_build_tables(m, (int)initial_limit, 1))
    return 0;

  // build_tables sets update_cycle/symbols_until_update if force_update_cycle
  // >= 0
  return 1;
}

int lzhamd_model_reset_all(lzhamd_model_t *m) {
  if (!m || !m->num_syms)
    return 0;
  for (uint32_t i = 0; i < m->num_syms; i++)
    m->sym_freq[i] = 1;
  m->total_count = 0;
  if (!lzhamd_model_build_tables(m, -1, 1))
    return 0;
  uint16_t initial = (uint16_t)min_u32(m->max_cycle, 16u);
  m->update_cycle = initial;
  m->symbols_until_update = initial;
  return 1;
}

static void lzhamd_model_rescale(lzhamd_model_t *m) {
  uint32_t total = 0;
  for (uint32_t i = 0; i < m->num_syms; i++) {
    uint32_t f = (m->sym_freq[i] + 1) >> 1;
    m->sym_freq[i] = (uint16_t)f;
    total += f;
  }
  m->total_count = total;
}

void lzhamd_model_reset_update_rate(lzhamd_model_t *m) {
  if (!m)
    return;
  // Upstream semantics: account for leftover cycle before rescaling
  if (m->symbols_until_update < m->update_cycle) {
    uint32_t delta =
        (uint32_t)m->update_cycle - (uint32_t)m->symbols_until_update;
    m->total_count += delta;
  }
  if (m->total_count > m->num_syms)
    lzhamd_model_rescale(m);
  uint16_t new_cycle = m->update_cycle;
  if (new_cycle > 8)
    new_cycle = 8;
  m->update_cycle = new_cycle;
  m->symbols_until_update = new_cycle;
}

uint32_t lzhamd_model_decode(lzhamd_model_t *m, lzhamd_bitreader_t *br) {
  if (!m || !br)
    return 0xFFFFFFFFu;
  // Decode symbol via Huffman tables
  uint32_t sym = lzhamd_huff_decode_symbol(&m->dt, br);
  if (sym >= m->num_syms)
    return 0xFFFFFFFFu;

  // Update frequency only (upstream tracks total_count at update/reset)
  uint32_t f = (uint32_t)m->sym_freq[sym] + 1;
  if (f > 65535u)
    f = 65535u;
  m->sym_freq[sym] = (uint16_t)f;

  // Update cycle and rebuild tables if needed
  if (--m->symbols_until_update == 0) {
    // Update tables now includes cycle calculation (C++ line 650-660)
    if (!lzhamd_model_build_tables(m, -1, 0))
      return 0xFFFFFFFFu;
  }

  return sym;
}
