#ifndef RNC_MINIDEC_H
#define RNC_MINIDEC_H

#include <stddef.h>
#include <stdint.h>

/* Complexity: O(out_len) time, O(1) extra heap (caller provided workspace via internal mmap in stub build).
 * Dependencies: none (nostdlib-friendly); uses only basic integer ops.
 * Alignment: none.
 * Notes: supports only RNC method 1 streams ("RNC\x01"). CRC checks are intentionally omitted for stub size.
 */
int rnc_decompress_to(const unsigned char *src, int src_len,
                      unsigned char *dst, int dst_cap);

#endif
