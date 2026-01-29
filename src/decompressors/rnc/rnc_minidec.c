#include "rnc_minidec.h"

/* Complexity: O(out_len) time.
 * Dependencies: none.
 * Alignment: none.
 * Notes: method 1 only ("RNC\x01"). CRC is not checked to keep the stub small.
 */

typedef struct {
  uint16_t bit_depth;
  uint32_t l3;
} rnc_huf_t;

static inline uint16_t rnc_read_be16(const unsigned char *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t rnc_read_be32(const unsigned char *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
         (uint32_t)p[3];
}

static inline uint32_t rnc_inverse_bits(uint32_t value, int count) {
  uint32_t r = 0;
  while (count--) {
    r <<= 1;
    r |= (value & 1u);
    value >>= 1;
  }
  return r;
}

static inline void rnc_build_codes(rnc_huf_t *t, int count) {
  uint32_t val = 0;
  uint32_t div = 0x80000000u;
  int bits = 1;

  while (bits <= 16) {
    int i = 0;
    for (;;) {
      if (i >= count) {
        bits++;
        div >>= 1;
        break;
      }
      if ((int)t[i].bit_depth == bits) {
        t[i].l3 = rnc_inverse_bits(val / div, bits);
        val += div;
      }
      i++;
    }
  }
}

typedef struct {
  const unsigned char *p;
  const unsigned char *end;
  const unsigned char *src;
  uint32_t src_len;
  uint32_t src_off;
  unsigned char *pack_block_start;
  unsigned char mem1[0xFFFFu];
  uint32_t bitbuf;
  uint32_t bitcnt;
  int err;
} rnc_in_t;

static inline unsigned char rnc_read_source_byte(rnc_in_t *in) {
  if (__builtin_expect(in->pack_block_start == &in->mem1[0xFFFDu], 0)) {
    int left = (int)in->src_len - (int)in->src_off;
    int size_to_read = (left <= (int)0xFFFD) ? left : (int)0xFFFD;
    if (__builtin_expect(size_to_read < 0, 0))
      size_to_read = 0;

    in->pack_block_start = in->mem1;

    if (size_to_read) {
      __builtin_memcpy(in->pack_block_start, in->src + (size_t)in->src_off,
                       (size_t)size_to_read);
      in->src_off += (uint32_t)size_to_read;
    }

    int left2;
    if (left - size_to_read > 2)
      left2 = 2;
    else
      left2 = left - size_to_read;
    if (left2 < 0)
      left2 = 0;

    if (left2) {
      __builtin_memcpy(&in->mem1[(unsigned)size_to_read],
                       in->src + (size_t)in->src_off, (size_t)left2);
      in->src_off += (uint32_t)left2;
      in->src_off -= (uint32_t)left2;
    }
  }

  return *in->pack_block_start++;
}

static inline void rnc_sync_after_raw(rnc_in_t *in) {
  uint32_t next3 = ((uint32_t)in->pack_block_start[2] << 16) |
                   ((uint32_t)in->pack_block_start[1] << 8) |
                   (uint32_t)in->pack_block_start[0];
  uint32_t keep =
      (in->bitcnt == 0) ? 0u : (in->bitbuf & ((1u << in->bitcnt) - 1u));
  in->bitbuf = (next3 << in->bitcnt) | keep;
}

static inline uint32_t rnc_input_bits_m1(rnc_in_t *in, int count) {
  uint32_t bits = 0;
  uint32_t prev = 1;

  while (count--) {
    if (!in->bitcnt) {
      unsigned char b1 = rnc_read_source_byte(in);
      unsigned char b2 = rnc_read_source_byte(in);
      in->bitbuf = ((uint32_t)in->pack_block_start[1] << 24) |
                   ((uint32_t)in->pack_block_start[0] << 16) |
                   ((uint32_t)b2 << 8) | (uint32_t)b1;
      in->bitcnt = 16;
    }

    if (in->bitbuf & 1u)
      bits |= prev;
    in->bitbuf >>= 1;
    prev <<= 1;
    in->bitcnt--;
  }

  return bits;
}

static inline void rnc_make_huftable(rnc_in_t *in, rnc_huf_t *t) {
  for (int i = 0; i < 16; ++i) {
    t[i].bit_depth = 0;
    t[i].l3 = 0;
  }

  int leaf = (int)rnc_input_bits_m1(in, 5);
  if (in->err)
    return;

  if (leaf > 16)
    leaf = 16;

  if (leaf) {
    for (int i = 0; i < leaf; ++i) {
      t[i].bit_depth = (uint16_t)rnc_input_bits_m1(in, 4);
      if (in->err)
        return;
    }
    rnc_build_codes(t, leaf);
  }
}

static inline uint32_t rnc_decode_table_data(rnc_in_t *in, rnc_huf_t *t) {
  uint32_t i = 0;

  for (;;) {
    if (__builtin_expect(i >= 16u, 0)) {
      in->err = 2;
      return 0;
    }
    uint32_t bd = (uint32_t)t[i].bit_depth;
    if (bd) {
      if (t[i].l3 == (in->bitbuf & ((1u << bd) - 1u))) {
        (void)rnc_input_bits_m1(in, (int)bd);
        if (in->err)
          return 0;
        if (i < 2)
          return i;
        return rnc_input_bits_m1(in, (int)i - 1) | (1u << ((int)i - 1));
      }
    }
    i++;
  }
}

int rnc_decompress_to(const unsigned char *src, int src_len, unsigned char *dst,
                      int dst_cap) {
  if (!src || !dst || src_len < 18 || dst_cap <= 0)
    return -10;

  /* Parse RNC header (big-endian):
   * [0]  u32: ("RNC"<<8)|method
   * [4]  u32: unpacked_size
   * [8]  u32: packed_size (payload only)
   * [12] u16: unpacked_crc (ignored)
   * [14] u16: packed_crc (ignored)
   * [16] u8 : leeway (ignored)
   * [17] u8 : chunks_count (ignored)
   */
  const unsigned char *h = src;
  uint32_t sign = rnc_read_be32(h);
  if ((sign >> 8) != 0x524E43u)
    return -11;

  int method = (int)(sign & 0xFFu);
  if (method != 1)
    return -12;

  uint32_t out_len = rnc_read_be32(h + 4);
  uint32_t packed_len = rnc_read_be32(h + 8);
  if (out_len > (uint32_t)dst_cap)
    return -13;

  if ((uint32_t)src_len < 18u + packed_len)
    return -14;

  rnc_in_t in;
  in.p = 0;
  in.end = 0;
  in.src = src + 18;
  in.src_len = packed_len;
  in.src_off = 0;
  in.pack_block_start = &in.mem1[0xFFFDu];
  in.bitbuf = 0;
  in.bitcnt = 0;
  in.err = 0;

  /* Consume lock/key bits written by encoder (2 bits, ignored). */
  (void)rnc_input_bits_m1(&in, 1);
  (void)rnc_input_bits_m1(&in, 1);
  if (in.err)
    return -20;

  rnc_huf_t raw[16], len[16], pos[16];

  uint32_t produced = 0;
  while (produced < out_len) {
    rnc_make_huftable(&in, raw);
    rnc_make_huftable(&in, len);
    rnc_make_huftable(&in, pos);
    if (in.err)
      return -21;

    uint32_t subchunks = rnc_input_bits_m1(&in, 16);
    if (in.err)
      return -22;

    while (subchunks--) {
      uint32_t data_len = rnc_decode_table_data(&in, raw);
      if (in.err)
        return -23;

      uint32_t raw_len = data_len;

      if (__builtin_expect(produced + data_len > out_len, 0))
        return -30;

      while (data_len--) {
        dst[produced++] = rnc_read_source_byte(&in);
      }

      if (raw_len)
        rnc_sync_after_raw(&in);

      if (subchunks) {
        uint32_t match_off = rnc_decode_table_data(&in, len) + 1;
        uint32_t match_cnt = rnc_decode_table_data(&in, pos) + 2;
        if (in.err)
          return (in.err == 1) ? -241 : -242;

        if (__builtin_expect(match_off == 0 || match_off > produced, 0))
          return -32;
        if (__builtin_expect(produced + match_cnt > out_len, 0))
          return -33;

        while (match_cnt--) {
          dst[produced] = dst[produced - match_off];
          produced++;
        }
      }
    }
  }

  return (int)produced;
}
