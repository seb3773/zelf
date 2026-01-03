#include <stdint.h>
#include <stddef.h>

/* Ultra-minimal x86/x86_64 BCJ decoder
 * Optimized for minimal code size
 * Based on BCJ filter from XZ Utils by Igor Pavlov and Lasse Collin
 */

size_t bcj_x86_decode(uint8_t* buf, size_t size, uint32_t offset)
{
    uint32_t mask = 0, pos = (uint32_t)(-5);
    size_t i;
    
    if (size < 5)
        return 0;
    
    if (offset - pos > 5)
        pos = offset - 5;
    
    for (i = 0; i <= size - 5; i++) {
        if (buf[i] != 0xE8 && buf[i] != 0xE9) {
            continue;
        }
        
        uint32_t off = offset + (uint32_t)i - pos;
        pos = offset + (uint32_t)i;
        
        if (off > 5) {
            mask = 0;
        } else {
            while (off--) {
                mask &= 0x77;
                mask <<= 1;
            }
        }
        
        uint8_t b = buf[i + 4];
        if ((b == 0 || b == 0xFF) && (mask >> 1) <= 4 && (mask >> 1) != 3) {
            uint32_t src = ((uint32_t)b << 24) | ((uint32_t)buf[i + 3] << 16) |
                          ((uint32_t)buf[i + 2] << 8) | buf[i + 1];
            
            uint32_t dest;
            for (;;) {
                dest = src - (offset + (uint32_t)i + 5);
                if (mask == 0)
                    break;
                
                static const uint8_t t[5] = {0, 1, 2, 2, 3};
                uint32_t idx = t[mask >> 1];
                b = (uint8_t)(dest >> (24 - idx * 8));
                
                if (b != 0 && b != 0xFF)
                    break;
                
                src = dest ^ ((1U << (32 - idx * 8)) - 1);
            }
            
            buf[i + 4] = (uint8_t)(~(((dest >> 24) & 1) - 1));
            buf[i + 3] = (uint8_t)(dest >> 16);
            buf[i + 2] = (uint8_t)(dest >> 8);
            buf[i + 1] = (uint8_t)dest;
            i += 4;
            mask = 0;
        } else {
            mask |= 1;
            if (b == 0 || b == 0xFF)
                mask |= 0x10;
        }
    }
    
    return i;
}
