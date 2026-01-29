#ifndef MINLZMA_CORE_H
#define MINLZMA_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tiny LZMA core, raw stream with 5-byte props header. 
 * Assumptions: valid props (lc<=8, lp<=4, pb<=4) already checked by caller.
 * Dictionary is external and equals dst buffer; no separate window.
 * Returns number of bytes written to dst, or -1 on error.
 */
int minlzma_decode(const unsigned char *src, int srcLen,
                   unsigned char *dst, int dstLen,
                   unsigned lc, unsigned lp, unsigned pb);

#ifdef __cplusplus
}
#endif

#endif /* MINLZMA_CORE_H */
