#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>

// Internal in-memory PowerPacker compressor (PP20) translated from reference tool
// No external process; produces: ["PP20" 4-byte table] + encoded payload + 4-byte trailer

static inline void store_be32(unsigned char* p, unsigned int v)
{
    p[0] = (unsigned char)((v >> 24) & 0xFF);
    p[1] = (unsigned char)((v >> 16) & 0xFF);
    p[2] = (unsigned char)((v >> 8)  & 0xFF);
    p[3] = (unsigned char)(v & 0xFF);
}

static inline unsigned int rol(unsigned int n, int c)
{
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
    c &= mask;
    return (n << c) | (n >> ((-c) & mask));
}

static inline unsigned short ror(unsigned short n, int c)
{
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
    c &= mask;
    return (unsigned short)((n >> c) | (n << ((-c) & mask)));
}

typedef struct {
    unsigned int token;
    unsigned int* ptr;
    unsigned int* ptr_max;
} write_res_t;

typedef struct {
    unsigned short w00[4];
    unsigned short w08[4];
    unsigned short w10[4];
    unsigned char  b2C[4];
    unsigned char* start;
    unsigned int   fsize;
    unsigned char* src_end;

    unsigned char** addrs;
    unsigned int   addrs_count;
    unsigned char* dst;
    unsigned int   tmp[0x80];
    unsigned char* print_pos;

    unsigned short value;
    unsigned short bits;

    unsigned short* wnd1;
    unsigned short* wnd2;
    unsigned short  wnd_max;
    unsigned short  wnd_off;
    unsigned short  wnd_left;
} CrunchInfo;

#define PP_READ_BITS(nbits, var) do {                \
    bit_cnt = (nbits); (var) = 0;                    \
    while (bits_left < bit_cnt) {                    \
        if (buf < src) return -1;                    \
        bit_buffer |= (unsigned int)(*--buf) << bits_left; \
        bits_left += 8;                               \
    }                                                 \
    bits_left -= bit_cnt;                             \
    while (bit_cnt--) {                               \
        (var) = ((var) << 1) | (bit_buffer & 1U);     \
        bit_buffer >>= 1;                             \
    }                                                 \
} while (0)

#define PP_BYTE_OUT(byte) do {                        \
    if (out <= dest) return -1;                      \
    *--out = (byte); written++;                      \
} while (0)

static void writeBits(int count, unsigned int value, write_res_t* dst, CrunchInfo* info)
{
    for (int i = 0; i < count; ++i) {
        int bit = (int)(value & 1U); value >>= 1;
        unsigned int c = dst->token & 0x80000000U;
        dst->token = (dst->token << 1) | (unsigned int)bit;
        if (c != 0U) {
            dst->ptr[0] = dst->token;
            dst->ptr++;
            dst->token = 1U;
            if (dst->ptr >= dst->ptr_max) {
                // Flush 0x80 dwords from tmp to dst as BE32
                unsigned int* tp = info->tmp;
                for (int j = 0; j < 0x80; ++j) {
                    store_be32(info->dst + (j << 2), tp[j]);
                }
                info->dst = &info->dst[0x200];
                dst->ptr = info->tmp;
            }
        }
    }
}

static void writeMoreBits(int count, write_res_t* dst, CrunchInfo* info)
{
    if (count < 4) {
        writeBits(2, (unsigned int)(count - 1), dst, info);
    } else {
        writeBits(2, (unsigned int)((count - 4) % 3), dst, info);
        for (int i = 0; i < (count - 4) / 3 + 1; ++i) {
            writeBits(2, 3U, dst, info);
        }
    }
    writeBits(1, 0U, dst, info);
}

static void prepareDict(int repeats, CrunchInfo* info)
{
    for (int i = 0; i < repeats; ++i) {
        info->wnd1[info->wnd_off] = 0;
        info->wnd2[info->wnd_off++] = 0;
        info->wnd_left--;
        if (info->wnd_left == 0) {
            info->wnd_left = info->wnd_max;
            info->wnd_off = 0;
        }
    }
}

static unsigned char* updateSpeedupLarge(unsigned char* curr, unsigned char* next, int count, CrunchInfo* info)
{
    unsigned char* src = &curr[info->wnd_max];
    for (int i = 0; i < count; ++i) {
        if (&src[i] >= (info->src_end - 1)) continue;
        unsigned short val = (unsigned short)((src[i + 0] << 8) | src[i + 1]);
        unsigned char* back = info->addrs[val];
        if (back != NULL) {
            int new_val = (int)(&src[i] - back);
            int diff = (int)(back - next);
            if (diff >= 0 && new_val < (int)(info->wnd_max / (unsigned int)sizeof(unsigned short))) {
                if (diff >= info->wnd_left) diff -= info->wnd_max;
                info->wnd1[info->wnd_off + diff] = (unsigned short)new_val;
                info->wnd2[info->wnd_off + diff] = (unsigned short)new_val;
            }
        }
        info->addrs[val] = &src[i];
    }
    return &src[count] - info->wnd_max;
}

static int ppCrunchBuffer_sub(CrunchInfo* info)
{
    for (int i = 0; i < info->wnd_max; ++i) {
        info->wnd1[i] = 0;
        info->wnd2[i] = 0;
    }
    for (int i = 0; i < (USHRT_MAX + 1); ++i) {
        info->addrs[i] = 0;
    }
    info->dst = info->start;
    info->wnd_off = 0;
    info->wnd_left = info->wnd_max;
    unsigned int max_size = info->wnd_left;
    if (info->wnd_left >= info->fsize) max_size = info->fsize;

    (void)updateSpeedupLarge(&info->start[-(int)info->wnd_max], info->start, (int)max_size, info);

    info->print_pos = info->start;
    unsigned char* src_curr = info->start;

    write_res_t res; res.ptr = info->tmp; res.ptr_max = &info->tmp[0x80]; res.token = 1U;
    int bits = 0;

    while (src_curr < info->src_end) {
        int progress = (int)(src_curr - info->print_pos);
        if (progress >= 0x200) {
            info->print_pos += progress;
        }
        unsigned char* src_max = &src_curr[0x7FFF];
        if (src_max >= info->src_end) src_max = info->src_end;
        int repeats = 1;
        unsigned char* next_src = &src_curr[1];
        unsigned char* cmp_src = NULL;
        unsigned short wnd_off_ = info->wnd_off;
        int bits_ = bits; unsigned int token_ = res.token;
        while (1) {
            next_src = &next_src[repeats - 1];
            cmp_src = &cmp_src[repeats - 1];
            int skip = 0;
            while (info->wnd1[info->wnd_off]) {
                int off = info->wnd1[info->wnd_off];
                next_src = &next_src[off];
                info->wnd_off = (unsigned short)(info->wnd_off + off);
                if (next_src < src_max && *next_src == src_curr[repeats] && next_src >= cmp_src) {
                    next_src = &next_src[1 - repeats];
                    cmp_src = &src_curr[2];
                    unsigned char* cmp_from = &next_src[1];
                    while (cmp_src < src_max && *cmp_src++ == *cmp_from++);
                    cmp_from--;
                    if (src_max < cmp_from) {
                        cmp_src = (unsigned char*)((ptrdiff_t)cmp_src - (ptrdiff_t)cmp_from + (ptrdiff_t)src_max);
                        cmp_from = src_max;
                    }
                    int curr_repeats = (int)(cmp_src - src_curr - 1);
                    if (curr_repeats > repeats) {
                        int shift = (int)(cmp_from - src_curr - curr_repeats);
                        unsigned short curr_bits = 3;
                        if (curr_repeats < 5) curr_bits = (unsigned short)(curr_repeats - 2);
                        if (info->w08[curr_bits] >= (unsigned short)shift) {
                            repeats = curr_repeats;
                            res.token = (res.token & 0xFFFF0000U) | (unsigned int)(shift & 0xFFFF);
                            bits = (int)curr_bits;
                        }
                    }
                    skip = 1; break;
                }
            }
            if (skip) continue;
            info->value = (unsigned short)(res.token & 0xFFFFU); info->bits = (unsigned short)bits; info->wnd_off = wnd_off_;
            bits = bits_; res.token = token_;
            if (repeats == 1) {
                writeBits(8, (unsigned int)src_curr[0], &res, info); bits++;
                prepareDict(1, info);
                src_curr = updateSpeedupLarge(src_curr, &src_curr[1], 1, info);
            } else {
                if (repeats < (int)info->wnd_max) {
                    prepareDict(repeats, info);
                    src_curr = updateSpeedupLarge(src_curr, &src_curr[repeats], repeats, info);
                } else {
                    src_curr = &src_curr[repeats];
                    info->wnd_off = 0; info->wnd_left = info->wnd_max;
                    for (int i = 0; i < info->wnd_max; ++i) { info->wnd1[info->wnd_off] = 0; info->wnd2[info->wnd_off++] = 0; }
                    info->wnd_off = 0; info->wnd_left = info->wnd_max;
                    src_curr = updateSpeedupLarge(&src_curr[-(int)info->wnd_left], src_curr, (int)info->wnd_left, info);
                }
                if (bits == 0) {
                    writeBits(1, 1U, &res, info);
                } else {
                    writeMoreBits(bits, &res, info); bits = 0;
                }
                unsigned short bits_count = info->bits; unsigned short value = (unsigned short)(info->value - 1); int count = 1;
                if (repeats >= 5) {
                    if (value >= 0x80) { writeBits(info->w00[bits_count], value, &res, info); value = 1; }
                    else { writeBits(7, value, &res, info); value = 0; }
                } else { count = info->w00[bits_count]; }
                writeBits(count, value, &res, info);
                writeBits(2, info->w10[bits_count], &res, info);
            }
            break;
        }
    }
    writeMoreBits(bits, &res, info);

    // Flush the last token dword if not already emitted (critical for correctness)
    bits = 0;
    if (res.token != 1U) {
        int bit = 0;
        while (!bit) {
            bits++;
            bit = (int)(res.token & 0x80000000U);
            res.token <<= 1;
        }
        // Original encoder uses a pointer to source end; we replicate with zero address
        unsigned int address = 0x00000000U;
        unsigned int last_token = 0U;
        for (int i = 0; i < bits; ++i) {
            unsigned int b = address & 1U;
            address >>= 1;
            last_token = (last_token << 1) | b;
        }
        last_token |= res.token;
        res.ptr[0] = last_token;
        res.ptr++;
    }

    int last_size = (int)(res.ptr - info->tmp);
    // Write remaining dwords from tmp to dst as BE32
    for (int i = 0; i < last_size; ++i) {
        unsigned int v = info->tmp[i];
        info->dst[(i<<2) + 0] = (unsigned char)((v >> 24) & 0xFF);
        info->dst[(i<<2) + 1] = (unsigned char)((v >> 16) & 0xFF);
        info->dst[(i<<2) + 2] = (unsigned char)((v >> 8)  & 0xFF);
        info->dst[(i<<2) + 3] = (unsigned char)(v & 0xFF);
    }
    // Trailer: 3-byte decompressed size (big-endian) + 1 byte of initial skip bits
    unsigned int trailer = (info->fsize << 8) | (unsigned int)(bits & 0xFF);
    unsigned char* tr = info->dst + (last_size << 2);
    tr[0] = (unsigned char)((trailer >> 24) & 0xFF);
    tr[1] = (unsigned char)((trailer >> 16) & 0xFF);
    tr[2] = (unsigned char)((trailer >> 8)  & 0xFF);
    tr[3] = (unsigned char)(trailer & 0xFF);
    return (int)(info->dst - info->start + (size_t)last_size * sizeof(unsigned int) + sizeof(unsigned int));
}

static CrunchInfo* ppAllocCrunchInfo(int eff, int old_version)
{
    CrunchInfo* info = (CrunchInfo*)malloc(sizeof(CrunchInfo));
    if (!info) return NULL;
    unsigned char eff_param3 = 10;
    unsigned char eff_param2 = 11;
    unsigned char eff_param1 = 11;
    switch (eff) {
    case 1:
        eff_param2 = 9; eff_param1 = 9; eff_param3 = 9; break;
    case 2:
        eff_param2 = 10; eff_param1 = 10; /* fallthrough */
    case 3:
        break;
    case 4:
        eff_param2 = 12; eff_param1 = 12; break;
    case 5:
        eff_param1 = 12; eff_param2 = 13; break;
    }
    info->w00[0] = info->b2C[0] = 9;
    info->w00[1] = info->b2C[1] = eff_param3;
    info->w00[2] = info->b2C[2] = eff_param1;
    info->w00[3] = info->b2C[3] = eff_param2;
    info->w08[0] = (1 << 9);
    info->w08[1] = (unsigned short)(1 << eff_param3);
    info->w08[2] = (unsigned short)(1 << eff_param1);
    info->w08[3] = (unsigned short)(1 << eff_param2);
    info->w10[0] = 0; info->w10[1] = 1; info->w10[2] = 2; info->w10[3] = 3;
    info->addrs_count = 0x10000;
    (void)old_version; // not used in this in-memory variant
    info->wnd_max = (unsigned short)((1U << eff_param2) * (unsigned int)sizeof(unsigned short));
    info->wnd1 = (unsigned short*)malloc((size_t)info->wnd_max * sizeof(unsigned short) * 2);
    info->wnd2 = NULL;
    info->addrs = NULL;
    if (info->wnd1) {
        info->wnd2 = &info->wnd1[info->wnd_max];
        memset(info->wnd1, 0, (size_t)info->wnd_max * sizeof(unsigned short) * 2);
        info->addrs = (unsigned char**)malloc((size_t)info->addrs_count * sizeof(unsigned char*));
        if (info->addrs) {
            memset(info->addrs, 0, (size_t)info->addrs_count * sizeof(unsigned char*));
            return info;
        }
    }
    if (info->addrs) free(info->addrs);
    if (info->wnd1) free(info->wnd1);
    free(info);
    return NULL;
}

static void ppFreeCrunchInfo(CrunchInfo* info)
{
    if (!info) return;
    if (info->addrs) free(info->addrs);
    if (info->wnd1) free(info->wnd1);
    free(info);
}

static int ppCrunchBuffer(unsigned int len, unsigned char* buf, CrunchInfo* info)
{
    info->start = buf; info->fsize = len; info->src_end = &buf[len];
    return ppCrunchBuffer_sub(info);
}

static int ppWriteDataHeader(int eff, int crypt, unsigned short checksum, const unsigned char* table, unsigned char* dst)
{
    (void)eff; (void)crypt; (void)checksum;
    // dst must have at least 8 bytes available
    memcpy(dst + 0, "PP20", 4);
    memcpy(dst + 4, table, 4);
    return 0;
}

int pp_compress_mem(const unsigned char* src, unsigned int len,
                    unsigned char** out, unsigned int* out_len,
                    int efficiency, int old_version)
{
    if (!src || !out || !out_len || len == 0) return -1;
    *out = NULL; *out_len = 0;
    CrunchInfo* info = ppAllocCrunchInfo(efficiency, old_version);
    if (!info) return -1;
    unsigned char* work = (unsigned char*)malloc(len);
    if (!work) { ppFreeCrunchInfo(info); return -1; }
    memcpy(work, src, len);
    int clen = ppCrunchBuffer(len, work, info);
    if (clen < 0) { free(work); ppFreeCrunchInfo(info); return -1; }
    size_t total = 8 + (size_t)clen;
    unsigned char* dst = (unsigned char*)malloc(total);
    if (!dst) { free(work); ppFreeCrunchInfo(info); return -1; }
    if (ppWriteDataHeader(efficiency, 0, 0, info->b2C, dst) != 0) {
        free(dst); free(work); ppFreeCrunchInfo(info); return -1;
    }
    memcpy(dst + 8, info->start, (size_t)clen);
    *out = dst; *out_len = (unsigned int)total;
    free(work); ppFreeCrunchInfo(info);
    return 0;
}
