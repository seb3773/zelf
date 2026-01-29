// lzhamd_symbol_codec.h - Pure C bitreader + arithmetic/adaptive bit model
// Minimal, C99. No C++.
#ifndef LZHAMD_SYMBOL_CODEC_H
#define LZHAMD_SYMBOL_CODEC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Probability scale (must match upstream semantics)
enum { LZHAMD_ARITH_PROB_BITS = 11 };
enum { LZHAMD_ARITH_PROB_SCALE = 1 << LZHAMD_ARITH_PROB_BITS };
enum { LZHAMD_ARITH_PROB_HALF = 1 << (LZHAMD_ARITH_PROB_BITS - 1) };

typedef struct {
    const uint8_t* buf;      // start
    const uint8_t* next;     // cursor
    const uint8_t* end;      // end
    uint64_t bitbuf;         // MSB-first bit buffer
    int bitcnt;              // number of valid bits in bitbuf
    int eof;                 // non-zero if no more bytes will arrive
} lzhamd_bitreader_t;

void lzhamd_br_init(lzhamd_bitreader_t* br, const uint8_t* buf, size_t size, int eof);
void lzhamd_br_set(lzhamd_bitreader_t* br, const uint8_t* buf, size_t size, const uint8_t* next, int eof);
uint32_t lzhamd_br_get_bits(lzhamd_bitreader_t* br, uint32_t n); // n in [0..24]
void lzhamd_br_align_to_byte(lzhamd_bitreader_t* br);
int lzhamd_br_remove_byte(lzhamd_bitreader_t* br); // returns -1 if not enough bits
uint32_t lzhamd_br_peek_bits(lzhamd_bitreader_t* br, uint32_t n); // does not consume
void lzhamd_br_drop_bits(lzhamd_bitreader_t* br, uint32_t n);

// Adaptive bit model (0-probability stored)
typedef struct {
    uint16_t p0; // in [1..LZHAMD_ARITH_PROB_SCALE-1]
} lzhamd_adapt_bit_model_t;

static inline void lzhamd_abm_init(lzhamd_adapt_bit_model_t* m) { m->p0 = (uint16_t)LZHAMD_ARITH_PROB_HALF; }
static inline void lzhamd_abm_update(lzhamd_adapt_bit_model_t* m, uint32_t bit) {
    if (!bit) m->p0 += (uint16_t)((LZHAMD_ARITH_PROB_SCALE - m->p0) >> 5);
    else      m->p0 -= (uint16_t)(m->p0 >> 5);
    if (m->p0 < 1) m->p0 = 1; if (m->p0 >= LZHAMD_ARITH_PROB_SCALE) m->p0 = LZHAMD_ARITH_PROB_SCALE - 1;
}

// Arithmetic decoder state
typedef struct {
    uint32_t value;
    uint32_t length;
    lzhamd_bitreader_t* br;
} lzhamd_arithdec_t;

void lzhamd_ad_init(lzhamd_arithdec_t* ad, lzhamd_bitreader_t* br);
uint32_t lzhamd_ad_get_bit(lzhamd_arithdec_t* ad, lzhamd_adapt_bit_model_t* m); // returns 0/1 and updates model

#ifdef __cplusplus
}
#endif

#endif // LZHAMD_SYMBOL_CODEC_H
