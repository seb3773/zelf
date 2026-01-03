// lzhamd_decompress.c - Pure C LZHAM decompressor (skeleton, UNBUFFERED only)
// C99, Linux x86_64. Minimal binary size focus.
// NOTE: This is the streaming API skeleton. The actual compressed block
// decoding logic will be implemented next using the helpers in
// lzhamd_symbol_codec.c and lzhamd_huffman.c.

#include "lzham_decompressor.h"
#include "lzhamd_huffman.h"
#include "lzhamd_internal.h"
#include "lzhamd_models.h"
#include "lzhamd_symbol_codec.h"
// #include <stdio.h>  // DEBUG only - breaks nostdlib stubs
#ifdef LZHAM_DEBUG
#include <stdio.h>
static int fail_line_num = 0;
static uint64_t g_total_huff_decodes = 0;
static uint64_t g_total_model_rebuilds = 0;
#define FAIL_COMP()                                                            \
  do {                                                                         \
    fail_line_num = __LINE__;                                                  \
    goto comp_fail;                                                            \
  } while (0)
#else
#define FAIL_COMP() goto comp_fail
#endif
#include <string.h>

// Minimal internal memcpy replacement (nostdlib-friendly)
static inline void lzhamd_copy_bytes(uint8_t *dst, const uint8_t *src,
                                     size_t n) {
  // Unrolled by 8 for a bit of speed without pulling libc
  while (n >= 8) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    dst[4] = src[4];
    dst[5] = src[5];
    dst[6] = src[6];
    dst[7] = src[7];
    dst += 8;
    src += 8;
    n -= 8;
  }
  while (n) {
    *dst++ = *src++;
    n--;
  }
}

// Stream state (hidden)
struct lzhamd_stream {
  lzhamd_params_t p;
  int started;
  // Bit I/O and arithmetic decoder
  lzhamd_bitreader_t br;
  lzhamd_arithdec_t ad;
  // Workspace-backed memory context (no malloc)
  lzhamd_memctx_t mem;
  // Pre-allocated scratch buffers for build_tables (avoid stack and repeated
  // alloc)
  int *scratch_buf;       // LZHAMD_HUFF_MAX_SYMS * 3 ints
  uint8_t *codesizes_buf; // LZHAMD_HUFF_MAX_SYMS bytes
  // Output tracking (UNBUFFERED only)
  uint8_t *out_base;
  size_t out_capacity;
  size_t out_ofs;
  // Dict and slots
  uint32_t dict_size;
  uint32_t dict_mask;
  uint32_t num_lzx_slots;
  // Models (Huffman)
  int models_inited;
  lzhamd_model_t lit_model;
  lzhamd_model_t delta_lit_model;
  lzhamd_model_t main_model;
  lzhamd_model_t rep_len_model[2];
  lzhamd_model_t large_len_model[2];
  lzhamd_model_t dist_lsb_model;
  // Arithmetic bit models
  lzhamd_adapt_bit_model_t is_match_model[LZHAMD_C_NUM_STATES];
  lzhamd_adapt_bit_model_t is_rep_model[LZHAMD_C_NUM_STATES];
  lzhamd_adapt_bit_model_t is_rep0_model[LZHAMD_C_NUM_STATES];
  lzhamd_adapt_bit_model_t is_rep0_single_byte_model[LZHAMD_C_NUM_STATES];
  lzhamd_adapt_bit_model_t is_rep1_model[LZHAMD_C_NUM_STATES];
  lzhamd_adapt_bit_model_t is_rep2_model[LZHAMD_C_NUM_STATES];
};

static int lzhamd_validate_params(const lzhamd_params_t *p) {
  if (!p)
    return 0;
  if (p->dict_size_log2 < LZHAM_MIN_DICT_SIZE_LOG2 ||
      p->dict_size_log2 > LZHAM_MAX_DICT_SIZE_LOG2_X64)
    return 0;
  if (p->num_seed_bytes)
    return 0; // seeds not supported in minimal build
  if (p->decompress_flags & LZHAMD_FLAG_READ_ZLIB_STREAM)
    return 0; // zlib wrapper not supported in minimal build
  return 1;
}

lzhamd_stream_t *lzhamd_create(const lzhamd_params_t *p) {
  if (!lzhamd_validate_params(p))
    return NULL;
  if (!p->workspace || p->workspace_size < LZHAMD_WORKSPACE_MIN)
    return NULL;
  // Place stream state at start of workspace (aligned to 8)
  uintptr_t base = (uintptr_t)p->workspace;
  base = (base + 7u) & ~((uintptr_t)7u);
  if ((base + sizeof(lzhamd_stream_t)) >
      ((uintptr_t)p->workspace + p->workspace_size))
    return NULL;
  lzhamd_stream_t *s = (lzhamd_stream_t *)base;
  memset(s, 0, sizeof(*s));
  s->p = *p;
  // Initialize memctx to remaining workspace after stream struct
  uintptr_t mem_start = (uintptr_t)s + sizeof(lzhamd_stream_t);
  lzhamd_memctx_init(
      &s->mem, (void *)mem_start,
      (size_t)(((uintptr_t)p->workspace + p->workspace_size) - mem_start));
  // Pre-allocate scratch buffers for build_tables (used repeatedly, never
  // freed)
  s->scratch_buf =
      (int *)lzhamd_memctx_alloc(&s->mem, lzhamd_huffman_scratch_size(), 4);
  s->codesizes_buf =
      (uint8_t *)lzhamd_memctx_alloc(&s->mem, LZHAMD_HUFF_MAX_SYMS, 1);
  if (!s->scratch_buf || !s->codesizes_buf)
    return NULL;
  // Dict setup
  s->dict_size = 1u << s->p.dict_size_log2;
  s->dict_mask = s->dict_size - 1u;
  s->num_lzx_slots = lzhamd_num_lzx_position_slots(s->p.dict_size_log2);
  s->models_inited = 0;
  // Init bit models
  for (int i = 0; i < LZHAMD_C_NUM_STATES; i++) {
    lzhamd_abm_init(&s->is_match_model[i]);
    lzhamd_abm_init(&s->is_rep_model[i]);
    lzhamd_abm_init(&s->is_rep0_model[i]);
    lzhamd_abm_init(&s->is_rep0_single_byte_model[i]);
    lzhamd_abm_init(&s->is_rep1_model[i]);
    lzhamd_abm_init(&s->is_rep2_model[i]);
  }
  return s;
}

lzhamd_stream_t *lzhamd_reset(lzhamd_stream_t *s, const lzhamd_params_t *p) {
  if (!s)
    return NULL;
  if (p) {
    if (!lzhamd_validate_params(p))
      return NULL;
    if (!p->workspace || p->workspace_size < LZHAMD_WORKSPACE_MIN)
      return NULL;
    s->p = *p;
  }
  // Reset stream fields
  lzhamd_params_t saved = s->p;
  memset(s, 0, sizeof(*s));
  s->p = saved;
  // Re-init memctx to remaining workspace after stream struct
  uintptr_t mem_start = (uintptr_t)s + sizeof(lzhamd_stream_t);
  lzhamd_memctx_init(
      &s->mem, (void *)mem_start,
      (size_t)(((uintptr_t)s->p.workspace + s->p.workspace_size) - mem_start));
  s->dict_size = 1u << s->p.dict_size_log2;
  s->dict_mask = s->dict_size - 1u;
  s->num_lzx_slots = lzhamd_num_lzx_position_slots(s->p.dict_size_log2);
  s->models_inited = 0;
  for (int i = 0; i < LZHAMD_C_NUM_STATES; i++) {
    lzhamd_abm_init(&s->is_match_model[i]);
    lzhamd_abm_init(&s->is_rep_model[i]);
    lzhamd_abm_init(&s->is_rep0_model[i]);
    lzhamd_abm_init(&s->is_rep0_single_byte_model[i]);
    lzhamd_abm_init(&s->is_rep1_model[i]);
    lzhamd_abm_init(&s->is_rep2_model[i]);
  }
  return s;
}

static inline void lzhamd_set_out(lzhamd_stream_t *s, uint8_t *out_ptr,
                                  size_t out_cap) {
  if (!s->out_base) {
    s->out_base = out_ptr;
    s->out_capacity = out_cap;
  }
}

lzhamd_status_t lzhamd_decompress(lzhamd_stream_t *s, const uint8_t *in_ptr,
                                  size_t *pIn_size, uint8_t *out_ptr,
                                  size_t *pOut_size, int finish_flag) {
  if (!s || !pIn_size || !pOut_size)
    return LZHAMD_STATUS_INVALID_PARAMETER;
  if (!(s->p.decompress_flags & LZHAMD_FLAG_OUTPUT_UNBUFFERED))
    return LZHAMD_STATUS_INVALID_PARAMETER;
  if (!out_ptr || *pOut_size == 0)
    return LZHAMD_STATUS_INVALID_PARAMETER;

  // UNBUFFERED requirement: caller provides the full destination buffer on the
  // first call and keeps it constant.
  lzhamd_set_out(s, out_ptr, *pOut_size);
  if (out_ptr != s->out_base || *pOut_size != s->out_capacity)
    return LZHAMD_STATUS_INVALID_PARAMETER;

  // Initialize bitreader on first call
  if (!s->started) {
    lzhamd_br_init(&s->br, in_ptr, *pIn_size, finish_flag ? 1 : 0);
    // Arithmetic decoder will be initialized when needed for comp blocks
    s->started = 1;
    // DEBUG: show bitreader state (commented - breaks nostdlib stubs)
    // fprintf(stderr, "[DBG] br init: buf=%p next=%p end=%p diff=%zd bitcnt=%d
    // eof=%d\n",
    //         (void *)s->br.buf, (void *)s->br.next, (void *)s->br.end,
    //         (s->br.end - s->br.next), s->br.bitcnt, s->br.eof);
  } else {
    lzhamd_br_set(&s->br, in_ptr, *pIn_size, in_ptr, finish_flag ? 1 : 0);
  }

  size_t out_start = s->out_ofs;

  for (;;) {
    // Decode block type (2 bits)
    if (s->br.bitcnt < LZHAMD_C_BLOCK_HEADER_BITS &&
        (size_t)(s->br.end - s->br.next) == 0) {
      // Need more input to read block type
      *pOut_size = s->out_ofs - out_start;
      *pIn_size = (size_t)(s->br.next - s->br.buf);
      if (!finish_flag)
        return LZHAMD_STATUS_NEEDS_MORE_INPUT;
#ifdef LZHAM_DEBUG
      fprintf(stderr,
              "[LZHAM_DEBUG] BAD_CODE: bitcnt=%d, end-next=%zd, buf=%p, "
              "next=%p, end=%p, out_ofs=%zu, started=%d\n",
              s->br.bitcnt, (s->br.end - s->br.next), (void *)s->br.buf,
              (void *)s->br.next, (void *)s->br.end, s->out_ofs, s->started);
#endif
      return LZHAMD_STATUS_FAILED_BAD_CODE;
    }

    uint32_t block_type =
        lzhamd_br_get_bits(&s->br, LZHAMD_C_BLOCK_HEADER_BITS);

    if (block_type == LZHAMD_C_SYNC_BLOCK) {
      // Read flush type (2 bits). For minimal impl we ignore rate resets.
      (void)lzhamd_br_get_bits(&s->br, LZHAMD_C_BLOCK_FLUSH_TYPE_BITS);
      lzhamd_br_align_to_byte(&s->br);
      // Validate sync pattern: expect 0x0000 then 0xFFFF
      uint32_t n0 = lzhamd_br_get_bits(&s->br, 16);
      uint32_t n1 = lzhamd_br_get_bits(&s->br, 16);
      if (n0 != 0 || n1 != 0xFFFF) {
        *pOut_size = s->out_ofs - out_start;
        *pIn_size = (size_t)(s->br.next - s->br.buf);
        return LZHAMD_STATUS_FAILED_BAD_SYNC_BLOCK;
      }
      // In unbuffered mode, original impl returns NOT_FINISHED to allow caller
      // to handle output now.
      *pOut_size = s->out_ofs - out_start;
      *pIn_size = (size_t)(s->br.next - s->br.buf);
      return LZHAMD_STATUS_NOT_FINISHED;
    } else if (block_type == LZHAMD_C_RAW_BLOCK) {
      // Read 24-bit len, then 8-bit check = xor of len bytes, then len++
      if (s->br.bitcnt < 24 && (size_t)(s->br.end - s->br.next) == 0) {
        *pOut_size = s->out_ofs - out_start;
        *pIn_size = (size_t)(s->br.next - s->br.buf);
        if (!finish_flag)
          return LZHAMD_STATUS_NEEDS_MORE_INPUT;
        return LZHAMD_STATUS_FAILED_EXPECTED_MORE_RAW_BYTES;
      }
      uint32_t len = lzhamd_br_get_bits(&s->br, 24);
      uint32_t chk = lzhamd_br_get_bits(&s->br, 8);
      uint32_t b0 = len & 0xFFu;
      uint32_t b1 = (len >> 8) & 0xFFu;
      uint32_t b2 = (len >> 16) & 0xFFu;
      if (chk != ((b0 ^ b1) ^ b2)) {
        *pOut_size = s->out_ofs - out_start;
        *pIn_size = (size_t)(s->br.next - s->br.buf);
        return LZHAMD_STATUS_FAILED_BAD_RAW_BLOCK;
      }
      len += 1u;

      // Align to next byte boundary and flush any full bytes from bit buffer to
      // output
      lzhamd_br_align_to_byte(&s->br);
      for (;;) {
        int vb = lzhamd_br_remove_byte(&s->br);
        if (vb < 0)
          break;
        if (s->out_ofs >= s->out_capacity) {
          *pOut_size = s->out_ofs - out_start;
          *pIn_size = (size_t)(s->br.next - s->br.buf);
          return LZHAMD_STATUS_FAILED_DEST_BUF_TOO_SMALL;
        }
        s->out_base[s->out_ofs++] = (uint8_t)vb;
        if (!--len)
          break;
      }

      // Bulk copy remaining raw bytes from input buffer
      while (len) {
        size_t in_rem = (size_t)(s->br.end - s->br.next);
        if (!in_rem) {
          *pOut_size = s->out_ofs - out_start;
          *pIn_size = (size_t)(s->br.next - s->br.buf);
          if (!finish_flag)
            return LZHAMD_STATUS_NEEDS_MORE_INPUT;
          return LZHAMD_STATUS_FAILED_EXPECTED_MORE_RAW_BYTES;
        }
        size_t out_rem = s->out_capacity - s->out_ofs;
        size_t to_copy = (len < in_rem) ? (size_t)len : in_rem;
        if (to_copy > out_rem) {
          *pOut_size = s->out_ofs - out_start;
          *pIn_size = (size_t)(s->br.next - s->br.buf);
          return LZHAMD_STATUS_FAILED_DEST_BUF_TOO_SMALL;
        }
        lzhamd_copy_bytes(s->out_base + s->out_ofs, s->br.next, to_copy);
        s->out_ofs += to_copy;
        s->br.next += to_copy;
        len -= (uint32_t)to_copy;
      }
      // Continue with next block
      continue;
    } else if (block_type == LZHAMD_C_COMP_BLOCK) {
      // Initialize arithmetic decoder and models on first COMP block
      if (!s->models_inited) {
        // Initialize models with sizes
        uint32_t main_syms =
            LZHAMD_C_LZX_NUM_SPECIAL_LENGTHS +
            ((s->num_lzx_slots - LZHAMD_C_LZX_LOWEST_USABLE_MATCH_SLOT) * 8u);
        if (!lzhamd_model_init(&s->lit_model, 256, 64, 64, &s->mem))
          FAIL_COMP();
        if (!lzhamd_model_init(&s->delta_lit_model, 256, 64, 64, &s->mem))
          FAIL_COMP();
        if (!lzhamd_model_init(&s->main_model, main_syms, 64, 64, &s->mem))
          FAIL_COMP();
        if (!lzhamd_model_init(
                &s->rep_len_model[0],
                (1 + (LZHAMD_C_MAX_MATCH_LEN - LZHAMD_C_MIN_MATCH_LEN + 1)), 64,
                64, &s->mem))
          FAIL_COMP();
        if (!lzhamd_model_init(
                &s->rep_len_model[1],
                (1 + (LZHAMD_C_MAX_MATCH_LEN - LZHAMD_C_MIN_MATCH_LEN + 1)), 64,
                64, &s->mem))
          FAIL_COMP();
        if (!lzhamd_model_init(&s->large_len_model[0],
                               (1 + LZHAMD_C_LZX_NUM_SECONDARY_LENGTHS), 64, 64,
                               &s->mem))
          FAIL_COMP();
        if (!lzhamd_model_init(&s->large_len_model[1],
                               (1 + LZHAMD_C_LZX_NUM_SECONDARY_LENGTHS), 64, 64,
                               &s->mem))
          FAIL_COMP();
        if (!lzhamd_model_init(&s->dist_lsb_model, 16, 64, 64, &s->mem))
          FAIL_COMP();
        s->models_inited = 1;
      }

      // Start arithmetic decoder (needs 4 bytes), then init block state
      lzhamd_ad_init(&s->ad, &s->br);
      uint32_t match_hist0 = 1, match_hist1 = 1, match_hist2 = 1,
               match_hist3 = 1;
      uint32_t cur_state = 0;
      // Then read block flush type (2 bits)
      uint32_t block_flush_type =
          lzhamd_br_get_bits(&s->br, LZHAMD_C_BLOCK_FLUSH_TYPE_BITS);
      if (block_flush_type == 1) {
        // Reset update rates
        lzhamd_model_reset_update_rate(&s->lit_model);
        lzhamd_model_reset_update_rate(&s->delta_lit_model);
        lzhamd_model_reset_update_rate(&s->main_model);
        lzhamd_model_reset_update_rate(&s->rep_len_model[0]);
        lzhamd_model_reset_update_rate(&s->rep_len_model[1]);
        lzhamd_model_reset_update_rate(&s->large_len_model[0]);
        lzhamd_model_reset_update_rate(&s->large_len_model[1]);
        lzhamd_model_reset_update_rate(&s->dist_lsb_model);
      } else if (block_flush_type == 2) {
        // Full reset of tables
        lzhamd_model_reset_all(&s->lit_model);
        lzhamd_model_reset_all(&s->delta_lit_model);
        lzhamd_model_reset_all(&s->main_model);
        lzhamd_model_reset_all(&s->rep_len_model[0]);
        lzhamd_model_reset_all(&s->rep_len_model[1]);
        lzhamd_model_reset_all(&s->large_len_model[0]);
        lzhamd_model_reset_all(&s->large_len_model[1]);
        lzhamd_model_reset_all(&s->dist_lsb_model);
      }

      static const uint8_t s_literal_next_state[24] = {
          0, 0, 0, 0, 1, 2, 3, 4,  5,  6,  4,  5,
          7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10};
      static const uint32_t s_huge_match_base_len[4] = {
          LZHAMD_C_MAX_MATCH_LEN + 1u, LZHAMD_C_MAX_MATCH_LEN + 1u + 256u,
          LZHAMD_C_MAX_MATCH_LEN + 1u + 256u + 1024u,
          LZHAMD_C_MAX_MATCH_LEN + 1u + 256u + 1024u + 4096u};
      static const uint8_t s_huge_match_code_len[4] = {8, 10, 12, 16};

      for (;;) {
        // is_match
        uint32_t is_match_bit =
            lzhamd_ad_get_bit(&s->ad, &s->is_match_model[cur_state]);
        if (!is_match_bit) {
          // Literal
          if (s->out_ofs >= s->out_capacity)
            goto dest_small;
          uint32_t r;
          if (cur_state < LZHAMD_C_NUM_LIT_STATES) {
            r = lzhamd_model_decode(&s->lit_model, &s->br);
          } else {
            // Delta-literal
            uint32_t rep_lit0_ofs = (s->out_ofs - match_hist0);
            uint32_t rep_lit0 = s->out_base[rep_lit0_ofs];
            uint32_t v = lzhamd_model_decode(&s->delta_lit_model, &s->br);
            r = v ^ rep_lit0;
          }
          if (r > 255u)
            FAIL_COMP();
          s->out_base[s->out_ofs++] = (uint8_t)r;
          cur_state = s_literal_next_state[cur_state];
          continue;
        }

        // Match path
        uint32_t match_len = 1;
        uint32_t is_rep =
            lzhamd_ad_get_bit(&s->ad, &s->is_rep_model[cur_state]);
        if (is_rep) {
          uint32_t is_rep0 =
              lzhamd_ad_get_bit(&s->ad, &s->is_rep0_model[cur_state]);
          if (is_rep0) {
            uint32_t is_rep0_len1 = lzhamd_ad_get_bit(
                &s->ad, &s->is_rep0_single_byte_model[cur_state]);
            if (is_rep0_len1) {
              // length 1 match
              match_len = 1;
              cur_state = (cur_state < LZHAMD_C_NUM_LIT_STATES) ? 9 : 11;
            } else {
              uint32_t idx = (cur_state >= LZHAMD_C_NUM_LIT_STATES) ? 1u : 0u;
              uint32_t e = lzhamd_model_decode(&s->rep_len_model[idx], &s->br);
              if (e == 0xFFFFFFFFu)
                FAIL_COMP();
              match_len = e + LZHAMD_C_MIN_MATCH_LEN;
              if (match_len == (LZHAMD_C_MAX_MATCH_LEN + 1u)) {
                // huge match
                uint32_t bits = 0;
                for (; bits < 3; bits++) {
                  if (!lzhamd_br_get_bits(&s->br, 1))
                    break;
                }
                uint32_t k =
                    lzhamd_br_get_bits(&s->br, s_huge_match_code_len[bits]);
                match_len = s_huge_match_base_len[bits] + k;
              }
              cur_state = (cur_state < LZHAMD_C_NUM_LIT_STATES) ? 8 : 11;
            }
          } else {
            uint32_t idx = (cur_state >= LZHAMD_C_NUM_LIT_STATES) ? 1u : 0u;
            uint32_t e = lzhamd_model_decode(&s->rep_len_model[idx], &s->br);
            if (e == 0xFFFFFFFFu)
              FAIL_COMP();
            match_len = e + LZHAMD_C_MIN_MATCH_LEN;
            if (match_len == (LZHAMD_C_MAX_MATCH_LEN + 1u)) {
              uint32_t bits = 0;
              for (; bits < 3; bits++) {
                if (!lzhamd_br_get_bits(&s->br, 1))
                  break;
              }
              uint32_t k =
                  lzhamd_br_get_bits(&s->br, s_huge_match_code_len[bits]);
              match_len = s_huge_match_base_len[bits] + k;
            }
            uint32_t is_rep1 =
                lzhamd_ad_get_bit(&s->ad, &s->is_rep1_model[cur_state]);
            if (is_rep1) {
              uint32_t t = match_hist1;
              match_hist1 = match_hist0;
              match_hist0 = t;
            } else {
              uint32_t is_rep2 =
                  lzhamd_ad_get_bit(&s->ad, &s->is_rep2_model[cur_state]);
              if (is_rep2) {
                uint32_t t = match_hist2;
                match_hist2 = match_hist1;
                match_hist1 = match_hist0;
                match_hist0 = t;
              } else {
                uint32_t t = match_hist3;
                match_hist3 = match_hist2;
                match_hist2 = match_hist1;
                match_hist1 = match_hist0;
                match_hist0 = t;
              }
            }
            cur_state = (cur_state < LZHAMD_C_NUM_LIT_STATES) ? 8 : 11;
          }
        } else {
          // Full match
          uint32_t sym = lzhamd_model_decode(&s->main_model, &s->br);
          if (sym == 0xFFFFFFFFu)
            FAIL_COMP();
          if (sym < LZHAMD_C_LZX_NUM_SPECIAL_LENGTHS) {
            // Special codes
            if (sym == LZHAMD_C_LZX_SPECIAL_END_OF_BLOCK_CODE)
              break;
            // Partial state reset
            match_hist0 = match_hist1 = match_hist2 = match_hist3 = 1;
            cur_state = 0;
            continue;
          }
          sym -= LZHAMD_C_LZX_NUM_SPECIAL_LENGTHS;
          match_len = (sym & 7u) + 2u;
          uint32_t match_slot =
              (sym >> 3) + LZHAMD_C_LZX_LOWEST_USABLE_MATCH_SLOT;
#ifdef LZHAM_DEBUG
          if (match_slot >= 40) {
            fprintf(stderr,
                    "[LZHAM_DEBUG] High slot: raw_sym=%u, sym_adj=%u, slot=%u, "
                    "out_ofs=%zu\n",
                    sym + LZHAMD_C_LZX_NUM_SPECIAL_LENGTHS, sym, match_slot,
                    s->out_ofs);
            fprintf(stderr,
                    "[LZHAM_DEBUG] main_model state: num_syms=%u, "
                    "num_lzx_slots=%u, total_count=%u\n",
                    s->main_model.num_syms, s->num_lzx_slots,
                    s->main_model.total_count);
            fprintf(stderr,
                    "[LZHAM_DEBUG] update_cycle=%u, symbols_until_update=%u\n",
                    s->main_model.update_cycle,
                    s->main_model.symbols_until_update);
          }
#endif
          if (match_len == 9u) {
            uint32_t idx = (cur_state >= LZHAMD_C_NUM_LIT_STATES) ? 1u : 0u;
            uint32_t e = lzhamd_model_decode(&s->large_len_model[idx], &s->br);
            if (e == 0xFFFFFFFFu)
              FAIL_COMP();
            match_len += e;
            if (match_len == (LZHAMD_C_MAX_MATCH_LEN + 1u)) {
              uint32_t bits = 0;
              for (; bits < 3; bits++) {
                if (!lzhamd_br_get_bits(&s->br, 1))
                  break;
              }
              uint32_t k =
                  lzhamd_br_get_bits(&s->br, s_huge_match_code_len[bits]);
              match_len = s_huge_match_base_len[bits] + k;
            }
          }
          uint32_t num_extra = g_lzhamd_lzx_position_extra_bits[match_slot];
          uint32_t extra_bits = 0;
          if (num_extra < 3u) {
            extra_bits =
                (num_extra ? lzhamd_br_get_bits(&s->br, num_extra) : 0u);
          } else {
            if (num_extra > 4u) {
              extra_bits = lzhamd_br_get_bits(&s->br, num_extra - 4u) << 4;
            }
            uint32_t j = lzhamd_model_decode(&s->dist_lsb_model, &s->br);
            if (j == 0xFFFFFFFFu)
              FAIL_COMP();
            extra_bits += j;
          }
          match_hist3 = match_hist2;
          match_hist2 = match_hist1;
          match_hist1 = match_hist0;
#ifdef LZHAM_DEBUG
          if (g_lzhamd_lzx_position_base[match_slot] + extra_bits > 1000000) {
            fprintf(stderr,
                    "[LZHAM_DEBUG] Large dist: slot=%u, num_extra=%u, "
                    "extra=%u, base=%u, final=%u\n",
                    match_slot, num_extra, extra_bits,
                    g_lzhamd_lzx_position_base[match_slot],
                    g_lzhamd_lzx_position_base[match_slot] + extra_bits);
          }
#endif
          match_hist0 = g_lzhamd_lzx_position_base[match_slot] + extra_bits;
          cur_state = (cur_state < LZHAMD_C_NUM_LIT_STATES)
                          ? LZHAMD_C_NUM_LIT_STATES
                          : (LZHAMD_C_NUM_LIT_STATES + 3);
        }

        // Perform match copy
        if ((match_hist0 > s->out_ofs) ||
            (s->out_ofs + match_len > s->out_capacity)) {
#ifdef LZHAM_DEBUG
          fprintf(stderr,
                  "[LZHAM_DEBUG] match bounds fail: hist0=%u, match_len=%u, "
                  "out_ofs=%zu, cap=%zu\n",
                  match_hist0, match_len, s->out_ofs, s->out_capacity);
#endif
          FAIL_COMP();
        }
        uint32_t src_ofs = (s->out_ofs - match_hist0);
        uint8_t *pCopy_src = s->out_base + src_ofs;
        uint8_t *pCopy_dst = s->out_base + s->out_ofs;
        if (match_hist0 == 1u) {
          uint8_t c = *pCopy_src;
          if (match_len < 8u) {
            for (uint32_t i = 0; i < match_len; i++)
              *pCopy_dst++ = c;
          } else {
            memset(pCopy_dst, c, match_len);
          }
        } else {
          if ((match_len < 8u) || ((int32_t)match_len > (int32_t)match_hist0)) {
            for (uint32_t i = 0; i < match_len; i++)
              *pCopy_dst++ = *pCopy_src++;
          } else {
            lzhamd_copy_bytes(pCopy_dst, pCopy_src, match_len);
          }
        }
        s->out_ofs += match_len;
      }

      // Align to next byte boundary at end of COMP block
      lzhamd_br_align_to_byte(&s->br);
      continue;

    comp_fail:
      *pOut_size = s->out_ofs - out_start;
      *pIn_size = (size_t)(s->br.next - s->br.buf);
#ifdef LZHAM_DEBUG
      fprintf(
          stderr,
          "[LZHAM_DEBUG] comp_fail at line %d: out_ofs=%zu, in_consumed=%zu\n",
          fail_line_num, s->out_ofs, *pIn_size);
#endif
      return LZHAMD_STATUS_FAILED_BAD_CODE;
    dest_small:
      *pOut_size = s->out_ofs - out_start;
      *pIn_size = (size_t)(s->br.next - s->br.buf);
      return LZHAMD_STATUS_FAILED_DEST_BUF_TOO_SMALL;
    } else if (block_type == LZHAMD_C_EOF_BLOCK) {
      // Align to byte and skip 4-byte checksum field (Adler32 not verified for
      // minimal stub)
      lzhamd_br_align_to_byte(&s->br);
      size_t need = 4;
      size_t have = (size_t)(s->br.end - s->br.next);
      if (have < need) {
        *pOut_size = s->out_ofs - out_start;
        *pIn_size = (size_t)(s->br.next - s->br.buf);
        return LZHAMD_STATUS_NEEDS_MORE_INPUT;
      }
      // Consume checksum bytes without verification
      (void)lzhamd_br_get_bits(&s->br, 16);
      (void)lzhamd_br_get_bits(&s->br, 16);
      // Success: all data decoded
      *pOut_size = s->out_ofs - out_start;
      *pIn_size = (size_t)(s->br.next - s->br.buf);
      return LZHAMD_STATUS_SUCCESS;
    } else {
      *pOut_size = s->out_ofs - out_start;
      *pIn_size = (size_t)(s->br.next - s->br.buf);
#ifdef LZHAM_DEBUG
      fprintf(stderr, "[LZHAM_DEBUG] unknown block_type=%u, out_ofs=%zu\n",
              block_type, s->out_ofs);
#endif
      return LZHAMD_STATUS_FAILED_BAD_CODE;
    }
  }
}

void lzhamd_destroy(lzhamd_stream_t *s) {
  // Workspace-backed: nothing to free.
}

lzhamd_status_t lzhamd_decompress_memory(const lzhamd_params_t *p, uint8_t *dst,
                                         size_t *pDst_len, const uint8_t *src,
                                         size_t src_len) {
  if (!p || !dst || !pDst_len || !src)
    return LZHAMD_STATUS_INVALID_PARAMETER;
  lzhamd_stream_t *s = lzhamd_create(p);
  if (!s)
    return LZHAMD_STATUS_FAILED_INITIALIZING;
  size_t in_size = src_len;
  size_t out_cap = *pDst_len;
  size_t out_done = 0;
  lzhamd_status_t st =
      lzhamd_decompress(s, src, &in_size, dst, &out_cap, /*finish*/ 1);
  if (st == LZHAMD_STATUS_SUCCESS ||
      st == LZHAMD_STATUS_FAILED_DEST_BUF_TOO_SMALL) {
    out_done = out_cap;
  }
  *pDst_len = out_done;
  lzhamd_destroy(s);
  return st;
}
