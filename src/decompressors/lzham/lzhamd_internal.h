// lzhamd_internal.h - Internal C helpers for pure C LZHAM decompressor
// Comments: English-only. No runtime logs (commented where present). No C++
// usage. Target: Linux x86_64. C99.

#ifndef LZHAMD_INTERNAL_H
#define LZHAMD_INTERNAL_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__)
#define LZHAMD_LIKELY(x) __builtin_expect(!!(x), 1)
#define LZHAMD_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LZHAMD_LIKELY(x) (x)
#define LZHAMD_UNLIKELY(x) (x)
#endif

#ifndef LZHAM_ASSERT
#include <assert.h>
#define LZHAM_ASSERT(x) assert(x)
#endif

#define LZHAM_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define LZHAM_MAX(a, b) (((a) > (b)) ? (a) : (b))

static inline uint32_t lzhamd_read_be_u32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// ---- LZ base constants copied from lzham_lzdecompbase.h/cpp (frozen)

enum {
  LZHAMD_C_MIN_MATCH_LEN = 2U,
  LZHAMD_C_MAX_MATCH_LEN = 257U,
  LZHAMD_C_MAX_HUGE_MATCH_LEN = 65536U
};

// Dict size bounds (x64)
enum { LZHAMD_C_MIN_DICT_SIZE_LOG2 = 15, LZHAMD_C_MAX_DICT_SIZE_LOG2 = 29 };

enum { LZHAMD_C_NUM_LIT_STATES = 7, LZHAMD_C_NUM_STATES = 12 };

enum {
  LZHAMD_C_BLOCK_HEADER_BITS = 2,
  LZHAMD_C_BLOCK_FLUSH_TYPE_BITS = 2,
  LZHAMD_C_SYNC_BLOCK = 0,
  LZHAMD_C_COMP_BLOCK = 1,
  LZHAMD_C_RAW_BLOCK = 2,
  LZHAMD_C_EOF_BLOCK = 3
};

enum {
  LZHAMD_C_LZX_NUM_SECONDARY_LENGTHS = 249,
  LZHAMD_C_NUM_HUGE_MATCH_CODES = 1,
  LZHAMD_C_LZX_NUM_SPECIAL_LENGTHS = 2,
  LZHAMD_C_LZX_LOWEST_USABLE_MATCH_SLOT = 1,
  LZHAMD_C_LZX_MAX_POSITION_SLOTS = 128
};

// Special codes used in main table
enum {
  LZHAMD_C_LZX_SPECIAL_END_OF_BLOCK_CODE = 0,
  LZHAMD_C_LZX_SPECIAL_PARTIAL_STATE_RESET = 1
};

// Table update settings (copied from lzham_lzdecompbase.cpp)
typedef struct lzhamd_table_update_settings_s {
  uint16_t max_update_interval;
  uint16_t slow_rate;
} lzhamd_table_update_settings_t;

static const lzhamd_table_update_settings_t g_lzhamd_table_update_settings[] = {
    {4, 32},     {5, 33},     {6, 34},     {7, 35},     {8, 36},
    {16, 48},    {32, 72},    {64, 64},    {98, 80},    {128, 96},
    {192, 112},  {256, 128},  {512, 160},  {1024, 192}, {2048, 224},
    {2048, 256}, {2048, 288}, {2048, 320}, {2048, 352}, {2048, 384}};

// LZX position tables (copied from lzham_lzdecompbase.cpp)
extern const uint32_t
    g_lzhamd_lzx_position_base[LZHAMD_C_LZX_MAX_POSITION_SLOTS];
extern const uint32_t
    g_lzhamd_lzx_position_extra_mask[LZHAMD_C_LZX_MAX_POSITION_SLOTS];
extern const uint8_t
    g_lzhamd_lzx_position_extra_bits[LZHAMD_C_LZX_MAX_POSITION_SLOTS];

// Number of position slots per dict_size_log2 (index: dict_log2 - 15)
static inline uint8_t lzhamd_num_lzx_position_slots(uint32_t dict_log2) {
  static const uint8_t k_num[15] = {30, 32, 34, 36, 38, 40, 42, 44,
                                    46, 48, 50, 52, 54, 58, 66};
  if (dict_log2 < LZHAMD_C_MIN_DICT_SIZE_LOG2 ||
      dict_log2 > LZHAMD_C_MAX_DICT_SIZE_LOG2)
    return 0;
  return k_num[dict_log2 - LZHAMD_C_MIN_DICT_SIZE_LOG2];
}

#endif // LZHAMD_INTERNAL_H
