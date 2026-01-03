//
// BriefLZ - small fast Lempel-Ziv
//
// Left-extended parsing variant (minimal stub for in-tree build)
//
#ifndef BRIEFLZ_LEPARSE_H_INCLUDED
#define BRIEFLZ_LEPARSE_H_INCLUDED

#include <stddef.h>

static size_t
blz_leparse_workmem_size(size_t src_size)
{
    (void)src_size;
    return LOOKUP_SIZE * sizeof(blz_word);
}

static unsigned long
blz_pack_leparse(const void *src, void *dst, unsigned long src_size,
                 void *workmem, unsigned long back_limit,
                 unsigned long accept_len)
{
    (void)back_limit;
    (void)accept_len;
    /* Fallback to base packer */
    return blz_pack(src, dst, src_size, workmem);
}

#endif /* BRIEFLZ_LEPARSE_H_INCLUDED */
