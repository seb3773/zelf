// Minimal NRV2B LE32 decompressor definitions (vendored from UCL, trimmed)
// Only the symbols and constants required by the stage-0 stub path.
#pragma once

#include <stdint.h>

typedef uint8_t  ucl_byte;
typedef uint32_t ucl_uint;
typedef uint32_t ucl_uint32;
typedef uint8_t *ucl_bytep;
typedef uint32_t *ucl_uintp;

#define UCL_UINT32_C(v) ((uint32_t)(v))

// Error codes (subset)
#define UCL_E_OK                 0
#define UCL_E_ERROR             -1
#define UCL_E_INPUT_OVERRUN    -201
#define UCL_E_OUTPUT_OVERRUN   -202
#define UCL_E_LOOKBEHIND_OVERRUN -203
#define UCL_E_INPUT_NOT_CONSUMED -205

static inline void ucl_unused_u(const void *p) { (void)p; }

int ucl_nrv2b_decompress_le32(const ucl_byte *src, ucl_uint src_len,
                              ucl_byte *dst, ucl_uintp dst_len);

int stage0_nrv2b_le32(const uint8_t *src, uint32_t src_len,
                      uint8_t *dst, uint32_t *dst_len);


