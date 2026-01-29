/**
 * @file nz1_decompressor.c
 * @brief NanoZip Decompressor Implementation (No Stdlib)
 */

#include "nz1_decompressor.h"

// Configuration
#define NZ_MAGIC 0x5A4E5A50
#define MAX_WINDOW ((1 << 16) - 1)
#define MIN_MATCH 3
#define MAX_MATCH 258
#define CRC32_POLY 0xEDB88320

// Internal: Memory copy for freestanding environment
static void z_memcpy(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  while (n--)
    *d++ = *s++;
}

// Internal: CRC32 calculation
static uint32_t nz_crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++)
      crc = (crc >> 1) ^ (CRC32_POLY & -(crc & 1));
  }
  return ~crc;
}

int nanozip_decompress(const void *src, int src_len, void *dst, int dst_cap) {
  const uint8_t *input = (const uint8_t *)src;
  uint8_t *output = (uint8_t *)dst;

  // Header check (14 bytes)
  if (src_len < 14)
    return -1;

  // Read header manually (avoiding potentially unaligned access issues with
  // direct casts if arch is strict) Magic Check
  uint32_t read_magic = ((uint32_t)input[0]) | ((uint32_t)input[1] << 8) |
                        ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);

  if (read_magic != NZ_MAGIC)
    return -1;

  uint32_t data_size = ((uint32_t)input[4]) | ((uint32_t)input[5] << 8) |
                       ((uint32_t)input[6] << 16) | ((uint32_t)input[7] << 24);

  uint32_t expected_crc = ((uint32_t)input[8]) | ((uint32_t)input[9] << 8) |
                          ((uint32_t)input[10] << 16) |
                          ((uint32_t)input[11] << 24);

  uint16_t window_size = ((uint16_t)input[12]) | ((uint16_t)input[13] << 8);

  // Validate
  if (!window_size || window_size > MAX_WINDOW)
    return -1;
  if ((int)data_size > dst_cap)
    return -1; // Not enough space

  const uint8_t *in_ptr = input + 14;
  size_t in_remain = (size_t)src_len - 14;
  size_t out_pos = 0;

  while (in_remain > 0 && out_pos < data_size) {
    if (*in_ptr == 0xBF) { // Escaped 0xBF or >= 0xC0
      if (in_remain < 2)
        break;  // Corrupt
      in_ptr++; // Skip BF
      in_remain--;
      output[out_pos++] = *in_ptr++;
      in_remain--;
    } else if (*in_ptr == 0xC0) { // Match
      if (in_remain < 4)
        break;

      // Format: C0 dist_low dist_high len-MIN_MATCH
      uint32_t dist = in_ptr[1] | ((uint32_t)in_ptr[2] << 8);
      uint32_t len = in_ptr[3] + MIN_MATCH;

      if (len > MAX_MATCH)
        break;

      // Boundary checks
      if (dist == 0 || dist > out_pos || out_pos + len > data_size)
        break;

      // Copy match (overlapping possible, so replicate logic essentially like
      // memcpy but with history) Since we write to output, we can read from it.
      // Standard memcpy for overlapping regions (src < dst) is undefined, but
      // here we are reading 'backwards' so it's a history copy. 'z_memcpy'
      // loops forward byte by byte which IS safe and correct for LZ history
      // copying where dist < len (RLE-like effect). Example: dist=1, len=5.
      // output has "A". out_pos=1. i=0: output[1] = output[1-1] -> "AA" i=1:
      // output[2] = output[2-1] -> "AAA" ... So byte-by-byte loop is REQUIRED
      // here.

      const uint8_t *match_src = output + out_pos - dist;
      for (uint32_t i = 0; i < len; i++) {
        output[out_pos++] = *match_src++;
      }

      in_ptr += 4;
      in_remain -= 4;
    } else { // Literal
      output[out_pos++] = *in_ptr++;
      in_remain--;
    }
  }

  if (out_pos != data_size)
    return -1;

  (void)expected_crc;

  return (int)data_size;
}
