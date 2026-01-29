/*
 * PowerPacker Decompressor - Standalone C Implementation
 * Based on pplib 1.2 by Stuart Caie (Public Domain, 25-Nov-2010)
 *
 * PowerPacker format: PP20 (popular on Amiga in late 80s/early 90s)
 *
 * This is a minimal standalone decompressor optimized for size.
 */

#include "powerpacker_decompress.h"
#include <string.h>

/* Bit reading macro - reads bits from end of compressed data backwards */
#define PP_READ_BITS(nbits, var)                                               \
  do {                                                                         \
    bit_cnt = (nbits);                                                         \
    (var) = 0;                                                                 \
    while (bits_left < bit_cnt) {                                              \
      if (buf < src)                                                           \
        return 0; /* out of source bits */                                     \
      bit_buffer |= *--buf << bits_left;                                       \
      bits_left += 8;                                                          \
    }                                                                          \
    bits_left -= bit_cnt;                                                      \
    while (bit_cnt--) {                                                        \
      (var) = ((var) << 1) | (bit_buffer & 1);                                 \
      bit_buffer >>= 1;                                                        \
    }                                                                          \
  } while (0)

/* Byte output macro - writes bytes backwards from end of dest buffer */
#define PP_BYTE_OUT(byte)                                                      \
  do {                                                                         \
    if (out <= dest)                                                           \
      return 0; /* output overflow */                                          \
    *--out = (byte);                                                           \
    written++;                                                                 \
  } while (0)

/*
 * Main decompression function
 * PowerPacker uses backward decompression (from end to start)
 */
static int pp_decrunch_buffer(const unsigned char *eff,
                              const unsigned char *src, unsigned char *dest,
                              unsigned int src_len, unsigned int dest_len,
                              const unsigned int litbit) {
  const unsigned char *buf = &src[src_len];
  unsigned char *out = &dest[dest_len], *dest_end = out;
  unsigned int bit_buffer = 0, x, todo, offbits, offset = 0, written = 0;
  unsigned char bits_left = 0, bit_cnt;

  if (src == NULL || dest == NULL)
    return 0;

  /* skip the first few bits */
  PP_READ_BITS(src[src_len + 3], x);

  /* while we still have output to unpack */
  while (written < dest_len) {
    PP_READ_BITS(1, x);
    if (x == litbit) {
      /* literal run */
      todo = 1;
      do {
        PP_READ_BITS(2, x);
        todo += x;
      } while (x == 3);

      while (todo--) {
        PP_READ_BITS(8, x);
        PP_BYTE_OUT(x);
      }

      /* should we end decoding on a literal, break out */
      if (written == dest_len)
        break;
    }

    /* match */
    PP_READ_BITS(2, x);
    offbits = eff[x];
    todo = x + 2;
    if (x == 3) {
      PP_READ_BITS(1, x);
      if (x == 0)
        offbits = 7;
      PP_READ_BITS(offbits, offset);
      do {
        PP_READ_BITS(3, x);
        todo += x;
      } while (x == 7);
    } else {
      PP_READ_BITS(offbits, offset);
    }

    if (&out[offset] >= dest_end)
      return 0; /* match overflow */

    while (todo--) {
      x = out[offset];
      PP_BYTE_OUT(x);
    }
  }

  /* all output bytes written without error */
  return 1;
}

/*
 * Get decompressed size from PowerPacker data
 */
unsigned int powerpacker_get_decompressed_size(const unsigned char *src,
                                               unsigned int src_len) {
  if (!src || src_len < 12)
    return 0;

  /* Check for PP20 magic */
  if (src[0] != 'P' || src[1] != 'P' || src[2] != '2' || src[3] != '0')
    return 0;

  /* Decompressed size is stored in last 3 bytes (before final byte) */
  return (src[src_len - 4] << 16) | (src[src_len - 3] << 8) | src[src_len - 2];
}

/*
 * Decompress PowerPacker data
 */
int powerpacker_decompress(const unsigned char *src, unsigned int src_len,
                           unsigned char *dest, unsigned int dest_len) {
  unsigned int decompressed_size;
  int efficiency_off = 0;
  int src_off = 0;

  if (!src || !dest || src_len < 12)
    return 0;

  /* Check magic and set offsets */
  uint32_t magic = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

  switch (magic) {
  case 0x50503230: /* PP20 */
    efficiency_off = 4;
    src_off =
        8; /* skip magic + efficiency table; src points to crunched stream */
    break;
  default:
    return 0; /* unsupported format */
  }

  /* Get decompressed size */
  decompressed_size =
      (src[src_len - 4] << 16) | (src[src_len - 3] << 8) | src[src_len - 2];

  /* Handle >16MB files where stored size wraps around */
  if (dest_len > 0xFFFFFF && (dest_len & 0xFFFFFF) == decompressed_size) {
    decompressed_size = dest_len;
  }

  /* Check if output buffer is large enough */
  if (decompressed_size > dest_len)
    return 0;

  /* Decompress: try normal mode first (litbit=0), then master mode (litbit=1)
   */
  if (pp_decrunch_buffer(&src[efficiency_off], &src[src_off], dest,
                         src_len - src_off - 4, decompressed_size, 0)) {
    return (int)decompressed_size;
  }
  if (pp_decrunch_buffer(&src[efficiency_off], &src[src_off], dest,
                         src_len - src_off - 4, decompressed_size, 1)) {
    return (int)decompressed_size;
  }
  return 0;
}
