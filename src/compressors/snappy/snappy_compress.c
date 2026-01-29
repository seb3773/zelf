/*
 * Minimal Snappy compressor in pure C (Linux x86_64)
 *
 * Emits a valid Snappy stream:
 *   varint(uncompressed_length) + sequence of literals/copies
 * Hash-based greedy matching, offsets up to 65535 (copy-2), fallback to copy-4 if needed.
 * For simplicity, uses a 32KB hash table of last positions. Good speed, reasonable ratio.
 *
 * Safety: validates pointers, bounds check on all emissions.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "snappy_compress.h"

/* Optional global progress callback */
static snappy_progress_fn g_snappy_progress_cb = NULL;
void snappy_set_progress_cb(snappy_progress_fn cb) {
    g_snappy_progress_cb = cb;
}

/* Internal helpers */
static inline uint32_t load32_le(const unsigned char *p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline size_t varint_write(uint32_t value, unsigned char *dst) {
    size_t n = 0;
    while (value >= 0x80) {
        dst[n++] = (unsigned char)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    dst[n++] = (unsigned char)value;
    return n;
}

static inline size_t emit_literal(unsigned char *dst, const unsigned char *lit, size_t len) {
    size_t n = 0;
    if (len < 60) {
        dst[n++] = (unsigned char)((len - 1) << 2);
    } else {
        /* long literal: encode len-1 in 1..4 bytes little-endian */
        uint32_t l = (uint32_t)(len - 1);
        unsigned char tmp[4];
        int lb = 0;
        while (l) { tmp[lb++] = (unsigned char)(l & 0xFF); l >>= 8; }
        if (lb == 0) { tmp[lb++] = 0; }
        dst[n++] = (unsigned char)((59 + lb) << 2);
        for (int i = 0; i < lb; ++i) dst[n++] = tmp[i];
    }
    memcpy(dst + n, lit, len);
    n += len;
    return n;
}

static inline size_t emit_copy1(unsigned char *dst, size_t len, size_t offset) {
    /* copy-1: tag = 01 | ((len-4)<<2) | ((offset>>8)<<5), then 1 byte low offset */
    size_t n = 0;
    size_t len_field = len - 4; /* 0..7 => len 4..11 */
    dst[n++] = (unsigned char)(0x01 | ((len_field & 0x7) << 2) | ((offset >> 8) << 5));
    dst[n++] = (unsigned char)(offset & 0xFF);
    return n;
}

static inline size_t emit_copy2(unsigned char *dst, size_t len, size_t offset) {
    /* copy-2: tag = 10 | ((len-1)<<2), then 2 bytes LE offset */
    size_t n = 0;
    size_t len_field = len - 1; /* 1..64 => field 0..63 */
    dst[n++] = (unsigned char)(0x02 | ((len_field & 0x3F) << 2));
    dst[n++] = (unsigned char)(offset & 0xFF);
    dst[n++] = (unsigned char)((offset >> 8) & 0xFF);
    return n;
}

static inline size_t emit_copy4(unsigned char *dst, size_t len, size_t offset) {
    /* copy-4: tag = 11 | ((len-1)<<2), then 4 bytes LE offset */
    size_t n = 0;
    size_t len_field = len - 1;
    dst[n++] = (unsigned char)(0x03 | ((len_field & 0x3F) << 2));
    dst[n++] = (unsigned char)(offset & 0xFF);
    dst[n++] = (unsigned char)((offset >> 8) & 0xFF);
    dst[n++] = (unsigned char)((offset >> 16) & 0xFF);
    dst[n++] = (unsigned char)((offset >> 24) & 0xFF);
    return n;
}

int snappy_compress_c(const unsigned char *input, size_t input_size,
                      unsigned char **output, size_t *output_size)
{
    if (!input || !output || !output_size) return -1;

    /* Conservative bound from Snappy paper: 32 + src + src/6 */
    size_t bound = 32 + input_size + input_size / 6;
    if (bound < 64) bound = 64;
    unsigned char *out = (unsigned char*)malloc(bound);
    if (!out) return -1;

    size_t op = 0;
    /* Header: varint of uncompressed size */
    op += varint_write((uint32_t)input_size, out + op);

    const size_t HASH_BITS = 15;               /* 32K table */
    const size_t HASH_SIZE = (size_t)1 << HASH_BITS;
    const uint32_t HASH_MULT = 0x1e35a7bdU;   /* as in Snappy */

    int32_t *ht = (int32_t*)malloc(HASH_SIZE * sizeof(int32_t));
    if (!ht) { free(out); return -1; }
    for (size_t i = 0; i < HASH_SIZE; ++i) ht[i] = -0x40000000; /* far negative */

    size_t ip = 0;
    size_t lit_start = 0;
    size_t last_report = 0; /* progress report position */

    if (input_size < 4) {
        /* Emit one literal for small inputs */
        op += emit_literal(out + op, input, input_size);
        *output = out; *output_size = op;
        free(ht);
        return 0;
    }

    while (ip + 4 <= input_size) {
        uint32_t v = load32_le(input + ip);
        size_t h = (size_t)((v * HASH_MULT) >> (32 - HASH_BITS));
        int32_t prev = ht[h];
        ht[h] = (int32_t)ip;

        size_t best_len = 0;
        size_t best_off = 0;
        if (prev >= 0) {
            size_t off = ip - (size_t)prev;
            if (off > 0 && off <= 0xFFFFFFFFu) {
                /* Check match */
                size_t a = ip;
                size_t b = (size_t)prev;
                if (a < input_size && b < input_size && input[a] == input[b]) {
                    /* Extend */
                    size_t max = input_size - a;
                    size_t len = 0;
                    while (len < max && input[a + len] == input[b + len]) {
                        ++len;
                        if (len >= 64 + 4) break; /* limit to reduce worst-case */
                    }
                    if (len >= 4) { best_len = len; best_off = off; }
                }
            }
        }

        if (best_len >= 4) {
            /* Flush pending literal */
            size_t lit_len = ip - lit_start;
            if (lit_len > 0) {
                op += emit_literal(out + op, input + lit_start, lit_len);
            }
            /* Emit copies, possibly chunking >64 */
            size_t remain = best_len;
            size_t off = best_off;
            while (remain > 0) {
                size_t chunk = (remain > 64) ? 64 : remain;
                if (off <= 0x7FF && chunk >= 4 && chunk <= 11) {
                    op += emit_copy1(out + op, chunk, off);
                } else if (off <= 0xFFFF) {
                    op += emit_copy2(out + op, chunk, off);
                } else {
                    op += emit_copy4(out + op, chunk, off);
                }
                remain -= chunk;
            }
            ip += best_len;
            lit_start = ip;
            /* Re-prime the hash at the new position */
            if (ip + 4 <= input_size) {
                uint32_t v2 = load32_le(input + ip);
                size_t h2 = (size_t)((v2 * HASH_MULT) >> (32 - HASH_BITS));
                ht[h2] = (int32_t)ip;
            }
        } else {
            /* No match, step */
            ++ip;
        }

        /* Bound check on output; enlarge if needed */
        if (op + 128 > bound) {
            size_t new_bound = bound + (bound >> 1) + 1024;
            unsigned char *grown = (unsigned char*)realloc(out, new_bound);
            if (!grown) { free(ht); free(out); return -1; }
            out = grown; bound = new_bound;
        }

        /* Progress: report roughly every 64KB advanced, without heavy cost */
        if (g_snappy_progress_cb) {
            size_t processed = ip;
            if (processed - last_report >= (size_t)64 * 1024 || processed == input_size) {
                last_report = processed;
                g_snappy_progress_cb(processed, input_size);
            }
        }
    }

    /* Emit trailing literal */
    if (lit_start < input_size) {
        op += emit_literal(out + op, input + lit_start, input_size - lit_start);
    }

    if (g_snappy_progress_cb) {
        g_snappy_progress_cb(input_size, input_size);
    }

    *output = out;
    *output_size = op;
    free(ht);
    return 0;
}
