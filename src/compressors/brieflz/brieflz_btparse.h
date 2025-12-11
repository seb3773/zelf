//
// BriefLZ - small fast Lempel-Ziv
//
// Forwards dynamic programming parse using binary trees
// (copied for in-tree build)
//
#ifndef BRIEFLZ_BTPARSE_H_INCLUDED
#define BRIEFLZ_BTPARSE_H_INCLUDED

#include <assert.h>
#include <limits.h>
#include <stddef.h>

static size_t blz_btparse_workmem_size(size_t src_size)
{
    return (5 * src_size + 3 + LOOKUP_SIZE) * sizeof(blz_word);
}

static unsigned long
blz_pack_btparse(const void *src, void *dst, unsigned long src_size, void *workmem,
                 const unsigned long max_depth, const unsigned long accept_len)
{
    struct blz_state bs;
    const unsigned char *const in = (const unsigned char *) src;
    const unsigned long last_match_pos = src_size > 4 ? src_size - 4 : 0;

    assert(src_size < BLZ_WORD_MAX);

    if (src_size == 0) {
        return 0;
    }

    bs.next_out = (unsigned char *) dst;
    *bs.next_out++ = in[0];
    if (src_size == 1) return 1;

    bs.tag_out = bs.next_out; bs.next_out += 2; bs.tag = 0; bs.bits_left = 16;

    if (src_size < 4) {
        for (unsigned long i = 1; i < src_size; ++i) { blz_putbit(&bs, 0); *bs.next_out++ = in[i]; }
        return (unsigned long) (blz_finalize(&bs) - (unsigned char *) dst);
    }

    blz_word *const cost = (blz_word *) workmem;
    blz_word *const mpos = cost + src_size + 1;
    blz_word *const mlen = mpos + src_size + 1;
    blz_word *const nodes = mlen + src_size + 1;
    blz_word *const lookup = nodes + 2 * src_size;

    for (unsigned long i = 0; i < LOOKUP_SIZE; ++i) lookup[i] = NO_MATCH_POS;

    lookup[blz_hash4(&in[0])] = 0; nodes[0] = NO_MATCH_POS; nodes[1] = NO_MATCH_POS;

    for (unsigned long i = 0; i <= src_size; ++i) { cost[i] = BLZ_WORD_MAX; mlen[i] = 1; }
    cost[0] = 0; cost[1] = 8;

    unsigned long next_match_cur = 1;

    for (unsigned long cur = 1; cur <= last_match_pos; ++cur) {
        if (cost[cur] > BLZ_WORD_MAX - 128) {
            blz_word min_cost = BLZ_WORD_MAX;
            for (unsigned long i = cur; i <= src_size; ++i) { min_cost = cost[i] < min_cost ? cost[i] : min_cost; }
            for (unsigned long i = cur; i <= src_size; ++i) { if (cost[i] != BLZ_WORD_MAX) cost[i] -= min_cost; }
        }
        if (cost[cur + 1] > cost[cur] + 9) { cost[cur + 1] = cost[cur] + 9; mlen[cur + 1] = 1; }
        if (cur > next_match_cur) next_match_cur = cur;
        unsigned long max_len = 3;
        const unsigned long hash = blz_hash4(&in[cur]);
        unsigned long pos = lookup[hash]; lookup[hash] = cur;
        blz_word *lt_node = &nodes[2 * cur]; blz_word *gt_node = &nodes[2 * cur + 1];
        unsigned long lt_len = 0; unsigned long gt_len = 0;
        assert(pos == NO_MATCH_POS || pos < cur);
        const unsigned long len_limit = cur == next_match_cur ? src_size - cur : (accept_len < src_size - cur ? accept_len : src_size - cur);
        unsigned long num_chain = max_depth;
        for (;;) {
            if (pos == NO_MATCH_POS || num_chain-- == 0) { *lt_node = NO_MATCH_POS; *gt_node = NO_MATCH_POS; break; }
            unsigned long len = lt_len < gt_len ? lt_len : gt_len;
            while (len < len_limit && in[pos + len] == in[cur + len]) { ++len; }
            if (cur == next_match_cur && len > max_len) {
                for (unsigned long i = max_len + 1; i <= len; ++i) {
                    unsigned long match_cost = blz_match_cost(cur - pos - 1, i);
                    assert(match_cost < BLZ_WORD_MAX - cost[cur]);
                    unsigned long cost_there = cost[cur] + match_cost;
                    if (cost_there < cost[cur + i]) { cost[cur + i] = cost_there; mpos[cur + i] = cur - pos - 1; mlen[cur + i] = i; }
                }
                max_len = len; if (len >= accept_len) next_match_cur = cur + len;
            }
            if (len >= accept_len || len == len_limit) { *lt_node = nodes[2 * pos]; *gt_node = nodes[2 * pos + 1]; break; }
            if (in[pos + len] < in[cur + len]) { *lt_node = pos; lt_node = &nodes[2 * pos + 1]; assert(*lt_node == NO_MATCH_POS || *lt_node < pos); pos = *lt_node; lt_len = len; }
            else { *gt_node = pos; gt_node = &nodes[2 * pos]; assert(*gt_node == NO_MATCH_POS || *gt_node < pos); pos = *gt_node; gt_len = len; }
        }
    }

    for (unsigned long cur = last_match_pos + 1; cur < src_size; ++cur) { if (cost[cur + 1] > cost[cur] + 9) { cost[cur + 1] = cost[cur] + 9; mlen[cur + 1] = 1; } }

    unsigned long next_token = src_size;
    for (unsigned long cur = src_size; cur > 1; cur -= mlen[cur], --next_token) { mlen[next_token] = mlen[cur]; mpos[next_token] = mpos[cur]; }

    unsigned long cur = 1;
    for (unsigned long i = next_token + 1; i <= src_size; cur += mlen[i++]) {
        if (mlen[i] == 1) { blz_putbit(&bs, 0); *bs.next_out++ = in[cur]; }
        else { const unsigned long offs = mpos[i]; blz_putbit(&bs, 1); blz_putgamma(&bs, mlen[i] - 2); blz_putgamma(&bs, (offs >> 8) + 2); *bs.next_out++ = offs & 0x00FF; }
    }

    return (unsigned long) (blz_finalize(&bs) - (unsigned char *) dst);
}

#endif /* BRIEFLZ_BTPARSE_H_INCLUDED */
