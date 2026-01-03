#ifndef BCJ_X86_MIN_H
#define BCJ_X86_MIN_H

#include <stdint.h>
#include <stddef.h>

/* Ultra-minimal BCJ x86/x86_64 decoder
 * 
 * Converts absolute addresses back to relative addresses for
 * x86 CALL (0xE8) and JMP (0xE9) instructions after decompression.
 * 
 * @param buf    Data buffer to decode (modified in-place)
 * @param size   Size of the buffer in bytes (must be >= 5)
 * @param offset Virtual memory offset (use 0 if unknown)
 * @return       Number of bytes processed
 */
size_t bcj_x86_decode(uint8_t* buf, size_t size, uint32_t offset);

#endif /* BCJ_X86_MIN_H */
