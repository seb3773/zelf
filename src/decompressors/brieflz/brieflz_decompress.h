#ifndef BRIEFLZ_DECOMPRESS_H
#define BRIEFLZ_DECOMPRESS_H

#include <stddef.h>

/*
 * BriefLZ stub-side decompressor API.
 *
 * Constraints:
 * - No malloc; caller provides output buffer.
 * - Returns number of bytes written, or -1 on error.
 */
int brieflz_decompress_to(const unsigned char *in, int in_size,
                          unsigned char *out, int out_max);

#endif /* BRIEFLZ_DECOMPRESS_H */
