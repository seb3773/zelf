/* BriefLZ stub-side decompressor (no malloc)
 * Constraints: CPU-bound, no external deps. Uses in-tree BriefLZ depack API.
 * Assumptions: Linux x86_64, compiled with -ffunction-sections/-Wl,--gc-sections.
 */
#include <stddef.h>
#include "brieflz_decompress.h"
#include "brieflz.h"

/*
 * brieflz_decompress_to()
 * Complexity: O(output_size)
 * Dependencies: brieflz.h (blz_depack), no dynamic allocation.
 * Alignment: none specific; caller should provide adequately sized output buffer.
 */
int brieflz_decompress_to(const unsigned char *in, int in_size,
                          unsigned char *out, int out_max)
{
    if (!in || in_size <= 0 || !out || out_max <= 0)
        return -1;

    /* Use core depacker; caller ensures source has sufficient sentinel padding. */
    unsigned long r = blz_depack((const void*)in, (void*)out, (unsigned long)out_max);
    if (r > (unsigned long)out_max)
        return -1;
    return (int)r;
}
