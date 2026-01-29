#include "csc_dec.h"
#include "csc_internal_dec.h"
#include <string.h>

// Struct definition must be before usage in GetWorkspaceSize
typedef struct CSCDecState_s {
  const uint8_t *src_cur;
  const uint8_t *src_end;

  // Pointers into workspace
  uint32_t *p_lit;
  uint32_t *p_delta;
  uint8_t *wnd;
  uint8_t *rc_buf;
  uint8_t *bc_buf;
  uint8_t *swap_buf;

  // Vars
  uint32_t dict_size;
  uint32_t csc_blocksize;
  uint32_t raw_blocksize;
  uint32_t wnd_size;

  uint32_t wnd_curpos;
  uint32_t rep_dist[4];

  // Range Coder
  uint32_t rc_range;
  uint32_t rc_code;
  uint32_t rc_size;    // index
  uint32_t rc_bufsize; // limit / avail
  uint64_t rc_low;

  // Bit Coder
  uint32_t bc_curbits;
  uint32_t bc_curval;
  uint32_t bc_size;    // index
  uint32_t bc_bufsize; // limit / avail

  // Model Probabilities
  uint32_t p_state[64 * 3];
  uint32_t p_repdist[64 * 4];
  uint32_t p_dist[8 + 16 * 2 + 32 * 4];
  uint32_t p_matchdist_extra[29 * 16];
  uint32_t p_matchlen_slot[2];
  uint32_t p_matchlen_extra1[8];
  uint32_t p_matchlen_extra2[8];
  uint32_t p_matchlen_extra3[128];
  uint32_t p_rle_len[16];

  uint32_t p_longlen;
  uint32_t p_rle_flag;

  uint32_t state;
  uint32_t ctx;

} CSCDecState;

// Properties struct internal
typedef struct {
  uint32_t dict_size;
  uint32_t csc_blocksize;
  uint32_t raw_blocksize;
} CSCDecProps;

#define CSC_IOBUF_MULT 32u

static void Dec_ReadProperties(CSCDecProps *props, const uint8_t *s) {
  props->dict_size = ((uint32_t)s[0] << 24) + ((uint32_t)s[1] << 16) +
                     ((uint32_t)s[2] << 8) + s[3];
  props->csc_blocksize = ((uint32_t)s[4] << 16) + ((uint32_t)s[5] << 8) + s[6];
  props->raw_blocksize = ((uint32_t)s[7] << 16) + ((uint32_t)s[8] << 8) + s[9];
}

intptr_t CSC_Dec_GetWorkspaceSize(const void *src, size_t src_len) {
  if (!src || src_len < CSC_PROP_SIZE)
    return -1;
  CSCDecProps props;
  Dec_ReadProperties(&props, (const uint8_t *)src);

  size_t size = 0;

  // Main struct
  size += sizeof(CSCDecState);

  // Align
  size = (size + 15) & ~15;

  // p_lit
  size += 256 * 256 * sizeof(uint32_t);

  // p_delta
  size += 256 * 256 * sizeof(uint32_t);

  // wnd
  size += props.dict_size + 8;

  // rc_buf (Double buffer size to prevent overflow during interleaving)
  size += (size_t)props.csc_blocksize * (size_t)CSC_IOBUF_MULT;

  // bc_buf (Double buffer size to prevent overflow during interleaving)
  size += (size_t)props.csc_blocksize * (size_t)CSC_IOBUF_MULT;

  // swap_buf
  size += props.raw_blocksize;

  return (intptr_t)size;
}

// =============================================================
// Decompressor Implementation
// =============================================================

static const uint32_t dist_table_[33] = {
    0,        1,         2,         3,         5,          9,        17,
    33,       65,        129,       257,       513,        1025,     2049,
    4097,     8193,      16385,     32769,     65537,      131073,   262145,
    524289,   1048577,   2097153,   4194305,   8388609,    16777217, 33554433,
    67108865, 134217729, 268435457, 536870913, 1073741825,
};

static const uint32_t rev16_table_[16] = {
    0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15,
};

static int ReadByte(CSCDecState *s, uint8_t *b) {
  if (s->src_cur >= s->src_end)
    return -1;
  *b = *s->src_cur++;
  return 0;
}

static int ReadBytes(CSCDecState *s, uint8_t *b, uint32_t len) {
  if (s->src_cur + len > s->src_end)
    return -1;
  memcpy(b, s->src_cur, len);
  s->src_cur += len;
  return 0;
}

static int FetchBlock(CSCDecState *s, int target_rc) {
  uint8_t *my_buf = target_rc ? s->rc_buf : s->bc_buf;
  uint32_t *my_limit = target_rc ? &s->rc_bufsize : &s->bc_bufsize;

  const uint32_t iobuf_cap = s->csc_blocksize * (uint32_t)CSC_IOBUF_MULT;

  (void)my_buf;
  (void)my_limit;

  while (1) {
    uint8_t fb;
    if (ReadByte(s, &fb) < 0)
      return -1;

    uint32_t bsize;
    int is_full = (fb >> 6) & 1;
    int type = (fb >> 7) & 1; // 1=RC, 0=BC

    if (is_full) {
      bsize = s->csc_blocksize;
    } else {
      uint8_t sb[3];
      if (ReadBytes(s, sb, 3) < 0)
        return -1;
      bsize = ((uint32_t)sb[0] << 16) | ((uint32_t)sb[1] << 8) | sb[2];
    }

    if (__builtin_expect(bsize == 0, 0))
      return -1;
    if (__builtin_expect(bsize > s->csc_blocksize, 0))
      return -1;

    uint8_t *dest_buf = type ? s->rc_buf : s->bc_buf;
    uint32_t *dest_filled = type ? &s->rc_bufsize : &s->bc_bufsize;
    uint32_t *dest_idx = type ? &s->rc_size : &s->bc_size;

    if (type == target_rc) {
      if (*dest_idx < *dest_filled) {
        // Warning: implicit discard
      }
      *dest_idx = 0;
      *dest_filled = 0; // Reset

      if (ReadBytes(s, dest_buf, bsize) < 0)
        return -1;
      *dest_filled = bsize;
      return 0;
    } else {
      if (*dest_idx >= *dest_filled) {
        *dest_idx = 0;
        *dest_filled = 0;
      }

      if (*dest_idx > 0 && *dest_idx < *dest_filled) {
        uint32_t remain = *dest_filled - *dest_idx;
        memmove(dest_buf, dest_buf + *dest_idx, remain);
        *dest_idx = 0;
        *dest_filled = remain;
      }

      uint32_t offset = *dest_filled;
      if (offset + bsize > iobuf_cap)
        return -1;

      if (ReadBytes(s, dest_buf + offset, bsize) < 0)
        return -1;
      *dest_filled += bsize;
    }
  }
}

static uint32_t coder_decode_direct(CSCDecState *s, uint32_t len) {
  if (s->bc_size >= s->bc_bufsize) {
    if (FetchBlock(s, 0) < 0)
      return 0;
  }

  uint32_t result;
  while (s->bc_curbits < len) {
    if (s->bc_size >= s->bc_bufsize) {
      if (FetchBlock(s, 0) < 0)
        return 0;
    }
    s->bc_curval = (s->bc_curval << 8) | s->bc_buf[s->bc_size++];
    s->bc_curbits += 8;
  }
  result = (s->bc_curval >> (s->bc_curbits - len)) & ((1 << len) - 1);
  s->bc_curbits -= len;
  return result;
}

static int DecodeBit_Func(CSCDecState *s, uint32_t *v_ptr, uint32_t *p_ptr) {
  if (s->rc_range < (1 << 24)) {
    s->rc_range <<= 8;
    if (s->rc_size >= s->rc_bufsize) {
      if (FetchBlock(s, 1) < 0)
        return -1;
    }
    s->rc_code = (s->rc_code << 8) + s->rc_buf[s->rc_size++];
  }
  uint32_t p = *p_ptr;
  uint32_t bound = (s->rc_range >> 12) * p;
  uint32_t v = *v_ptr;
  if (s->rc_code < bound) {
    s->rc_range = bound;
    p += (0xFFF - p) >> 5;
    v = v + v + 1;
  } else {
    s->rc_range -= bound;
    s->rc_code -= bound;
    p -= p >> 5;
    v = v + v;
  }
  *p_ptr = p;
  *v_ptr = v;
  return 0;
}

#define DecodeBit(s, v, p)                                                     \
  do {                                                                         \
    if (DecodeBit_Func(s, &v, &p) < 0)                                         \
      return DECODE_ERROR;                                                     \
  } while (0)

#define DecodeDirect(s, v, l)                                                  \
  do {                                                                         \
    if ((l) <= 16)                                                             \
      v = coder_decode_direct(s, l);                                           \
    else {                                                                     \
      v = (coder_decode_direct(s, (l) - 16) << 16);                            \
      v = v | coder_decode_direct(s, 16);                                      \
    }                                                                          \
  } while (0)

static uint32_t decode_int(CSCDecState *s) {
  uint32_t slot = 0, num = 0;
  DecodeDirect(s, slot, 5);
  DecodeDirect(s, num, slot == 0 ? 1 : slot);
  if (slot)
    num += (1 << slot);
  return num;
}

static int decode_bad(CSCDecState *s, uint8_t *dst, uint32_t *size,
                      uint32_t max_bsize) {
  *size = decode_int(s);
  if (*size > max_bsize) {
    return -1;
  }

  for (uint32_t i = 0; i < *size; i++) {
    dst[i] = coder_decode_direct(s, 8);
  }
  return 0;
}

static uint32_t decode_matchlen_1(CSCDecState *s) {
  uint32_t v = 0, lenbase;
  uint32_t *p;
  DecodeBit(s, v, s->p_matchlen_slot[0]);
  if (v == 0) {
    p = s->p_matchlen_extra1;
    lenbase = 0;
  } else {
    v = 0;
    DecodeBit(s, v, s->p_matchlen_slot[1]);
    if (v == 0) {
      p = s->p_matchlen_extra2;
      lenbase = 8;
    } else {
      p = s->p_matchlen_extra3;
      lenbase = 16;
    }
  }

  uint32_t i = 1;
  if (lenbase == 16) { // decode 7 bits
    do {
      DecodeBit(s, i, p[i]);
    } while (i < 0x80);
    return lenbase + (i & 0x7F);
  } else { // decode 3 bits
    do {
      DecodeBit(s, i, p[i]);
    } while (i < 0x08);
    return lenbase + (i & 0x07);
  }
}

static uint32_t decode_matchlen_2(CSCDecState *s) {
  uint32_t len = decode_matchlen_1(s);
  if (len == 143) {
    uint32_t v = 0;
    for (;; v = 0, len += 143) {
      DecodeBit(s, v, s->p_longlen);
      if (v)
        break;
    }
    return len + decode_matchlen_1(s);
  } else
    return len;
}

static int decode_rle(CSCDecState *s, uint8_t *dst, uint32_t *size,
                      uint32_t max_bsize) {
  uint32_t c, flag, len, i;
  uint32_t sCtx = 0;
  uint32_t *p = NULL;

  *size = decode_int(s);
  if (*size > max_bsize) {
    return -1;
  }

  for (i = 0; i < *size;) {
    flag = 0;
    DecodeBit(s, flag, s->p_rle_flag);
    c = 1;
    if (flag == 0) {
      p = &s->p_delta[sCtx * 256];
      do {
        DecodeBit(s, c, p[c]);
      } while (c < 0x100);
      dst[i] = c & 0xFF;
      sCtx = dst[i];
      i++;
    } else {
      len = decode_matchlen_2(s) + 11;
      if (i == 0) {
        return -1;
      }
      while (len-- > 0 && i < *size) {
        dst[i] = dst[i - 1];
        i++;
      }
      sCtx = dst[i - 1];
    }
  }
  return 0;
}

static uint32_t decode_literal(CSCDecState *s) {
  uint32_t i = 1, *p;
  p = &s->p_lit[s->ctx * 256];
  do {
    DecodeBit(s, i, p[i]);
  } while (i < 0x100);

  s->ctx = i & 0xFF;
  s->state = (s->state * 4 + 0) & 0x3F;
  return s->ctx;
}

static int decode_literals(CSCDecState *s, uint8_t *dst, uint32_t *size,
                           uint32_t max_bsize) {
  *size = decode_int(s);
  if (*size > max_bsize) {
    return -1;
  }

  for (uint32_t i = 0; i < *size; i++) {
    uint32_t c = 1, *p;
    p = &s->p_lit[s->ctx * 256];
    do {
      DecodeBit(s, c, p[c]);
    } while (c < 0x100);
    s->ctx = c & 0xFF;
    dst[i] = s->ctx;
  }
  return 0;
}

static int decode_match(CSCDecState *s, uint32_t *dist_ptr, uint32_t *len_ptr) {
  uint32_t len = decode_matchlen_2(s);
  uint32_t dist = 0;
  uint32_t pdist_pos, sbits;
  switch (len) {
  case 0:
    pdist_pos = 0;
    sbits = 3;
    break;
  case 1:
  case 2:
    pdist_pos = 16 * (len - 1) + 8;
    sbits = 4;
    break;
  case 3:
  case 4:
  case 5:
    pdist_pos = 32 * (len - 3) + 8 + 16 * 2;
    sbits = 5;
    break;
  default:
    pdist_pos = 32 * 3 + 8 + 16 * 2;
    sbits = 5;
    break;
  };
  uint32_t *p = s->p_dist + pdist_pos;
  uint32_t slot, i = 1;
  do {
    DecodeBit(s, i, p[i]);
  } while (i < (1u << sbits));
  slot = i & ((1 << sbits) - 1);
  if (slot <= 2)
    dist = slot;
  else {
    uint32_t ebits = slot - 2;
    uint32_t elen = 0;
    if (ebits > 4)
      DecodeDirect(s, elen, ebits - 4);
    i = 1;
    p = &s->p_matchdist_extra[(ebits - 1) * 16];
    do {
      DecodeBit(s, i, p[i]);
    } while (i < 0x10);
    dist = dist_table_[slot] + (elen << 4) + rev16_table_[i & 0x0F];
  }
  s->state = (s->state * 4 + 1) & 0x3F;
  *len_ptr = len;
  *dist_ptr = dist;
  return 0;
}

static void decode_1byte_match(CSCDecState *s) {
  s->state = (s->state * 4 + 2) & 0x3F;
  s->ctx = 0;
}

static int decode_repdist_match(CSCDecState *s, uint32_t *rep_idx,
                                uint32_t *match_len) {
  uint32_t i = 1;
  do {
    DecodeBit(s, i, s->p_repdist[s->state * 3 + i - 1]);
  } while (i < 0x4);
  *rep_idx = i & 0x3;
  *match_len = decode_matchlen_2(s);
  s->state = (s->state * 4 + 3) & 0x3F;
  return 0;
}

static int lz_decode(CSCDecState *s, uint8_t *dst, uint32_t *size,
                     uint32_t limit) {
  uint32_t copied_size = 0;
  uint32_t copied_wndpos = s->wnd_curpos;
  uint32_t i;

  // Initialize

  for (i = 0; i <= limit;) {
    uint32_t v = 0;
    DecodeBit(s, v, s->p_state[s->state * 3 + 0]);
    if (v == 0) {
      s->wnd[s->wnd_curpos++] = decode_literal(s);
      i++;
    } else {
      v = 0;
      DecodeBit(s, v, s->p_state[s->state * 3 + 1]);

      uint32_t dist = 0, len = 0, cpy_pos;
      uint8_t *cpy_src, *cpy_dst;
      if (v == 1) {
        decode_match(s, &dist, &len);
        if (len == 0 && dist == 64) {
          break;
        }
        dist++;
        len += 2;
        s->rep_dist[3] = s->rep_dist[2];
        s->rep_dist[2] = s->rep_dist[1];
        s->rep_dist[1] = s->rep_dist[0];
        s->rep_dist[0] = dist;
        cpy_pos = s->wnd_curpos >= dist ? s->wnd_curpos - dist
                                        : s->wnd_curpos + s->wnd_size - dist;
        if (cpy_pos >= s->wnd_size || cpy_pos + len > s->wnd_size ||
            len + i > limit || s->wnd_curpos + len > s->wnd_size)
          return DECODE_ERROR;

        cpy_dst = s->wnd + s->wnd_curpos;
        cpy_src = s->wnd + cpy_pos;
        i += len;
        s->wnd_curpos += len;
        while (len--) {
          *cpy_dst++ = *cpy_src++;
        }
        s->ctx = (s->wnd[s->wnd_curpos - 1]) >> 0;
      } else {
        v = 0;
        DecodeBit(s, v, s->p_state[s->state * 3 + 2]);
        if (v == 0) {
          decode_1byte_match(s);
          cpy_pos = s->wnd_curpos > s->rep_dist[0]
                        ? s->wnd_curpos - s->rep_dist[0]
                        : s->wnd_curpos + s->wnd_size - s->rep_dist[0];
          s->wnd[s->wnd_curpos++] = s->wnd[cpy_pos];
          i++;
          s->ctx = (s->wnd[s->wnd_curpos - 1]) >> 0;
        } else {
          uint32_t repdist_idx = 0; // Init
          decode_repdist_match(s, &repdist_idx, &len);
          len += 2;
          if (len + i > limit) {
            return DECODE_ERROR;
          }

          dist = s->rep_dist[repdist_idx];
          for (int j = repdist_idx; j > 0; j--)
            s->rep_dist[j] = s->rep_dist[j - 1];
          s->rep_dist[0] = dist;

          cpy_pos = s->wnd_curpos >= dist ? s->wnd_curpos - dist
                                          : s->wnd_curpos + s->wnd_size - dist;
          if (cpy_pos >= s->wnd_size || cpy_pos + len > s->wnd_size ||
              len + i > limit || s->wnd_curpos + len > s->wnd_size)
            return DECODE_ERROR;
          cpy_dst = s->wnd + s->wnd_curpos;
          cpy_src = s->wnd + cpy_pos;
          i += len;
          s->wnd_curpos += len;
          while (len--)
            *cpy_dst++ = *cpy_src++;
          s->ctx = (s->wnd[s->wnd_curpos - 1]) >> 0;
        }
      }
    }

    if (s->wnd_curpos > s->wnd_size) {
      return DECODE_ERROR;
    } else if (s->wnd_curpos == s->wnd_size) {
      s->wnd_curpos = 0;
      memcpy(dst + copied_size, s->wnd + copied_wndpos, i - copied_size);
      copied_wndpos = 0;
      copied_size = i;
    }
  }
  *size = i;
  memcpy(dst + copied_size, s->wnd + copied_wndpos, *size - copied_size);
  return 0;
}

static void lz_copy2dict(CSCDecState *s, uint8_t *src, uint32_t size) {
  for (uint32_t i = 0; i < size;) {
    uint32_t cur_block;
    cur_block = MIN(s->wnd_size - s->wnd_curpos, size - i);
    cur_block = MIN(cur_block, 8192);

    memcpy(s->wnd + s->wnd_curpos, src + i, cur_block);
    s->wnd_curpos += cur_block;
    s->wnd_curpos = s->wnd_curpos >= s->wnd_size ? 0 : s->wnd_curpos;
    i += cur_block;
  }
}

// Init
static void InitDecoder(CSCDecState *s) {
  // Init probs
  int i;
  for (i = 0; i < 64 * 3; i++)
    s->p_state[i] = 2048;
  for (i = 0; i < 256 * 256; i++)
    s->p_lit[i] = 2048;
  for (i = 0; i < 64 * 4; i++)
    s->p_repdist[i] = 2048;
  for (i = 0; i < 8 + 16 * 2 + 32 * 4; i++)
    s->p_dist[i] = 2048;
  for (i = 0; i < 16; i++)
    s->p_rle_len[i] = 2048;

  for (i = 0; i < 2; i++)
    s->p_matchlen_slot[i] = 2048;
  for (i = 0; i < 8; i++)
    s->p_matchlen_extra1[i] = 2048;
  for (i = 0; i < 8; i++)
    s->p_matchlen_extra2[i] = 2048;
  for (i = 0; i < 128; i++)
    s->p_matchlen_extra3[i] = 2048;
  for (i = 0; i < 29 * 16; i++)
    s->p_matchdist_extra[i] = 2048;

  s->p_longlen = 2048;
  s->p_rle_flag = 2048;
  s->state = 0;
  s->ctx = 0;

  s->wnd_curpos = 0;
  s->rep_dist[0] = s->rep_dist[1] = s->rep_dist[2] = s->rep_dist[3] = 0;

  s->rc_low = 0;
  s->rc_range = 0xFFFFFFFF;
  s->rc_code = 0;
  s->rc_size = 0;
  s->rc_bufsize = 0;

  s->bc_curbits = 0;
  s->bc_curval = 0;
  s->bc_size = 0;
  s->bc_bufsize = 0;

  CSC_Filters_Init();
}

intptr_t CSC_Dec_Decompress(const void *src, size_t src_len, void *dst,
                            size_t dst_cap, void *workmem) {
  if (!src || !dst || !workmem)
    return -1;

  CSCDecProps props;
  Dec_ReadProperties(&props, (const uint8_t *)src);

  CSCDecState *s = (CSCDecState *)workmem;
  uint8_t *p = (uint8_t *)(s + 1);

  p = (uint8_t *)(((uintptr_t)p + 15) & ~15);

  s->src_cur = (const uint8_t *)src + CSC_PROP_SIZE;
  s->src_end = (const uint8_t *)src + src_len;

  s->dict_size = props.dict_size;
  // Set wnd_size
  s->wnd_size = props.dict_size;
  s->csc_blocksize = props.csc_blocksize;
  s->raw_blocksize = props.raw_blocksize;

  s->p_lit = (uint32_t *)p;
  p += 256 * 256 * sizeof(uint32_t);

  s->p_delta = (uint32_t *)p;
  p += 256 * 256 * sizeof(uint32_t);

  s->wnd = p;
  p += props.dict_size + 8;

  s->rc_buf = p;
  // Double buffer size
  p += (size_t)props.csc_blocksize * (size_t)CSC_IOBUF_MULT;

  s->bc_buf = p;
  // Double buffer size
  p += (size_t)props.csc_blocksize * (size_t)CSC_IOBUF_MULT;

  s->swap_buf = p;

  InitDecoder(s);

  // Initial fetch
  if (s->rc_bufsize == 0) {
    if (FetchBlock(s, 1) < 0)
      return -1;
  }
  if (s->bc_bufsize == 0) {
    if (FetchBlock(s, 0) < 0)
      return -1;
  }

  if (s->rc_bufsize < 5)
    return -1;
  s->rc_code = ((uint32_t)s->rc_buf[1] << 24) | ((uint32_t)s->rc_buf[2] << 16) |
               ((uint32_t)s->rc_buf[3] << 8) | ((uint32_t)s->rc_buf[4]);
  s->rc_size = 5;

  uint8_t *dst_u8 = (uint8_t *)dst;
  size_t total_out = 0;

  {
    int i;
    for (i = 0; i < 256 * 256; i++)
      s->p_delta[i] = 2048;
  }

  for (;;) {
    uint32_t size;
    int ret = 0;

    uint32_t type = decode_int(s);

    if (type == DT_NORMAL) {
      ret = lz_decode(s, dst_u8 + total_out, &size, s->raw_blocksize);
      if (ret < 0)
        return -1;
    } else if (type == DT_EXE) {
      ret = lz_decode(s, dst_u8 + total_out, &size, s->raw_blocksize);
      if (ret < 0)
        return -1;
      CSC_Filters_Inverse_E89(dst_u8 + total_out, size);
    } else if (type == DT_ENGTXT) {
      size = decode_int(s);
      ret = lz_decode(s, dst_u8 + total_out, &size, s->raw_blocksize);
      if (ret < 0)
        return -1;
      CSC_Filters_Inverse_Dict(dst_u8 + total_out, size, s->swap_buf);
    } else if (type == DT_BAD) {
      ret = decode_bad(s, dst_u8 + total_out, &size, s->raw_blocksize);
      if (ret < 0)
        return -1;
      lz_copy2dict(s, dst_u8 + total_out, size);
    } else if (type == DT_ENTROPY) {
      ret = decode_literals(s, dst_u8 + total_out, &size, s->raw_blocksize);
      if (ret < 0)
        return -1;
      lz_copy2dict(s, dst_u8 + total_out, size);
    } else if (type == SIG_EOF) {
      size = 0;
      break;
    } else if (type >= DT_DLT && type < DT_DLT + DLT_CHANNEL_MAX) {
      uint32_t chnNum = DltIndex[type - DT_DLT];
      ret = decode_rle(s, dst_u8 + total_out, &size, s->raw_blocksize);
      if (ret < 0)
        return -1;
      CSC_Filters_Inverse_Delta(dst_u8 + total_out, size, chnNum, s->swap_buf);
      lz_copy2dict(s, dst_u8 + total_out, size);
    } else {
      return -1;
    }

    total_out += size;
    if (total_out > dst_cap)
      return -1;

    if (decode_int(s) == 1) {
      // Reset RC/BC
      s->rc_low = 0;
      s->rc_range = 0xFFFFFFFF;
      s->rc_code = 0;
      s->rc_size = 0;
      s->rc_bufsize = 0;

      s->bc_curbits = 0;
      s->bc_curval = 0;
      s->bc_size = 0;
      s->bc_bufsize = 0;

      if (s->rc_bufsize == 0) {
        if (FetchBlock(s, 1) < 0)
          return -1;
      }
      if (s->bc_bufsize == 0) {
        if (FetchBlock(s, 0) < 0)
          return -1;
      }

      if (s->rc_bufsize < 5)
        return -1;
      s->rc_code = ((uint32_t)s->rc_buf[1] << 24) |
                   ((uint32_t)s->rc_buf[2] << 16) |
                   ((uint32_t)s->rc_buf[3] << 8) | ((uint32_t)s->rc_buf[4]);
      s->rc_size = 5;
    }
  }

  return (intptr_t)total_out;
}
