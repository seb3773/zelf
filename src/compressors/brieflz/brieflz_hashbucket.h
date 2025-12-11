//
// BriefLZ - small fast Lempel-Ziv
//
// Hash-bucket variant (minimal stub for in-tree build)
//
#ifndef BRIEFLZ_HASHBUCKET_H_INCLUDED
#define BRIEFLZ_HASHBUCKET_H_INCLUDED

#include <stddef.h>

static size_t
blz_hashbucket_workmem_size(size_t src_size, unsigned int bucket_size)
{
    (void)src_size; (void)bucket_size;
    /* Minimal requirement: reuse base lookup table */
    return LOOKUP_SIZE * sizeof(blz_word);
}

static unsigned long
blz_pack_hashbucket(const void *src, void *dst, unsigned long src_size, void *workmem,
                    const unsigned int bucket_size, const unsigned long accept_len)
{
    (void)bucket_size; (void)accept_len;
    /* Fallback to base packer */
    return blz_pack(src, dst, src_size, workmem);
}

#endif /* BRIEFLZ_HASHBUCKET_H_INCLUDED */
