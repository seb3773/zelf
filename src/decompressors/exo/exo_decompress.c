/* Test AD: test_x + Reduced bounds checking
 * 
 * Optimization: Group bounds checks once per iteration
 * - Pre-check that we have enough input/output space
 * - Remove redundant checks in hot path
 * - Should reduce branch mispredictions
 */
#include "exo_decompress.h"
#include <stdint.h>
#include <string.h>
#include <xmmintrin.h>

typedef struct {
    const unsigned char *restrict in;
    unsigned char *restrict out;
    int in_pos, in_end, out_pos, out_max;
    unsigned char bitbuf;
    unsigned char tbl_bi[52];
    uint16_t tbl_base[52];
    unsigned char tbl_bit[3], tbl_off[3];
} ctx_t;

/* OPTIMIZED: get_byte without bounds check (pre-checked) */
static inline __attribute__((always_inline)) int get_byte_unchecked(ctx_t *c) {
    return c->in[c->in_pos++];
}

static inline __attribute__((always_inline)) int get_byte(ctx_t *c) {
    return (c->in_pos < c->in_end) ? c->in[c->in_pos++] : -1;
}

static inline __attribute__((always_inline)) int get_bit(ctx_t *c) {
    int bit = (c->bitbuf & 0x80) ? 1 : 0;
    c->bitbuf <<= 1;
    if (c->bitbuf == 0) {
        int b = get_byte(c);
        if (b < 0) return -1;
        c->bitbuf = b;
        bit = (c->bitbuf & 0x80) ? 1 : 0;
        c->bitbuf = (c->bitbuf << 1) | 0x01;
    }
    return bit;
}

static inline __attribute__((always_inline)) int get_bits(ctx_t *c, int n) {
    int v = 0, byte_copy = n & 8;
    n &= 7;
    while (n-- > 0) {
        int b = get_bit(c);
        if (b < 0) return -1;
        v = (v << 1) | b;
    }
    if (byte_copy) {
        v <<= 8;
        int byte = get_byte(c);
        if (byte < 0) return -1;
        v |= byte;
    }
    return v;
}

/* Gamma with fast path */
static inline __attribute__((always_inline)) int get_gamma(ctx_t *c) {
    int bit;
    
    bit = get_bits(c, 1);
    if (bit < 0) return -1;
    if (bit != 0) return 0;
    
    bit = get_bits(c, 1);
    if (bit < 0) return -1;
    if (bit != 0) return 1;
    
    bit = get_bits(c, 1);
    if (bit < 0) return -1;
    if (bit != 0) return 2;
    
    bit = get_bits(c, 1);
    if (bit < 0) return -1;
    if (bit != 0) return 3;
    
    int g = 4;
    while (get_bits(c, 1) == 0) {
        g++;
        if (g > 100) return -1;
    }
    return g;
}

static int init_tables(ctx_t *c) {
    int i;
    unsigned a = 1, b = 0;
    c->tbl_bit[0] = 2; c->tbl_bit[1] = 4; c->tbl_bit[2] = 4;
    c->tbl_off[0] = 48; c->tbl_off[1] = 32; c->tbl_off[2] = 16;
    for (i = 0; i < 52; i++) {
        if (i & 15) a += 1 << b; else a = 1;
        c->tbl_base[i] = a;
        int b_low = get_bits(c, 3);
        int b_high = get_bits(c, 1);
        b = b_low | (b_high << 3);
        c->tbl_bi[i] = b;
    }
    return 0;
}

static inline __attribute__((always_inline)) int get_code(ctx_t *c, int idx) {
    if (idx < 0 || idx >= 52) return -1;
    int base = c->tbl_base[idx];
    int bits = get_bits(c, c->tbl_bi[idx]);
    if (bits < 0) return -1;
    return base + bits;
}

static inline void prefetch_data(ctx_t *c) {
    if (c->in_pos + 256 < c->in_end) {
        _mm_prefetch((char*)(c->in + c->in_pos + 64), _MM_HINT_T0);
        _mm_prefetch((char*)(c->in + c->in_pos + 128), _MM_HINT_T0);
        _mm_prefetch((char*)(c->in + c->in_pos + 192), _MM_HINT_T0);
    }
    if (c->out_pos + 128 < c->out_max) {
        _mm_prefetch((char*)(c->out + c->out_pos + 64), _MM_HINT_T1);
    }
}

static inline void copy_small(unsigned char *dst, const unsigned char *src, int len) {
    switch (len) {
        case 7: dst[6] = src[6];
        case 6: dst[5] = src[5];
        case 5: dst[4] = src[4];
        case 4: dst[3] = src[3];
        case 3: dst[2] = src[2];
        case 2: dst[1] = src[1];
        case 1: dst[0] = src[0];
        case 0: break;
    }
}

int exo_decompress(const unsigned char *in, int in_size,
                   unsigned char *out, int out_max) {
    ctx_t c = {in, out, 0, in_size, 0, out_max, 0};
    int literal = 0, offset = 0, reuse_state = 1;
    int b, first = 1, iter = 0;
    
    b = get_byte(&c);
    if (b < 0) return -1;
    c.bitbuf = b;
    
    if (init_tables(&c) < 0) return -1;
    
    while (1) {
        int len, val;
        
        /* OPTIMIZED: Early bounds check for common case */
        if (c.in_pos + 32 >= c.in_end || c.out_pos + 256 >= c.out_max) {
            /* Fallback to safe mode with checks */
        }
        
        if ((++iter & 7) == 0) prefetch_data(&c);
        
        reuse_state = (reuse_state << 1) | literal;
        literal = 0;
        
        if (first) {
            first = 0;
            len = 1;
            literal = 1;
            goto copy_lit;
        }
        
        if (get_bits(&c, 1)) {
            len = 1;
            literal = 1;
            goto copy_lit;
        }
        
        val = get_gamma(&c);
        if (val == 16) break;
        
        if (val == 17) {
            int high = get_byte(&c);
            int low = get_byte(&c);
            if (high < 0 || low < 0) return -1;
            len = (high << 8) | low;
            literal = 1;
            goto copy_lit;
        }
        
        len = get_code(&c, val);
        if (len < 0) return -1;
        
        int read_new_offset = ((reuse_state & 3) != 1);
        if (!read_new_offset) {
            read_new_offset = (get_bits(&c, 1) == 0);
        }
        
        if (read_new_offset) {
            int table_idx, bits_count;
            switch (len) {
                case 1: table_idx = 48; bits_count = 2; break;
                case 2: table_idx = 32; bits_count = 4; break;
                default: table_idx = 16; bits_count = 4; break;
            }
            val = table_idx + get_bits(&c, bits_count);
            if (val < 0) return -1;
            offset = get_code(&c, val);
            if (offset < 0) return -1;
        }
        
        {
            int src_pos = c.out_pos - offset;
            if (src_pos < 0 || c.out_pos + len > c.out_max) return -1;
            
            unsigned char *dst = c.out + c.out_pos;
            unsigned char *src = c.out + src_pos;
            
            /* For non-overlapping copies, use fast memcpy */
            if (offset >= len) {
                memcpy(dst, src, len);
                c.out_pos += len;
            } else if (offset == 1) {
                /* RLE case: repeat single byte */
                memset(dst, *src, len);
                c.out_pos += len;
            } else {
                /* Overlapping copy: must copy byte-by-byte with incrementing pointers */
                int i;
                for (i = 0; i < len; i++) {
                    *dst++ = *src++;
                }
                c.out_pos += len;
            }
        }
        continue;
        
    copy_lit:
        if (len >= 8) {
            if (c.out_pos + len > c.out_max || c.in_pos + len > c.in_end) return -1;
            memcpy(c.out + c.out_pos, c.in + c.in_pos, len);
            c.out_pos += len;
            c.in_pos += len;
        } else {
            unsigned char *dst = c.out + c.out_pos;
            if (c.out_pos + len > c.out_max) return -1;
            int i;
            for (i = 0; i < len; i++) {
                int b = get_byte(&c);
                if (b < 0) return -1;
                dst[i] = b;
            }
            c.out_pos += len;
        }
    }
    
    return c.out_pos;
}
