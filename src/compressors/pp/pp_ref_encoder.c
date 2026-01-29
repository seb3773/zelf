/*
 * PowerPacker reference-like encoder (C99)
 * Ported from compression_libs/powerpacker/powerpacker._reference/powerpacker_src/main.cpp
 * In-memory API: pp_compress_mem_ref()
 *
 * Assumptions:
 * - CPU-bound; uses contiguous structures and memcpy for token flushing.
 * - No debug prints; no file I/O.
 * - Emits PP20 stream: ["PP20"][eff table 4B][tokens as BE32][trailer as BE32]
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

/* Helpers */
static inline unsigned int rol_u32(unsigned int n, int c) {
    const unsigned int mask = (unsigned int)(CHAR_BIT * sizeof(n) - 1);
    c &= (int)mask;
    return (n << c) | (n >> ((-c) & mask));
}

static inline unsigned short ror_u16(unsigned short n, int c) {
    const unsigned int mask = (unsigned int)(CHAR_BIT * sizeof(n) - 1);
    c &= (int)mask;
    return (unsigned short)((n >> c) | (n << ((-c) & mask)));
}

static size_t store_be_word(unsigned char* dst, size_t pos, unsigned short v) {
    dst[pos + 0] = (unsigned char)((v >> 8) & 0xFF);
    dst[pos + 1] = (unsigned char)(v & 0xFF);
    return 2;
}

static size_t store_be_dwords(unsigned char* dst, size_t pos, const unsigned int* buf, int byte_count) {
    size_t written = 0;
    int dcount = byte_count / 4;
    for (int i = 0; i < dcount; ++i) {
        unsigned int dw = buf[i];
        unsigned short hi = (unsigned short)((dw >> 16) & 0xFFFF);
        unsigned short lo = (unsigned short)(dw & 0xFFFF);
        written += store_be_word(dst, pos + written, hi);
        written += store_be_word(dst, pos + written, lo);
    }
    return written;
}

/* Encoder state */
typedef struct {
    unsigned int token;
    unsigned int* ptr;
    unsigned int* ptr_max;
} pp_wr_t;

typedef struct {
    unsigned short w00[4];
    unsigned short w08[4];
    unsigned short w10[4];
    unsigned char  b2C[4];
    unsigned char* start;
    unsigned int   fsize;
    unsigned char* src_end;

    unsigned char** addrs;
    unsigned int    addrs_count;
    unsigned char*  dst;
    unsigned int    tmp[0x80];
    unsigned char*  print_pos; /* unused for now */

    unsigned short  value;
    unsigned short  bits;

    unsigned short* wnd1;
    unsigned short* wnd2;
    unsigned short  wnd_max;
    unsigned short  wnd_off;
    unsigned short  wnd_left;
} PPRefInfo;

/* Forward decls */
static unsigned char* ppref_update_speedup(unsigned char* curr, unsigned char* next, int count, PPRefInfo* info);
static void ppref_write_bits(int count, unsigned int value, pp_wr_t* dst, PPRefInfo* info);
static void ppref_write_more_bits(int count, pp_wr_t* dst, PPRefInfo* info);
static void ppref_prepare_dict(int repeats, PPRefInfo* info);
static int  ppref_crunch_sub(PPRefInfo* info);
static PPRefInfo* ppref_alloc_info(int eff, int old_version);
static void ppref_free_info(PPRefInfo* info);

static void ppref_write_bits(int count, unsigned int value, pp_wr_t* dst, PPRefInfo* info) {
    for (int i = 0; i < count; ++i) {
        int bit = (int)(value & 1U);
        value >>= 1;
        unsigned int c = dst->token & 0x80000000U;
        dst->token = (dst->token << 1) | (unsigned int)bit;
        if (c != 0U) {
            dst->ptr[0] = dst->token;
            dst->ptr++;
            dst->token = 1U;
            if (dst->ptr >= dst->ptr_max) {
                /* flush tmp to dst */
                memcpy(info->dst, info->tmp, 0x200);
                info->dst = &info->dst[0x200];
                dst->ptr = info->tmp;
            }
        }
    }
}

static void ppref_write_more_bits(int count, pp_wr_t* dst, PPRefInfo* info) {
    if (count < 4) {
        ppref_write_bits(2, (unsigned int)(count - 1), dst, info);
    } else {
        ppref_write_bits(2, (unsigned int)((count - 4) % 3), dst, info);
        for (int i = 0; i < (count - 4) / 3 + 1; ++i) {
            ppref_write_bits(2, 3U, dst, info);
        }
    }
    ppref_write_bits(1, 0U, dst, info);
}

static void ppref_prepare_dict(int repeats, PPRefInfo* info) {
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

static unsigned char* ppref_update_speedup(unsigned char* curr, unsigned char* next, int count, PPRefInfo* info) {
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

static int ppref_crunch_sub(PPRefInfo* info) {
    for (int i = 0; i < info->wnd_max; ++i) { info->wnd1[i] = 0; info->wnd2[i] = 0; }
    for (int i = 0; i < (USHRT_MAX + 1); ++i) info->addrs[i] = 0;

    info->dst = info->start;
    info->wnd_off = 0;
    info->wnd_left = info->wnd_max;
    {
        unsigned int max_size = info->wnd_left;
        if (info->wnd_left >= info->fsize) max_size = info->fsize;
        ppref_update_speedup(&info->start[-(int)info->wnd_max], info->start, (int)max_size, info);
    }

    info->print_pos = info->start;
    {
        unsigned char* src_curr = info->start;
        pp_wr_t res; res.ptr = info->tmp; res.ptr_max = &info->tmp[0x80]; res.token = 1U;
        int bits = 0;
        while (src_curr < info->src_end) {
            int progress = (int)(src_curr - info->print_pos);
            if (progress >= 0x200) {
                info->print_pos += progress; /* progress callback omitted */
            }
            unsigned char* src_max = &src_curr[0x7FFF];
            if (src_max >= info->src_end) src_max = info->src_end;

            int repeats = 1;
            unsigned char* next_src = &src_curr[1];
            unsigned char* cmp_src = NULL;
            unsigned short wnd_off_ = info->wnd_off;
            int bits_ = bits;
            unsigned int token_ = res.token;

            while (1) {
                next_src = &next_src[repeats - 1];
                cmp_src = &cmp_src[repeats - 1];
                {
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
                                /* adjust for overrun using pointer difference */
                                ptrdiff_t delta = cmp_src - cmp_from;
                                cmp_src = src_max + delta;
                                cmp_from = src_max;
                            }
                            {
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
                            }
                            skip = 1; break;
                        }
                    }
                    if (skip) continue;
                }
                /* set_token */
                info->value = (unsigned short)(res.token & 0xFFFFU);
                info->bits  = (unsigned short)bits;
                info->wnd_off = wnd_off_;
                bits = bits_; res.token = token_;

                if (repeats == 1) {
                    ppref_write_bits(8, (unsigned int)src_curr[0], &res, info);
                    bits++;
                    ppref_prepare_dict(1, info);
                    src_curr = ppref_update_speedup(src_curr, &src_curr[1], 1, info);
                } else {
                    if (repeats < (int)info->wnd_max) {
                        ppref_prepare_dict(repeats, info);
                        src_curr = ppref_update_speedup(src_curr, &src_curr[repeats], repeats, info);
                    } else {
                        src_curr = &src_curr[repeats];
                        info->wnd_off = 0; info->wnd_left = info->wnd_max;
                        for (int i = 0; i < info->wnd_max; ++i) { info->wnd1[info->wnd_off] = 0; info->wnd2[info->wnd_off++] = 0; }
                        info->wnd_off = 0; info->wnd_left = info->wnd_max;
                        src_curr = ppref_update_speedup(&src_curr[-(int)info->wnd_left], src_curr, (int)info->wnd_left, info);
                    }
                    if (bits == 0) { ppref_write_bits(1, 1U, &res, info); }
                    else { ppref_write_more_bits(bits, &res, info); bits = 0; }
                    /* Emit repeats length bits (3-bit groups) */
                    if (repeats >= 5) {
                        if (repeats < 12) {
                            ppref_write_bits(3, (unsigned int)(repeats - 5), &res, info);
                        } else {
                            ppref_write_bits(3, (unsigned int)((repeats - 12) % 7), &res, info);
                            for (int i = 0; i < ((repeats - 12) / 7) + 1; ++i) {
                                ppref_write_bits(3, 7U, &res, info);
                            }
                        }
                    }
                    {
                        unsigned short bits_count = info->bits;
                        unsigned short value = (unsigned short)(info->value - 1);
                        int count = 1;
                        if (repeats >= 5) {
                            if (value >= 0x80) { ppref_write_bits((int)info->w00[bits_count], value, &res, info); value = 1; }
                            else { ppref_write_bits(7, value, &res, info); value = 0; }
                        } else { count = (int)info->w00[bits_count]; }
                        ppref_write_bits(count, value, &res, info);
                        ppref_write_bits(2, (unsigned int)info->w10[bits_count], &res, info);
                    }
                }
                break;
            }
        }
        ppref_write_more_bits(bits, &res, info);
        /* Flush last token if needed */
        bits = 0;
        if (res.token != 1U) {
            int bit = 0;
            while (!bit) { bits++; bit = (int)(res.token & 0x80000000U); res.token <<= 1; }
            {
                unsigned int address = 0x00000000U;
                unsigned int last_token = 0U;
                for (int i = 0; i < bits; ++i) { unsigned int b = address & 1U; address >>= 1; last_token = (last_token << 1) | b; }
                last_token |= res.token;
                res.ptr[0] = last_token; res.ptr++;
            }
        }
        {
            int last_size = (int)(res.ptr - info->tmp);
            memcpy(info->dst, info->tmp, (size_t)last_size * sizeof(unsigned int));
            ((unsigned int*)info->dst)[last_size] = (info->fsize << 8) | (unsigned int)(bits & 0xFF);
            return (int)(info->dst - info->start + (size_t)last_size * sizeof(unsigned int) + sizeof(unsigned int));
        }
    }
}

static void ppref_free_info(PPRefInfo* info) {
    if (!info) return;
    if (info->addrs) free(info->addrs);
    if (info->wnd1) free(info->wnd1);
    free(info);
}

static PPRefInfo* ppref_alloc_info(int eff, int old_version) {
    PPRefInfo* info = (PPRefInfo*)malloc(sizeof(PPRefInfo));
    if (!info) return NULL;
    unsigned char eff_param3 = 10, eff_param2 = 11, eff_param1 = 11;
    switch (eff) {
        case 1: eff_param2 = 9; eff_param1 = 9; eff_param3 = 9; break;
        case 2: eff_param2 = 10; eff_param1 = 10; /* fallthrough */
        case 3: break;
        case 4: eff_param2 = 12; eff_param1 = 12; break;
        case 5: eff_param1 = 12; eff_param2 = 13; break;
    }
    info->w00[0] = info->b2C[0] = 9;
    info->w00[1] = info->b2C[1] = eff_param3;
    info->w00[2] = info->b2C[2] = eff_param1;
    info->w00[3] = info->b2C[3] = eff_param2;
    info->w08[0] = (1U << 9);
    info->w08[1] = (unsigned short)(1U << eff_param3);
    info->w08[2] = (unsigned short)(1U << eff_param1);
    info->w08[3] = (unsigned short)(1U << eff_param2);
    info->w10[0] = 0; info->w10[1] = 1; info->w10[2] = 2; info->w10[3] = 3;
    info->addrs_count = 0x10000U;
    (void)old_version; /* keep signature; not used */
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

/* Public API */
int pp_compress_mem_ref(const unsigned char* input, unsigned int input_len,
                        unsigned char** out, unsigned int* out_len,
                        int efficiency, int old_version) {
    if (!input || input_len == 0 || !out || !out_len) return -1;
    *out = NULL; *out_len = 0;
    PPRefInfo* info = ppref_alloc_info(efficiency, old_version);
    if (!info) return -1;
    /* Working buffer same size as input */
    unsigned char* work = (unsigned char*)malloc(input_len);
    if (!work) { ppref_free_info(info); return -1; }
    memcpy(work, input, input_len);

    info->start = work; info->fsize = input_len; info->src_end = &work[input_len];
    int crunched_len = ppref_crunch_sub(info);
    if (crunched_len < 0) { free(work); ppref_free_info(info); return -1; }

    size_t out_size = 8 + (size_t)crunched_len;
    unsigned char* dst = (unsigned char*)malloc(out_size);
    if (!dst) { free(work); ppref_free_info(info); return -1; }

    /* header */
    dst[0] = 'P'; dst[1] = 'P'; dst[2] = '2'; dst[3] = '0';
    dst[4] = info->b2C[0]; dst[5] = info->b2C[1]; dst[6] = info->b2C[2]; dst[7] = info->b2C[3];

    /* serialize tokens and trailer from work buffer */
    {
        int dword_bytes = crunched_len - 4;
        int dword_count = dword_bytes / 4;
        size_t pos = 8;
        pos += store_be_dwords(dst, pos, (const unsigned int*)work, dword_bytes);
        {
            unsigned int trailer = ((const unsigned int*)work)[dword_count];
            dst[pos + 0] = (unsigned char)((trailer >> 24) & 0xFF);
            dst[pos + 1] = (unsigned char)((trailer >> 16) & 0xFF);
            dst[pos + 2] = (unsigned char)((trailer >> 8)  & 0xFF);
            dst[pos + 3] = (unsigned char)(trailer & 0xFF);
        }
    }

    *out = dst;
    *out_len = (unsigned int)out_size;
    free(work);
    ppref_free_info(info);
    return 0;
}
