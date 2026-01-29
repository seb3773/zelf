#include "codec_shrinkler.h"
#include <stddef.h>
#include <stdint.h>

// ASM entry point
extern size_t ShrinklerDecompress(const unsigned char *src, unsigned char *dst,
                                  void (*progress_cb)(size_t, void *),
                                  void *cb_arg, unsigned int *contexts);

// Header layout produced by shrinkler_comp.cpp (big-endian):
//  0..3   : 'S' 'h' 'r' 'i'
//  4      : major
//  5      : minor
//  6..7   : header_size_minus_8 (u16 BE)
//  8..11  : compressed_payload_size (u32 BE)
//  12..15 : uncompressed_size (u32 BE)
//  16..19 : front_overlap_margin (u32 BE)
//  20..23 : flags (u32 BE)  bit0 = parity_context

static inline unsigned be16(const unsigned char *p) {
  return ((unsigned)p[0] << 8) | (unsigned)p[1];
}
static inline unsigned be32(const unsigned char *p) {
  return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
         ((unsigned)p[2] << 8) | (unsigned)p[3];
}

#define SHR_NUM_CONTEXTS 1536

int shrinkler_decompress_c(const char *src, char *dst, int compressedSize,
                           int dstCapacity) {
  if (!src || !dst || compressedSize < 24 || dstCapacity <= 0)
    return -1;
  const unsigned char *s = (const unsigned char *)src;
  if (s[0] != 'S' || s[1] != 'h' || s[2] != 'r' || s[3] != 'i')
    return -2;

  unsigned hdr_rem = be16(s + 6);
  unsigned hdr_sz = 8u + hdr_rem;
  if ((int)hdr_sz > compressedSize || hdr_sz < 24u)
    return -3;

  unsigned comp_payload = be32(s + 8);
  unsigned uncomp_size = be32(s + 12);
  unsigned margin = be32(s + 16); // front overlap margin
  unsigned flags = be32(s + 20);
  (void)flags; // parity_context unsupported in ASM (compiled for bytes)

  {
    uint64_t cap = (uint64_t)(unsigned)dstCapacity;
    uint64_t u = (uint64_t)uncomp_size;
    uint64_t m = (uint64_t)margin;

    if (u > cap)
      return -4;
    // Reject insane margin values (prevents dst_work wrapping far away)
    // Note: this also implicitly covers unsigned addition overflow.
    if (m > cap)
      return -8;
    // Require extra capacity to place margin in front of dst
    if (u + m > cap)
      return -8;
  }
  if ((unsigned)compressedSize < hdr_sz + comp_payload)
    return -5;

  const unsigned char *payload = s + hdr_sz;
  // Use a local context buffer to avoid relying on .bss in the stub image
  unsigned int ctx[SHR_NUM_CONTEXTS + 1];
  for (int i = 0; i < (int)SHR_NUM_CONTEXTS; ++i) {
    ctx[i] = 0x8000u; // Initialize adaptive contexts to 0x8000
  }
  // Decode into dst + margin to allow back-references before start
  unsigned char *dst_work = (unsigned char *)dst + margin;
  size_t written = ShrinklerDecompress(payload, dst_work, 0, 0, ctx);
  if ((int)written <= 0)
    return -6;
  if (uncomp_size && written != (size_t)uncomp_size) {
    // Size mismatch -> error to avoid running corrupted payload
    return -7;
  }
  // Move down to dst base in-place (forward copy is safe since dst < dst_work)
  for (unsigned i = 0; i < uncomp_size; ++i) {
    ((unsigned char *)dst)[i] = dst_work[i];
  }
  return (int)written;
}
