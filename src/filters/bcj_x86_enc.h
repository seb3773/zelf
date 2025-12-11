#ifndef BCJ_X86_ENC_H
#define BCJ_X86_ENC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Encode x86/x86_64 BCJ in-place.
 * Converts 32-bit relative offsets of CALL/JMP (E8/E9) into absolute
 * addresses to improve compressibility.
 *
 * Complexity: O(n)
 * Safety: buffer must be writable, size >= 0
 */
size_t bcj_x86_encode(uint8_t *buffer, size_t size, uint32_t start_offset);

#ifdef __cplusplus
}
#endif

#endif /* BCJ_X86_ENC_H */
