// lzhamd_symbol_codec.c - Pure C bitreader + arithmetic decoder
// C99, Linux x86_64

#include "lzhamd_symbol_codec.h"
#include <string.h>

static inline uint32_t lzhamd_min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

void lzhamd_br_init(lzhamd_bitreader_t* br, const uint8_t* buf, size_t size, int eof) {
    if (!br) return;
    br->buf = buf; br->next = buf; br->end = buf ? (buf + size) : 0; br->bitbuf = 0; br->bitcnt = 0; br->eof = eof ? 1 : 0;
}

void lzhamd_br_set(lzhamd_bitreader_t* br, const uint8_t* buf, size_t size, const uint8_t* next, int eof) {
    if (!br) return;
    br->buf = buf; br->end = buf ? (buf + size) : 0; br->next = (next && next >= buf && next <= br->end) ? next : buf; br->eof = eof ? 1 : 0;
}

// Refill helper
static inline void lzhamd_br_refill(lzhamd_bitreader_t* br, int want_bits) {
    while (br->bitcnt < want_bits) {
        uint32_t byte = 0;
        if (br->next == br->end) {
            if (!br->eof) break; // caller must supply more data later
        } else {
            byte = *br->next++;
        }
        br->bitbuf |= ((uint64_t)byte) << (64 - 8 - br->bitcnt);
        br->bitcnt += 8;
    }
}

uint32_t lzhamd_br_get_bits(lzhamd_bitreader_t* br, uint32_t n) {
    if (!n) return 0U;
    if (n > 24U) n = 24U;
    lzhamd_br_refill(br, (int)n);
    uint32_t v = (uint32_t)(br->bitbuf >> (64 - n));
    br->bitbuf <<= n;
    br->bitcnt -= (int)n;
    return v;
}

void lzhamd_br_align_to_byte(lzhamd_bitreader_t* br) {
    int r = br->bitcnt & 7; if (r) { (void)lzhamd_br_get_bits(br, (uint32_t)r); }
}

int lzhamd_br_remove_byte(lzhamd_bitreader_t* br) {
    if (br->bitcnt < 8) return -1;
    int v = (int)(br->bitbuf >> (64 - 8));
    br->bitbuf <<= 8; br->bitcnt -= 8; return v;
}

uint32_t lzhamd_br_peek_bits(lzhamd_bitreader_t* br, uint32_t n) {
    if (!n) return 0U;
    if (n > 24U) n = 24U;
    lzhamd_br_refill(br, (int)n);
    return (uint32_t)(br->bitbuf >> (64 - n));
}

void lzhamd_br_drop_bits(lzhamd_bitreader_t* br, uint32_t n) {
    if (!n) return;
    if (n > (uint32_t)br->bitcnt) n = (uint32_t)br->bitcnt;
    br->bitbuf <<= n;
    br->bitcnt -= (int)n;
}

void lzhamd_ad_init(lzhamd_arithdec_t* ad, lzhamd_bitreader_t* br) {
    if (!ad) return; ad->br = br; ad->value = 0; ad->length = 0;
    // Seed with 4 bytes (matches LZHAM_SYMBOL_CODEC_DECODE_ARITH_START semantics)
    for (int i = 0; i < 4; ++i) {
        uint32_t b = lzhamd_br_get_bits(br, 8);
        ad->value = (ad->value << 8) | b;
    }
    // Upstream sets arith_length to cSymbolCodecArithMaxLen to avoid immediate renormalization.
    ad->length = 0xFFFFFFFFU;
}

uint32_t lzhamd_ad_get_bit(lzhamd_arithdec_t* ad, lzhamd_adapt_bit_model_t* m) {
    // Renormalize
    while (ad->length < 0x01000000U) {
        uint32_t b = lzhamd_br_get_bits(ad->br, 8);
        ad->value = (ad->value << 8) | b;
        ad->length <<= 8;
    }
    uint32_t x = (uint32_t)m->p0 * (ad->length >> LZHAMD_ARITH_PROB_BITS);
    uint32_t bit = (ad->value >= x);
    if (!bit) {
        m->p0 += (uint16_t)((LZHAMD_ARITH_PROB_SCALE - m->p0) >> 5);
        ad->length = x;
    } else {
        m->p0 -= (uint16_t)(m->p0 >> 5);
        ad->value -= x;
        ad->length -= x;
    }
    if (m->p0 < 1) m->p0 = 1; if (m->p0 >= LZHAMD_ARITH_PROB_SCALE) m->p0 = LZHAMD_ARITH_PROB_SCALE - 1;
    return bit;
}
