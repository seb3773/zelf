/* ZX0 forward-buffer decompressor for stubs (no malloc) */
#include <stddef.h>
#include "zx0_decompress.h"

#define INITIAL_OFFSET 1

typedef struct {
    const unsigned char *src;
    const unsigned char *src_end;
    int bit_mask;
    int bit_value;
    int backtrack;
    int last_byte;
} BitReader;

static inline int rd_byte(BitReader *br) {
    if (br->src >= br->src_end) return -1;
    br->last_byte = *br->src++;
    return br->last_byte;
}

static inline int rd_bit(BitReader *br) {
    if (br->backtrack) {
        br->backtrack = 0;
        return br->last_byte & 1;
    }
    br->bit_mask >>= 1;
    if (!br->bit_mask) {
        br->bit_mask = 128;
        br->bit_value = rd_byte(br);
    }
    return (br->bit_value & br->bit_mask) ? 1 : 0;
}

static inline int rd_elias_gamma(BitReader *br, int inverted) {
    int value = 1;
    while (!rd_bit(br)) {
        int b = rd_bit(br);
        value = (value << 1) | (b ^ inverted);
    }
    return value;
}

int zx0_decompress_to(const unsigned char *in, int in_size,
                      unsigned char *out, int out_max)
{
    if (!in || in_size <= 0 || !out || out_max <= 0) return -1;

    BitReader br;
    br.src = in;
    br.src_end = in + (size_t)in_size;
    br.bit_mask = 0;
    br.bit_value = 0;
    br.backtrack = 0;
    br.last_byte = 0;

    int last_offset = INITIAL_OFFSET;
    int out_pos = 0;

    for (;;) {
        /* COPY_LITERALS */
        int length = rd_elias_gamma(&br, 0);
        for (int i = 0; i < length; i++) {
            int c = rd_byte(&br);
            if (c < 0) return -1;
            if (out_pos >= out_max) return -1;
            out[out_pos++] = (unsigned char)c;
        }
        if (rd_bit(&br)) {
            /* COPY_FROM_NEW_OFFSET */
            for (;;) {
                last_offset = rd_elias_gamma(&br, 1);
                if (last_offset == 256) {
                    return out_pos; /* EOF */
                }
                {
                    int t = rd_byte(&br);
                    if (t < 0) return -1;
                    last_offset = last_offset * 128 - (t >> 1);
                }
                br.backtrack = 1;
                length = rd_elias_gamma(&br, 0) + 1;
                for (int i = 0; i < length; i++) {
                    int src_pos = out_pos - last_offset;
                    if (src_pos < 0 || out_pos >= out_max) return -1;
                    out[out_pos++] = out[src_pos];
                }
                if (!rd_bit(&br)) break; /* back to COPY_LITERALS */
            }
        } else {
            /* COPY_FROM_LAST_OFFSET */
            length = rd_elias_gamma(&br, 0);
            for (int i = 0; i < length; i++) {
                int src_pos = out_pos - last_offset;
                if (src_pos < 0 || out_pos >= out_max) return -1;
                out[out_pos++] = out[src_pos];
            }
            if (!rd_bit(&br)) {
                /* continue outer for to COPY_LITERALS */
                continue;
            } else {
                /* fallthrough to COPY_FROM_NEW_OFFSET loop */
                for (;;) {
                    last_offset = rd_elias_gamma(&br, 1);
                    if (last_offset == 256) {
                        return out_pos; /* EOF */
                    }
                    {
                        int t = rd_byte(&br);
                        if (t < 0) return -1;
                        last_offset = last_offset * 128 - (t >> 1);
                    }
                    br.backtrack = 1;
                    int len2 = rd_elias_gamma(&br, 0) + 1;
                    for (int i = 0; i < len2; i++) {
                        int src_pos = out_pos - last_offset;
                        if (src_pos < 0 || out_pos >= out_max) return -1;
                        out[out_pos++] = out[src_pos];
                    }
                    if (!rd_bit(&br)) break;
                }
            }
        }
    }
}
