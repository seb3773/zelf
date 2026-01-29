/**
 * @file nz1_compressor.c
 * @brief NanoZip Compressor Implementation
 */

#include "nz1_compressor.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// =====================
// CONFIGURABLE SETTINGS
// =====================
#define NZ_MAGIC 0x5A4E5A50            // 'NZPZ'
#define MAX_WINDOW ((1 << 16) - 1)     // 65535 bytes max
#define MIN_WINDOW (1 << 10)           // 1KB min
#define DEFAULT_WINDOW ((1 << 16) - 1) // 65535 bytes
#define MAX_MATCH 258
#define MIN_MATCH 3
#define HASH_BITS 16
#define MATCH_SEARCH_LIMIT 256
#define CRC32_POLY 0xEDB88320

// =====================
// PLATFORM DETECTION
// =====================
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVX2__)
#include <immintrin.h>
#define SIMD_WIDTH 32
typedef __m256i simd_vec;
#define VEC_LOAD(a) _mm256_loadu_si256((const __m256i *)(a))
#define VEC_CMP(a, b) _mm256_cmpeq_epi8(a, b)
#define VEC_MOVEMASK(a) _mm256_movemask_epi8(a)
#elif defined(__aarch64__) || defined(__arm__)
#include <arm_neon.h>
#define SIMD_WIDTH 16
typedef uint8x16_t simd_vec;
#define VEC_LOAD(a) vld1q_u8(a)
#define VEC_CMP(a, b) vceqq_u8(a, b)
#define VEC_MOVEMASK(a)                                                        \
  ({                                                                           \
    uint32_t m = 0;                                                            \
    uint8_t temp[16];                                                          \
    vst1q_u8(temp, a);                                                         \
    for (int i = 0; i < 16; i++) {                                             \
      if (temp[i] == 0xFF)                                                     \
        m |= (1 << i);                                                         \
    }                                                                          \
    m;                                                                         \
  })
#else
#define SIMD_WIDTH 8
typedef union {
  uint8_t bytes[8];
  uint32_t words[2];
} simd_vec;
#define VEC_LOAD(a)                                                            \
  ({                                                                           \
    simd_vec v;                                                                \
    memcpy(v.bytes, a, SIMD_WIDTH);                                            \
    v;                                                                         \
  })
#define VEC_CMP(a, b)                                                          \
  ({                                                                           \
    simd_vec v;                                                                \
    for (int i = 0; i < SIMD_WIDTH; i++)                                       \
      v.bytes[i] = (a.bytes[i] == b.bytes[i]) ? 0xFF : 0;                      \
    v;                                                                         \
  })
#define VEC_MOVEMASK(a)                                                        \
  ({                                                                           \
    uint32_t m = 0;                                                            \
    for (int i = 0; i < SIMD_WIDTH; i++)                                       \
      m |= (a.bytes[i] & 0x80) ? (1 << i) : 0;                                 \
    m;                                                                         \
  })
#endif

// =====================
// CORE COMPRESSION STRUCTURE
// =====================
typedef struct {
  uint32_t *head;
  uint32_t *chain;
  size_t window_size;
} NZ_State;

// =====================
// UTILITY FUNCTIONS
// =====================

static void nz_init(NZ_State *state, size_t window_size) {
  if (window_size < MIN_WINDOW)
    window_size = DEFAULT_WINDOW;
  if (window_size > MAX_WINDOW)
    window_size = MAX_WINDOW;

  state->window_size = window_size;
  // calloc initializes memory to zero
  state->head = calloc(1 << HASH_BITS, sizeof(uint32_t));
  state->chain = calloc(window_size, sizeof(uint32_t));
}

static void nz_cleanup(NZ_State *state) {
  free(state->head);
  free(state->chain);
}

static uint32_t nz_crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++)
      crc = (crc >> 1) ^ (CRC32_POLY & -(crc & 1));
  }
  return ~crc;
}

// =====================
// SAFE MATCH FINDING
// =====================

static inline uint32_t find_match(const uint8_t *data, size_t pos, size_t end,
                                 NZ_State *state, uint32_t *out_match_pos) {
  if (pos + MIN_MATCH > end) {
    *out_match_pos = 0;
    return 0;
  }

  // Compute rolling hash
  uint32_t hash = (data[pos] << 16) | (data[pos + 1] << 8) | data[pos + 2];
  hash = (hash * 0x9E3779B1) >> (32 - HASH_BITS);

  uint32_t prev_head_value = state->head[hash];
  state->head[hash] = pos; // Update head

  uint32_t best_len = 0;
  uint32_t best_match_candidate = 0;

  uint32_t candidate = prev_head_value;

  for (int i = 0; i < MATCH_SEARCH_LIMIT && candidate; i++) {
    if (candidate == pos) {
      candidate = state->chain[candidate % state->window_size];
      continue;
    }

    size_t dist = pos - candidate;
    if (dist > state->window_size)
      break;

    size_t max_len = (end - pos) < MAX_MATCH ? end - pos : MAX_MATCH;
    uint32_t len = 0;

    // Vectorized comparison
    while (len + SIMD_WIDTH <= max_len) {
      simd_vec a = VEC_LOAD(data + pos + len);
      simd_vec b = VEC_LOAD(data + candidate + len);
      simd_vec cmp = VEC_CMP(a, b);
      uint32_t mask = VEC_MOVEMASK(cmp);

      if (mask != (uint32_t)((1ULL << SIMD_WIDTH) - 1)) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__GNUC__) ||             \
    defined(__clang__)
        len += __builtin_ctz(~mask);
#else
        for (int j = 0; j < SIMD_WIDTH; ++j) {
          if (!((mask >> j) & 1)) {
            len += j;
            break;
          }
        }
#endif
        break;
      }
      len += SIMD_WIDTH;
    }

    // Scalar tail comparison
    while (len < max_len && data[pos + len] == data[candidate + len])
      len++;

    if (len > best_len && len >= MIN_MATCH) {
      best_len = len;
      best_match_candidate = candidate;
      if (len >= MAX_MATCH)
        break;
    }

    candidate = state->chain[candidate % state->window_size];
  }

  state->chain[pos % state->window_size] = prev_head_value;

  *out_match_pos = best_match_candidate;
  return best_len;
}

// =====================
// CORE COMPRESSION API
// =====================

size_t nanozip_compress(const uint8_t *input, size_t in_size, uint8_t *output,
                        size_t out_size, int window_size) {
  // Header: MAGIC(4) | ORIGINAL_SIZE(4) | CRC(4) | WINDOW_SIZE(2) = 14 bytes
  if (out_size < in_size + 14)
    return 0;

  NZ_State state;
  nz_init(&state, window_size);

  uint32_t magic_val = NZ_MAGIC;
  memcpy(output + 0, &magic_val, sizeof(magic_val));
  uint32_t in_size_u32 = (uint32_t)in_size;
  memcpy(output + 4, &in_size_u32, sizeof(uint32_t));
  uint32_t calculated_crc = nz_crc32(input, in_size);
  memcpy(output + 8, &calculated_crc, sizeof(calculated_crc));
  uint16_t window_s_val = (uint16_t)state.window_size;
  memcpy(output + 12, &window_s_val, sizeof(window_s_val));

  uint8_t *out_ptr = output + 14;

  size_t pos = 0;
  for (; pos < in_size;) {
    uint32_t actual_match_pos = 0;
    uint32_t match_len = find_match(input, pos, in_size, &state, &actual_match_pos);

    if (match_len >= MIN_MATCH) {
      uint32_t dist = pos - actual_match_pos;

      if (out_ptr - output > (ptrdiff_t)out_size - 4)
        goto out_fail;

      *out_ptr++ = 0xC0;
      *out_ptr++ = (uint8_t)dist;
      *out_ptr++ = (uint8_t)(dist >> 8);
      *out_ptr++ = (uint8_t)(match_len - MIN_MATCH);
      pos += match_len;
    } else {
      if (input[pos] == 0xBF) {
        if (out_ptr - output >= (ptrdiff_t)out_size - 2)
          goto out_fail;
        *out_ptr++ = 0xBF;
        *out_ptr++ = 0xBF;
        pos++;
      } else if (input[pos] >= 0xC0) {
        if (out_ptr - output >= (ptrdiff_t)out_size - 2)
          goto out_fail;
        *out_ptr++ = 0xBF;
        *out_ptr++ = input[pos++];
      } else {
        if (out_ptr - output >= (ptrdiff_t)out_size - 1)
          goto out_fail;
        *out_ptr++ = input[pos++];
      }
    }
  }

  nz_cleanup(&state);
  return (pos == in_size) ? (size_t)(out_ptr - output) : 0;

out_fail:
  nz_cleanup(&state);
  return 0;
}
