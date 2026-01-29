// Complete rewrite of Huffman generation from C++ lzham_huffman_codes.cpp
// Lines 31-283

#include "lzhamd_huffman.h"
#include <string.h>

// Line 31-41: sym_freq structure (C++ has m_freq, m_left, m_right)
typedef struct {
  uint32_t m_freq;
  uint16_t m_left;
  uint16_t m_right;
} sym_freq_t;

// Lines 43-159: radix_sort_syms (exact C++ translation)
static sym_freq_t *radix_sort_syms(uint32_t num_syms, sym_freq_t *syms0,
                                   sym_freq_t *syms1) {
  const uint32_t cMaxPasses = 2;
  uint32_t hist[256 * cMaxPasses];

  // Line 48: memset hist
  memset(hist, 0, sizeof(hist[0]) * 256 * cMaxPasses);

  // Lines 50-73: Build histograms
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

  // Line 78: determine number of passes
  const uint32_t total_passes = (hist[256] == num_syms) ? 1 : cMaxPasses;

  // Lines 80-147: Radix sort passes
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

// Lines 179-224: calculate_minimum_redundancy (exact C++ copy)
static void calculate_minimum_redundancy(int A[], int n) {
  int root, leaf, next, avbl, used, dpth;

  if (n == 0)
    return;
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

// Lines 161-171: huffman_work_tables structure
typedef struct {
  // cMaxInternalNodes = cHuffmanMaxSupportedSyms = 1024
  sym_freq_t syms0[1024 + 1 + 1024];
  sym_freq_t syms1[1024 + 1 + 1024];
} huffman_work_tables_t;

uint32_t lzhamd_huffman_scratch_size(void) {
  return sizeof(huffman_work_tables_t);
}

// Lines 226-283: generate_huffman_codes (exact translation)
int lzhamd_generate_huffman_codes(void *pContext, uint32_t num_syms,
                                  const uint16_t *pFreq, uint8_t *pCodesizes,
                                  uint32_t *max_code_size,
                                  uint32_t *total_freq_ret,
                                  lzhamd_code_size_hist_t *code_size_hist) {
  // Line 228-229
  if ((!num_syms) || (num_syms > 1024)) // cHuffmanMaxSupportedSyms
    return 0;

  // Line 231
  huffman_work_tables_t *state = (huffman_work_tables_t *)pContext;

  // Lines 233-234
  uint32_t max_freq = 0;
  uint32_t total_freq = 0;

  // Lines 236-254: Build used symbols list
  uint32_t num_used_syms = 0;
  for (uint32_t i = 0; i < num_syms; i++) {
    uint32_t freq = pFreq[i];

    if (!freq)
      pCodesizes[i] = 0;
    else {
      total_freq += freq;
      max_freq = (freq > max_freq) ? freq : max_freq; // math::maximum

      sym_freq_t *sf = &state->syms0[num_used_syms];
      sf->m_left = (uint16_t)i;
      sf->m_right = 0xFFFF; // cUINT16_MAX
      sf->m_freq = freq;
      num_used_syms++;
    }
  }

  // Line 256
  *total_freq_ret = total_freq;

  // Lines 258-262: Handle single symbol
  if (num_used_syms == 1) {
    pCodesizes[state->syms0[0].m_left] = 1;
    return 1;
  }

  // Line 264: Radix sort
  sym_freq_t *syms = radix_sort_syms(num_used_syms, state->syms0, state->syms1);

  // Lines 266-268: Copy frequencies to int array
  int x[1024]; // cHuffmanMaxSupportedSyms
  for (uint32_t i = 0; i < num_used_syms; i++)
    x[i] = syms[i].m_freq;

  // Line 270
  calculate_minimum_redundancy(x, num_used_syms);

  // Lines 272-279: Build code sizes and histogram
  uint32_t max_len = 0;
  for (uint32_t i = 0; i < num_used_syms; i++) {
    uint32_t len = x[i];
    max_len = (len > max_len) ? len : max_len; // math::maximum

    // Line 277: Limit to cMaxUnlimitedHuffCodeSize (32)
    uint32_t hist_len = (len < 32) ? len : 32;
    code_size_hist->num_codes[hist_len]++;

    pCodesizes[syms[i].m_left] = (uint8_t)len;
  }

  // Line 280
  *max_code_size = max_len;

  // Line 282
  return 1;
}
