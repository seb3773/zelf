// COMPLETE REWRITE of lzhamd_model_build_tables following C++ update_tables()
// line-by-line From lzham_symbol_codec.cpp lines 565-685

static int lzhamd_model_build_tables_new(lzhamd_model_t *m,
                                         int force_update_cycle,
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

  // Lines 576-577: code_size_histogram code_size_hist; code_size_hist.clear()
  lzhamd_code_size_hist_t code_size_hist;
  lzhamd_csh_clear(&code_size_hist);

  // Lines 579-594: Shortcut if sym_freq_all_ones && m_total_syms >= 2
  if (sym_freq_all_ones && (m->num_syms >= 2)) {
    // Line 582: uint base_code_size = math::floor_log2i(m_total_syms)
    uint32_t base_code_size = 0;
    uint32_t t = m->num_syms;
    t >>= 1;
    while (t) {
      base_code_size++;
      t >>= 1;
    }

    // Line 583: uint num_left = m_total_syms - (1 << base_code_size)
    uint32_t num_left = m->num_syms - (1u << base_code_size);

    // Line 584: num_left *= 2
    num_left *= 2;

    // Lines 585-586: if (num_left > m_total_syms) num_left = m_total_syms
    if (num_left > m->num_syms)
      num_left = m->num_syms;

    // Lines 588-589: memset code_sizes
    memset(&m->codesizes_buf[0], base_code_size + 1, num_left);
    memset(&m->codesizes_buf[num_left], base_code_size, m->num_syms - num_left);

    // Line 591: code_size_hist.init(...)
    lzhamd_csh_init_pair(&code_size_hist, base_code_size,
                         m->num_syms - num_left, base_code_size + 1, num_left);

    // Line 593: max_code_size = base_code_size + (num_left ? 1 : 0)
    max_code_size = base_code_size + (num_left ? 1 : 0);
  }

  // Line 596: bool status = false
  int status = 0;

  // Lines 597-629: if (!max_code_size) { generate huffman codes }
  if (!max_code_size) {
    // Lines 599-600: allocate scratch (we use pre-allocated buffer)
    void *pTables = m->scratch_buf;

    // Line 602: uint total_freq = 0
    uint32_t total_freq = 0;

    // Line 603: status = generate_huffman_codes(...)
    status = lzhamd_generate_huffman_codes(pTables, m->num_syms, m->sym_freq,
                                           m->codesizes_buf, &max_code_size,
                                           &total_freq, &code_size_hist);

    // Lines 606-610: if (!status || total_freq != m_total_count) return false
    if (!status || (total_freq != m->total_count))
      return 0;

    // Lines 612-628: if (max_code_size > cMaxExpectedHuffCodeSize) limit it
    if (max_code_size > LZHAMD_HUFF_MAX_CODE_SIZE) {
      // Line 614: limit_max_code_size
      status = lzhamd_limit_max_code_size(m->num_syms, m->codesizes_buf,
                                          LZHAMD_HUFF_MAX_CODE_SIZE);
      if (!status)
        return 0;

      // Lines 622-623: rebuild histogram after limiting
      lzhamd_csh_clear(&code_size_hist);
      lzhamd_csh_init(&code_size_hist, m->num_syms, m->codesizes_buf);

      // Lines 625-627: find actual max_code_size after limiting
      for (max_code_size = LZHAMD_HUFF_MAX_CODE_SIZE; max_code_size >= 1;
           max_code_size--) {
        if (code_size_hist.num_codes[max_code_size])
          break;
      }
    }
  }

  // Lines 650-660: Update cycle calculation
  if (force_update_cycle >= 0) {
    // Line 651: m_symbols_until_update = m_update_cycle = force_update_cycle
    m->symbols_until_update = m->update_cycle = (uint32_t)force_update_cycle;
  } else {
    // Line 654: m_update_cycle = (31U + m_update_cycle * MAX(32U, adapt_rate))
    // >> 5U
    uint32_t adapt =
        m->adapt_rate ? m->adapt_rate : 64; // LZHAM_DEFAULT_ADAPT_RATE
    adapt = max_u32(32, adapt);
    m->update_cycle = (31U + m->update_cycle * adapt) >> 5U;

    // Lines 656-657: if (m_update_cycle > m_max_cycle) m_update_cycle =
    // m_max_cycle
    if (m->update_cycle > m->max_cycle)
      m->update_cycle = m->max_cycle;

    // Line 659: m_symbols_until_update = m_update_cycle
    m->symbols_until_update = m->update_cycle;
  }

  // Lines 662-675: Generate decoder tables (we're always decoding, not
  // encoding) Line 666: uint actual_table_bits = m_decoder_table_bits
  uint32_t actual_table_bits = m->decoder_table_bits;

  // Lines 669-672: Cost heuristic to decide if table is worth building
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

  // Lines 677-682: if (!status) return false
  if (!status)
    return 0;

  // Line 684: return true
  return 1;
}
