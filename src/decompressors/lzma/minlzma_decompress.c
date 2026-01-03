// Minimal tiny core (no 7-Zip dependency)
#include "minlzma_core.h"

int minlzma_decompress_c(const char *src, char *dst, int compressedSize, int dstCapacity) {
    if (!src || !dst || compressedSize <= 5 || dstCapacity <= 0) return -1;

    const unsigned char *p = (const unsigned char *)src;
    unsigned p0 = p[0];
    unsigned lc = p0 % 9; p0 /= 9;
    unsigned lp = p0 % 5; unsigned pb = p0 / 5;
    // The stub scan already validated ranges; enforce tiny-core supported set
    if (!(lc == 3 && lp == 0 && pb == 2)) {
        return -1;
    }
    const unsigned char *comp = (const unsigned char *)(src + 5);
    int compLen = compressedSize - 5;
    int r = minlzma_decode(comp, compLen, (unsigned char *)dst, dstCapacity, lc, lp, pb);
    if (r <= 0) return r; /* propagate error for stub diagnostics */
    return (r <= dstCapacity) ? r : -1;
}
