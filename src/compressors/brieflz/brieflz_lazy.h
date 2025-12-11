//
// BriefLZ - small fast Lempel-Ziv
//
// Lazy parsing variant (minimal stub for in-tree build)
//
#ifndef BRIEFLZ_LAZY_H_INCLUDED
#define BRIEFLZ_LAZY_H_INCLUDED

#include <stddef.h>

static size_t
blz_lazy_workmem_size(size_t src_size)
{
    /* Minimal requirement: reuse base lookup table */
    (void)src_size;
    return LOOKUP_SIZE * sizeof(blz_word);
}

static unsigned long
blz_pack_lazy(const void *src, void *dst, unsigned long src_size, void *workmem)
{
    /* Fallback to base packer to keep integration simple */
    return blz_pack(src, dst, src_size, workmem);
}

#endif /* BRIEFLZ_LAZY_H_INCLUDED */
