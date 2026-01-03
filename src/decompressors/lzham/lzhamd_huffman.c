// lzhamd_huffman.c - Pure C Huffman utilities for LZHAM decompressor
// C99. No C++.

#include "lzhamd_huffman.h"
#include <string.h>

static inline uint32_t umin(uint32_t a, uint32_t b) { return a < b ? a : b; }
static inline uint32_t umax(uint32_t a, uint32_t b) { return a > b ? a : b; }
static inline uint32_t ceil_log2_u32(uint32_t x) {
  uint32_t r = 0;
  uint32_t v = x - 1;
  while (v) {
    v >>= 1;
    r++;
  }
  return r;
}
static inline int is_pow2_u32(uint32_t x) { return x && !(x & (x - 1)); }
static inline uint32_t next_pow2_u32(uint32_t x) {
  if (x <= 1)
    return 1;
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

void lzhamd_csh_init_from_codes(lzhamd_code_size_hist_t *h, uint32_t num_syms,
                                const uint8_t *codesizes) {
  if (!h || !codesizes)
    return;
  lzhamd_csh_clear(h);
  for (uint32_t i = 0; i < num_syms; i++) {
    uint32_t c = codesizes[i];
    if (c <= 32)
      h->num_codes[c]++;
  }
}

void lzhamd_csh_init_pair(lzhamd_code_size_hist_t *h, uint32_t code_size0,
                          uint32_t total_syms0, uint32_t code_size1,
                          uint32_t total_syms1) {
  if (!h)
    return;
  lzhamd_csh_clear(h);
  if (code_size0 <= 32)
    h->num_codes[code_size0] += total_syms0;
  if (code_size1 <= 32)
    h->num_codes[code_size1] += total_syms1;
}

// Moffat/Katajainen calculate_minimum_redundancy
static void calculate_minimum_redundancy(int A[], int n) {
  int root, leaf, next, avbl, used, dpth;
  if (n == 0) {
    return;
  }
  if (n == 1) {
    A[0] = 0;
    return;
  }
  A[0] += A[1];
  root = 0;
  leaf = 2;
  for (next = 1; next < n - 1; next++) {
    if (leaf >= n || A[root] < A[leaf]) {
      A[next] = A[root];
      A[root++] = next;
    } else
      A[next] = A[leaf++];
    if (leaf >= n || (root < next && A[root] < A[leaf])) {
      A[next] += A[root];
      A[root++] = next;
    } else
      A[next] += A[leaf++];
  }
  A[n - 2] = 0;
  for (next = n - 3; next >= 0; next--)
    A[next] = A[A[next]] + 1;
  avbl = 1;
  used = dpth = 0;
  root = n - 2;
  next = n - 1;
  while (avbl > 0) {
    while (root >= 0 && A[root] == dpth) {
      used++;
      root--;
    }
    while (avbl > used) {
      A[next--] = dpth;
      avbl--;
    }
    avbl = 2 * used;
    dpth++;
    used = 0;
  }
}

// C++ line 31-41: sym_freq structure (exact match)
typedef struct {
  uint32_t m_freq;
  uint16_t m_left;
  uint16_t m_right;
} sym_freq_t;

// C++ lines 43-159: radix_sort_syms (exact translation)
static sym_freq_t *radix_sort_syms(uint32_t num_syms, sym_freq_t *syms0,
                                   sym_freq_t *syms1) {
  const uint32_t cMaxPasses = 2;
  uint32_t hist[256 * cMaxPasses];

  memset(hist, 0, sizeof(hist[0]) * 256 * cMaxPasses);

  {
    sym_freq_t *p = syms0;
    sym_freq_t *q = syms0 + (num_syms >> 1) * 2;

    for (; p != q; p += 2) {
      const uint32_t freq0 = p[0].m_freq;
      const uint32_t freq1 = p[1].m_freq;

      hist[freq0 & 0xFF]++;
      hist[256 + ((freq0 >> 8) & 0xFF)]++;

      hist[freq1 & 0xFF]++;
      hist[256 + ((freq1 >> 8) & 0xFF)]++;
    }

    if (num_syms & 1) {
      const uint32_t freq = p->m_freq;
      hist[freq & 0xFF]++;
      hist[256 + ((freq >> 8) & 0xFF)]++;
    }
  }

  sym_freq_t *pCur_syms = syms0;
  sym_freq_t *pNew_syms = syms1;

  const uint32_t total_passes = (hist[256] == num_syms) ? 1 : cMaxPasses;

  for (uint32_t pass = 0; pass < total_passes; pass++) {
    const uint32_t *pHist = &hist[pass << 8];
    uint32_t offsets[256];

    uint32_t cur_ofs = 0;
    for (uint32_t i = 0; i < 256; i += 2) {
      offsets[i] = cur_ofs;
      cur_ofs += pHist[i];
      offsets[i + 1] = cur_ofs;
      cur_ofs += pHist[i + 1];
    }

    const uint32_t pass_shift = pass << 3;
    sym_freq_t *p = pCur_syms;
    sym_freq_t *q = pCur_syms + (num_syms >> 1) * 2;

    for (; p != q; p += 2) {
      uint32_t c0 = p[0].m_freq;
      uint32_t c1 = p[1].m_freq;

      if (pass) {
        c0 >>= 8;
        c1 >>= 8;
      }

      c0 &= 0xFF;
      c1 &= 0xFF;

      if (c0 == c1) {
        uint32_t dst_offset0 = offsets[c0];
        offsets[c0] = dst_offset0 + 2;
        pNew_syms[dst_offset0] = p[0];
        pNew_syms[dst_offset0 + 1] = p[1];
      } else {
        uint32_t dst_offset0 = offsets[c0]++;
        uint32_t dst_offset1 = offsets[c1]++;
        pNew_syms[dst_offset0] = p[0];
        pNew_syms[dst_offset1] = p[1];
      }
    }

    if (num_syms & 1) {
      uint32_t c = ((p->m_freq) >> pass_shift) & 0xFF;
      uint32_t dst_offset = offsets[c];
      offsets[c] = dst_offset + 1;
      pNew_syms[dst_offset] = *p;
    }

    sym_freq_t *t = pCur_syms;
    pCur_syms = pNew_syms;
    pNew_syms = t;
  }

  return pCur_syms;
}

// C++ lines 161-171: huffman_work_tables
typedef struct {
  sym_freq_t syms0[LZHAMD_HUFF_MAX_SYMS + 1 + LZHAMD_HUFF_MAX_SYMS];
  sym_freq_t syms1[LZHAMD_HUFF_MAX_SYMS + 1 + LZHAMD_HUFF_MAX_SYMS];
} huffman_work_tables_t;

uint32_t lzhamd_huffman_scratch_size(void) {
  return sizeof(huffman_work_tables_t);
}

// C++ lines 226-283: generate_huffman_codes (exact translation)
int lzhamd_generate_huffman_codes(void *pContext, uint32_t num_syms,
                                  const uint16_t *pFreq, uint8_t *pCodesizes,
                                  uint32_t *max_code_size,
                                  uint32_t *total_freq_ret,
                                  lzhamd_code_size_hist_t *code_size_hist) {
  if ((!num_syms) || (num_syms > LZHAMD_HUFF_MAX_SYMS))
    return 0;

  huffman_work_tables_t *state = (huffman_work_tables_t *)pContext;

  uint32_t max_freq = 0;
  uint32_t total_freq = 0;

  uint32_t num_used_syms = 0;
  for (uint32_t i = 0; i < num_syms; i++) {
    uint32_t freq = pFreq[i];

    if (!freq)
      pCodesizes[i] = 0;
    else {
      total_freq += freq;
      max_freq = (freq > max_freq) ? freq : max_freq;

      sym_freq_t *sf = &state->syms0[num_used_syms];
      sf->m_left = (uint16_t)i;
      sf->m_right = 0xFFFF;
      sf->m_freq = freq;
      num_used_syms++;
    }
  }

  *total_freq_ret = total_freq;

  if (num_used_syms == 1) {
    pCodesizes[state->syms0[0].m_left] = 1;
    return 1;
  }

  sym_freq_t *syms = radix_sort_syms(num_used_syms, state->syms0, state->syms1);

  int x[LZHAMD_HUFF_MAX_SYMS];
  for (uint32_t i = 0; i < num_used_syms; i++)
    x[i] = syms[i].m_freq;

  calculate_minimum_redundancy(x, num_used_syms);

  uint32_t max_len = 0;
  for (uint32_t i = 0; i < num_used_syms; i++) {
    uint32_t len = x[i];
    max_len = (len > max_len) ? len : max_len;

    uint32_t hist_len = (len < 32) ? len : 32;
    code_size_hist->num_codes[hist_len]++;

    pCodesizes[syms[i].m_left] = (uint8_t)len;
  }

  *max_code_size = max_len;

  return 1;
}

int lzhamd_limit_max_code_size(uint32_t num_syms, uint8_t *codesizes,
                               uint32_t max_code_size) {
  const uint32_t cMaxEverCodeSize = 34;
  if (!num_syms || num_syms > LZHAMD_HUFF_MAX_SYMS || max_code_size < 1 ||
      max_code_size > cMaxEverCodeSize)
    return 0;
  uint32_t num_codes[cMaxEverCodeSize + 1];
  memset(num_codes, 0, sizeof(num_codes));
  uint32_t should_limit = 0;
  for (uint32_t i = 0; i < num_syms; i++) {
    uint32_t c = codesizes[i];
    if (c > cMaxEverCodeSize)
      return 0;
    num_codes[c]++;
    if (c > max_code_size)
      should_limit = 1;
  }
  if (!should_limit)
    return 1;
  uint32_t ofs = 0, next_sorted_ofs[cMaxEverCodeSize + 1];
  for (uint32_t i = 1; i <= cMaxEverCodeSize; i++) {
    next_sorted_ofs[i] = ofs;
    ofs += num_codes[i];
  }
  if (ofs < 2 || ofs > LZHAMD_HUFF_MAX_SYMS)
    return 1;
  if (ofs > (1U << max_code_size))
    return 0;
  for (uint32_t i = max_code_size + 1; i <= cMaxEverCodeSize; i++)
    num_codes[max_code_size] += num_codes[i];
  uint32_t total = 0;
  for (uint32_t i = max_code_size; i; --i)
    total += (num_codes[i] << (max_code_size - i));
  if (total == (1U << max_code_size))
    return 1;
  do {
    num_codes[max_code_size]--;
    uint32_t i;
    for (i = max_code_size - 1; i; i--) {
      if (!num_codes[i])
        continue;
      num_codes[i]--;
      num_codes[i + 1] += 2;
      break;
    }
    if (!i)
      return 0;
    total--;
  } while (total != (1U << max_code_size));
  uint8_t new_codesizes[LZHAMD_HUFF_MAX_SYMS];
  uint8_t *p = new_codesizes;
  for (uint32_t i = 1; i <= max_code_size; i++) {
    uint32_t n = num_codes[i];
    if (n) {
      memset(p, (int)i, n);
      p += n;
    }
  }
  for (uint32_t i = 0; i < num_syms; i++) {
    uint32_t c = codesizes[i];
    if (c) {
      uint32_t no = next_sorted_ofs[c];
      next_sorted_ofs[c] = no + 1;
      codesizes[i] = new_codesizes[no];
    }
  }
  return 1;
}

void lzhamd_dt_init(lzhamd_decoder_tables_t *dt) {
  if (!dt)
    return;
  memset(dt, 0, sizeof(*dt));
}
void lzhamd_dt_release(lzhamd_decoder_tables_t *dt) {
  if (!dt)
    return; /* workspace-backed: nothing to free */
  memset(dt, 0, sizeof(*dt));
}

int lzhamd_generate_decoder_tables(uint32_t num_syms, const uint8_t *codesizes,
                                   lzhamd_decoder_tables_t *dt,
                                   uint32_t table_bits,
                                   const lzhamd_code_size_hist_t *hist,
                                   int sym_freq_all_ones,
                                   lzhamd_memctx_t *mem) {
  if (!dt || !codesizes || !num_syms || table_bits > LZHAMD_HUFF_MAX_CODE_SIZE)
    return 0;

  dt->num_syms = num_syms;

  uint32_t min_codes[LZHAMD_HUFF_MAX_CODE_SIZE];
  uint32_t sorted_positions[LZHAMD_HUFF_MAX_CODE_SIZE + 1];

  uint32_t next_code = 0;
  uint32_t total_used_syms = 0;
  uint32_t max_code_size = 0;
  uint32_t min_code_size = 0xFFFFFFFFU;

  for (uint32_t i = 1; i <= LZHAMD_HUFF_MAX_CODE_SIZE; i++) {
    uint32_t n = hist->num_codes[i];
    if (!n) {
      dt->max_codes[i - 1] = 0;
    } else {
      min_code_size = umin(min_code_size, i);
      max_code_size = umax(max_code_size, i);

      min_codes[i - 1] = next_code;

      dt->max_codes[i - 1] = next_code + n - 1;
      // Precise C++ formula for max_codes
      dt->max_codes[i - 1] =
          1 + ((dt->max_codes[i - 1] << (16 - i)) | ((1U << (16 - i)) - 1));

      dt->val_ptrs[i - 1] = total_used_syms;

      sorted_positions[i] = total_used_syms;

      next_code += n;
      total_used_syms += n;
    }
    next_code <<= 1;
  }

  dt->total_used_syms = total_used_syms;

  if (total_used_syms > dt->cur_sorted_size) {
    dt->cur_sorted_size = total_used_syms;
    if (!is_pow2_u32(total_used_syms)) {
      dt->cur_sorted_size = umin(num_syms, next_pow2_u32(total_used_syms));
    }
    dt->sorted_symbol_order = (uint16_t *)lzhamd_memctx_alloc(
        mem, dt->cur_sorted_size * sizeof(uint16_t), 2);
    if (!dt->sorted_symbol_order)
      return 0;
  }

  dt->min_code_size = (uint8_t)min_code_size;
  dt->max_code_size = (uint8_t)max_code_size;

  if (sym_freq_all_ones) {
    if (min_code_size == max_code_size) {
      for (uint32_t k = 0; k < num_syms; k++)
        dt->sorted_symbol_order[k] = (uint16_t)k;
    } else {
      uint32_t count_max = hist->num_codes[max_code_size];
      uint32_t offset_max = sorted_positions[max_code_size];
      for (uint32_t k = 0; k < count_max; k++)
        dt->sorted_symbol_order[offset_max + k] = (uint16_t)k;

      uint32_t count_min = hist->num_codes[min_code_size];
      uint32_t offset_min = sorted_positions[min_code_size];
      for (uint32_t k = 0; k < count_min; k++)
        dt->sorted_symbol_order[offset_min + k] = (uint16_t)(count_max + k);
    }
  } else {
    for (uint32_t i = 0; i < num_syms; i++) {
      uint32_t c = codesizes[i];
      if (!c)
        continue; // skip unused
      uint32_t sorted_pos = sorted_positions[c]++;
      dt->sorted_symbol_order[sorted_pos] = (uint16_t)i;
    }
  }

  if (table_bits <= dt->min_code_size)
    table_bits = 0;
  dt->table_bits = table_bits;

  if (table_bits) {
    uint32_t table_size = 1U << table_bits;
    if (table_size > dt->cur_lookup_size) {
      dt->cur_lookup_size = table_size;
      dt->lookup = (uint32_t *)lzhamd_memctx_alloc(
          mem, table_size * sizeof(uint32_t), 4);
      if (!dt->lookup)
        return 0;
    }
    memset(dt->lookup, 0xFF, table_size * sizeof(uint32_t));

    for (uint32_t codesize = 1; codesize <= table_bits; codesize++) {
      if (!hist->num_codes[codesize])
        continue;

      uint32_t fillsize = table_bits - codesize;
      uint32_t fillnum = 1U << fillsize;

      uint32_t min_code = min_codes[codesize - 1];
      // get_unshifted_max_code equivalent
      uint32_t max_code_shifted = dt->max_codes[codesize - 1];
      uint32_t max_code = (max_code_shifted >> (16 - codesize)) - 1;

      uint32_t val_ptr = dt->val_ptrs[codesize - 1];

      for (uint32_t code = min_code; code <= max_code; code++) {
        uint32_t sym_index = dt->sorted_symbol_order[val_ptr + code - min_code];

        for (uint32_t j = 0; j < fillnum; j++) {
          uint32_t t = j + (code << fillsize);
          // C++ stores: sym_index | (codesize << 16)
          dt->lookup[t] = sym_index | (codesize << 16U);
        }
      }
    }
  }

  for (uint32_t i = 0; i < LZHAMD_HUFF_MAX_CODE_SIZE; i++)
    dt->val_ptrs[i] -= min_codes[i];

  dt->table_max_code = 0;
  dt->decode_start_code_size = dt->min_code_size;

  if (table_bits) {
    uint32_t i;
    for (i = table_bits; i >= 1; i--) {
      if (hist->num_codes[i]) {
        dt->table_max_code = dt->max_codes[i - 1];
        break;
      }
    }
    if (i >= 1) {
      dt->decode_start_code_size = table_bits + 1;
      for (i = table_bits + 1; i <= max_code_size; i++) {
        if (hist->num_codes[i]) {
          dt->decode_start_code_size = i;
          break;
        }
      }
    }
  }

  dt->table_shift = 32 - dt->table_bits;
  return 1;
}
