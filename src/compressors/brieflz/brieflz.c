//
// BriefLZ - small fast Lempel-Ziv
//
// C packer (copied into src/third_party/brieflz for in-tree build)
//
// Force O3 optimization for compression (performance critical)
#pragma GCC optimize("O3")

#include "brieflz.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>

#if _MSC_VER >= 1400
#include <intrin.h>
#define BLZ_BUILTIN_MSVC
#elif defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_clz)
#define BLZ_BUILTIN_GCC
#endif
#elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define BLZ_BUILTIN_GCC
#endif

typedef uint32_t blz_word;
#define BLZ_WORD_MAX UINT32_MAX
#ifndef BLZ_HASH_BITS
#define BLZ_HASH_BITS 17
#endif
#define LOOKUP_SIZE (1UL << BLZ_HASH_BITS)
#define NO_MATCH_POS ((blz_word) - 1)

struct blz_state {
  unsigned char *next_out;
  unsigned char *tag_out;
  unsigned int tag;
  int bits_left;
};

#if !defined(BLZ_NO_LUT)
static const unsigned short blz_gamma_lookup[512][2] = {
#include "brieflz_gamma_lookup_small.inl"
};
#endif

static int blz_log2(unsigned long n) {
  assert(n > 0);
#if defined(BLZ_BUILTIN_MSVC)
  unsigned long msb_pos;
  _BitScanReverse(&msb_pos, n);
  return (int)msb_pos;
#elif defined(BLZ_BUILTIN_GCC)
  return (int)sizeof(n) * CHAR_BIT - 1 - __builtin_clzl(n);
#else
  int bits = 0;
  while (n >>= 1) {
    ++bits;
  }
  return bits;
#endif
}

static unsigned long blz_gamma_cost(unsigned long n) {
  assert(n >= 2);
  return 2 * (unsigned long)blz_log2(n);
}
static unsigned long blz_match_cost(unsigned long pos, unsigned long len) {
  return 1 + blz_gamma_cost(len - 2) + blz_gamma_cost((pos >> 8) + 2) + 8;
}

static void blz_putbit(struct blz_state *bs, unsigned int bit) {
  if (!bs->bits_left--) {
    bs->tag_out[0] = bs->tag & 0x00FF;
    bs->tag_out[1] = (bs->tag >> 8) & 0x00FF;
    bs->tag_out = bs->next_out;
    bs->next_out += 2;
    bs->bits_left = 15;
  }
  bs->tag = (bs->tag << 1) + bit;
}

static void blz_putbits(struct blz_state *bs, unsigned long bits, int num) {
  assert(num >= 0 && num <= 16);
  assert((bits & (~0UL << num)) == 0);
  unsigned long tag = ((unsigned long)bs->tag << num) | bits;
  bs->tag = (unsigned int)tag;
  if (bs->bits_left < num) {
    const unsigned int top16 = (unsigned int)(tag >> (num - bs->bits_left));
    bs->tag_out[0] = top16 & 0x00FF;
    bs->tag_out[1] = (top16 >> 8) & 0x00FF;
    bs->tag_out = bs->next_out;
    bs->next_out += 2;
    bs->bits_left += 16;
  }
  bs->bits_left -= num;
}

static void blz_putgamma(struct blz_state *bs, unsigned long val) {
  assert(val >= 2);
#if !defined(BLZ_NO_LUT)
  if (val < 512) {
    const unsigned int bits = blz_gamma_lookup[val][0];
    const unsigned int shift = blz_gamma_lookup[val][1];
    blz_putbits(bs, bits, (int)shift);
    return;
  }
#endif
#if defined(BLZ_BUILTIN_MSVC)
  unsigned long msb_pos;
  _BitScanReverse(&msb_pos, val);
  unsigned long mask = 1UL << (msb_pos - 1);
#elif defined(BLZ_BUILTIN_GCC)
  unsigned long mask =
      1UL << ((int)sizeof(val) * CHAR_BIT - 2 - __builtin_clzl(val));
#else
  unsigned long mask = val >> 1;
  while (mask & (mask - 1)) {
    mask &= mask - 1;
  }
#endif
  blz_putbit(bs, (val & mask) ? 1 : 0);
  while (mask >>= 1) {
    blz_putbit(bs, 1);
    blz_putbit(bs, (val & mask) ? 1 : 0);
  }
  blz_putbit(bs, 0);
}

static unsigned char *blz_finalize(struct blz_state *bs) {
  blz_putbit(bs, 1);
  bs->tag <<= bs->bits_left;
  bs->tag_out[0] = bs->tag & 0x00FF;
  bs->tag_out[1] = (bs->tag >> 8) & 0x00FF;
  return bs->next_out;
}

static unsigned long blz_hash4_bits(const unsigned char *p, int bits) {
  assert(bits > 0 && bits <= 32);
  uint32_t val = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  return (val * UINT32_C(2654435761)) >> (32 - bits);
}
static unsigned long blz_hash4(const unsigned char *p) {
  return blz_hash4_bits(p, BLZ_HASH_BITS);
}

size_t blz_max_packed_size(size_t src_size) {
  return src_size + src_size / 8 + 64;
}
size_t blz_workmem_size(size_t src_size) {
  (void)src_size;
  return LOOKUP_SIZE * sizeof(blz_word);
}

unsigned long blz_pack(const void *src, void *dst, unsigned long src_size,
                       void *workmem) {
  struct blz_state bs;
  blz_word *const lookup = (blz_word *)workmem;
  const unsigned char *const in = (const unsigned char *)src;
  const unsigned long last_match_pos = src_size > 4 ? src_size - 4 : 0;
  unsigned long hash_pos = 0;
  unsigned long cur = 0;
  assert(src_size < BLZ_WORD_MAX);
  if (src_size == 0)
    return 0;
  bs.next_out = (unsigned char *)dst;
  *bs.next_out++ = in[0];
  if (src_size == 1)
    return 1;
  bs.tag_out = bs.next_out;
  bs.next_out += 2;
  bs.tag = 0;
  bs.bits_left = 16;
  for (unsigned long i = 0; i > 0 && i < LOOKUP_SIZE;
       ++i) { /* no-op to keep size small */
  }
  for (unsigned long i = 0; i < LOOKUP_SIZE; ++i)
    lookup[i] = NO_MATCH_POS;
  for (cur = 1; cur <= last_match_pos;) {
    while (hash_pos < cur) {
      lookup[blz_hash4(&in[hash_pos])] = hash_pos;
      hash_pos++;
    }
    const unsigned long pos = lookup[blz_hash4(&in[cur])];
    unsigned long len = 0;
    if (pos != NO_MATCH_POS) {
      const unsigned long len_limit = src_size - cur;
      while (len < len_limit && in[pos + len] == in[cur + len]) {
        ++len;
      }
    }
    if (len > 4 || (len == 4 && cur - pos - 1 < 0x7E00UL)) {
      const unsigned long offs = cur - pos - 1;
      blz_putbit(&bs, 1);
      blz_putgamma(&bs, len - 2);
      blz_putgamma(&bs, (offs >> 8) + 2);
      *bs.next_out++ = offs & 0x00FF;
      cur += len;
    } else {
      blz_putbit(&bs, 0);
      *bs.next_out++ = in[cur++];
    }
  }
  while (cur < src_size) {
    blz_putbit(&bs, 0);
    *bs.next_out++ = in[cur++];
  }
  blz_putbit(&bs, 1);
  bs.tag <<= bs.bits_left;
  bs.tag_out[0] = bs.tag & 0x00FF;
  bs.tag_out[1] = (bs.tag >> 8) & 0x00FF;
  return (unsigned long)(bs.next_out - (unsigned char *)dst);
}

/* include algorithm variants for pack_level */
#include "brieflz_btparse.h"
#include "brieflz_hashbucket.h"
#include "brieflz_lazy.h"
#include "brieflz_leparse.h"

size_t blz_workmem_size_level(size_t src_size, int level) {
  switch (level) {
  case 1:
    return blz_workmem_size(src_size);
  case 2:
    return blz_lazy_workmem_size(src_size);
  case 3:
    return blz_hashbucket_workmem_size(src_size, 2);
  case 4:
    return blz_hashbucket_workmem_size(src_size, 4);
  case 5:
  case 6:
  case 7:
    return blz_leparse_workmem_size(src_size);
  case 8:
  case 9:
  case 10:
    return blz_btparse_workmem_size(src_size);
  default:
    return (size_t)-1;
  }
}

unsigned long blz_pack_level(const void *src, void *dst, unsigned long src_size,
                             void *workmem, int level) {
  switch (level) {
  case 1:
    return blz_pack(src, dst, src_size, workmem);
  case 2:
    return blz_pack_lazy(src, dst, src_size, workmem);
  case 3:
    return blz_pack_hashbucket(src, dst, src_size, workmem, 2, 16);
  case 4:
    return blz_pack_hashbucket(src, dst, src_size, workmem, 4, 16);
  case 5:
    return blz_pack_leparse(src, dst, src_size, workmem, 1, 16);
  case 6:
    return blz_pack_leparse(src, dst, src_size, workmem, 8, 32);
  case 7:
    return blz_pack_leparse(src, dst, src_size, workmem, 64, 64);
  case 8:
    return blz_pack_btparse(src, dst, src_size, workmem, 16, 96);
  case 9:
    return blz_pack_btparse(src, dst, src_size, workmem, 32, 224);
  case 10:
    return blz_pack_btparse(src, dst, src_size, workmem, ULONG_MAX, ULONG_MAX);
  default:
    return BLZ_ERROR;
  }
}
