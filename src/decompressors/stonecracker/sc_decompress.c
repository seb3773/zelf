#include "sc_decompress.h"

// Minimal binary size implementation
// No libc dependencies (except implicit ones if compiler generates them, but we
// avoid calls) Single function for better optimization

// Helper macro: refill 16 bits into the buffer
uint32_t sc_get_raw_size(const uint8_t *input, size_t input_size) {
  if (input_size < 16)
    return 0;
  return ((uint32_t)input[8] << 24) | ((uint32_t)input[9] << 16) |
         ((uint32_t)input[10] << 8) | input[11];
}

size_t sc_decompress(const uint8_t *input, size_t input_size, uint8_t *output,
                     size_t output_capacity) {
  // Header check: "ZULU" or "S403"
  // 0x5A554C55 or 0x53343033
  uint32_t sig = ((uint32_t)input[0] << 24) | ((uint32_t)input[1] << 16) |
                 ((uint32_t)input[2] << 8) | input[3];
  if (sig != 0x5A554C55 && sig != 0x53343033)
    return 0;

  if (input_size < 18)
    return 0;

  uint32_t raw_size = sc_get_raw_size(input, input_size);
  if (raw_size > output_capacity)
    return 0;

  // Packed size field at offset 12
  uint32_t packed_field = ((uint32_t)input[12] << 24) |
                          ((uint32_t)input[13] << 16) |
                          ((uint32_t)input[14] << 8) | input[15];
  size_t data_end = 16 + packed_field + 2;
  if (data_end > input_size)
    return 0;

  // BitReader state
  const uint8_t *ptr = input + data_end; // Points just AFTER the last byte
  uint32_t buffer = 0;
  int bits_in_buffer = 0;

  // Initial setup: Read bit count (last 2 bytes)
  // ptr points to end. ptr[-2] is high byte of bit count.
  uint16_t bit_count = ((uint16_t)ptr[-2] << 8) | ptr[-1];
  ptr -= 2;

  if (bit_count > 16)
    return 0;

  // Read first word
  if (ptr < input + 16 + 2)
    return 0; // Bounds check
  uint16_t initial_word = ((uint16_t)ptr[-2] << 8) | ptr[-1];
  ptr -= 2;

  // Swap based on ancient analysis:
  // ancient reads Last 2 bytes (bit_count in my code) as VALUE (buffer).
  // ancient reads Prev 2 bytes (initial_word in my code) as BITCOUNT.
  buffer = initial_word;
  bits_in_buffer = bit_count; // Only keep valid bits?
  // The decompressor logic says: "bitReader.reset(value, bitCount)".
  // If we just treat it as having `bit_count` bits, we are good.
  // But `buffer` contains 16 bits. We should mask it?
  // If `bit_count` is e.g. 3, it means only the 3 LSBs are valid.
  // So yes, mask it.
  buffer &= (1UL << bit_count) - 1;

  size_t out_pos = raw_size;

  // Helper macro: refill 16 bits into the buffer
#define REFILL_WORD()                                                           \
  do {                                                                          \
    uint16_t w = ((uint16_t)ptr[-2] << 8) | ptr[-1];                            \
    ptr -= 2;                                                                   \
    buffer |= ((uint32_t)w) << bits_in_buffer;                                  \
    bits_in_buffer += 16;                                                       \
  } while (0)

  while (out_pos > 0) {
    // REFILL
    while (bits_in_buffer <= 16) {
      if (ptr > input + 17) {
        REFILL_WORD();
      } else {
        break;
      }
    }

    int is_match = buffer & 1;
    buffer >>= 1;
    bits_in_buffer--;

    if (!is_match) {
      // Literal
      if (bits_in_buffer < 8) {
        if (ptr > input + 17) {
          REFILL_WORD();
        }
      }
      uint8_t lit = buffer & 0xFF;
      buffer >>= 8;
      bits_in_buffer -= 8;
      output[--out_pos] = lit;
    } else {
      // Match
      if (bits_in_buffer < 2) {
        if (ptr > input + 17) {
          REFILL_WORD();
        }
      }
      int index = buffer & 3;
      buffer >>= 2;
      bits_in_buffer -= 2;

      int dist_bits;
      uint32_t dist_base;
      switch (index) {
      case 0:
        dist_bits = 5;
        dist_base = 0;
        break;
      case 1:
        dist_bits = 8;
        dist_base = 32;
        break;
      case 2:
        dist_bits = 10;
        dist_base = 288;
        break;
      case 3:
        dist_bits = 12;
        dist_base = 1312;
        break;
      }

      while (bits_in_buffer < dist_bits) {
        if (ptr > input + 17) {
          REFILL_WORD();
        } else
          break;
      }
      uint32_t dist_val = buffer & ((1 << dist_bits) - 1);
      buffer >>= dist_bits;
      bits_in_buffer -= dist_bits;

      uint32_t distance = dist_base + dist_val + 1;

      uint32_t count = 2;
#define READ_BIT(var)                                                          \
  if (bits_in_buffer == 0 && ptr > input + 17) {                               \
    REFILL_WORD();                                                             \
  }                                                                            \
  var = buffer & 1;                                                            \
  buffer >>= 1;                                                                \
  bits_in_buffer--;

      int bit;
      READ_BIT(bit);
      if (!bit) {
        count++;
        READ_BIT(bit);
        if (!bit) {
          count++;
          READ_BIT(bit);
          if (!bit) {
            count++;
            while (1) {
              while (bits_in_buffer < 3) {
                if (ptr > input + 17) {
                  REFILL_WORD();
                } else
                  break;
              }
              uint32_t tmp = buffer & 7;
              buffer >>= 3;
              bits_in_buffer -= 3;
              count += tmp;
              if (tmp != 7)
                break;
            }
          }
        }
      }
#undef READ_BIT

      if (count > out_pos)
        count = out_pos;

      for (uint32_t i = 0; i < count; i++) {
        size_t dst_idx = out_pos - 1 - i;
        size_t src_idx = dst_idx + distance;
        if (src_idx <
            output_capacity) { // Use output_capacity which is raw_size
          output[dst_idx] = output[src_idx];
        } else {
          output[dst_idx] = 0;
        }
      }
      out_pos -= count;
    }
  }

#undef REFILL_WORD
  return raw_size;
}
