#include <stdint.h>
#include <stddef.h>
#include "bcj_x86_enc.h"

#define TEST_MS_BYTE(b) ((b) == 0 || (b) == 0xFF)

typedef struct {
    uint32_t prev_mask;
    uint32_t prev_pos;
} bcj_x86_state_t;

size_t bcj_x86_encode(uint8_t* buffer, size_t size, uint32_t start_offset)
{
    static const uint32_t MASK_TO_BIT_NUMBER[5] = { 0, 1, 2, 2, 3 };

    bcj_x86_state_t state;
    state.prev_mask = 0;
    state.prev_pos = (uint32_t)(-5);

    uint32_t now_pos = start_offset;

    if (size < 5)
        return 0;

    if (now_pos - state.prev_pos > 5)
        state.prev_pos = now_pos - 5;

    const size_t limit = size - 5;
    size_t buffer_pos = 0;

    while (buffer_pos <= limit) {
        uint8_t opcode = buffer[buffer_pos];

        /* CALL (0xE8) or JMP (0xE9) */
        if (opcode != 0xE8 && opcode != 0xE9) {
            ++buffer_pos;
            continue;
        }

        const uint32_t offset = now_pos + (uint32_t)buffer_pos - state.prev_pos;
        state.prev_pos = now_pos + (uint32_t)buffer_pos;

        if (offset > 5) {
            state.prev_mask = 0;
        } else {
            for (uint32_t i = 0; i < offset; ++i) {
                state.prev_mask &= 0x77;
                state.prev_mask <<= 1;
            }
        }

        uint8_t b = buffer[buffer_pos + 4];

        if (TEST_MS_BYTE(b) && (state.prev_mask >> 1) <= 4 && (state.prev_mask >> 1) != 3) {
            /* read current 32-bit immediate (little-endian) */
            uint32_t src = ((uint32_t)b << 24)
                         | ((uint32_t)buffer[buffer_pos + 3] << 16)
                         | ((uint32_t)buffer[buffer_pos + 2] << 8)
                         | (uint32_t)buffer[buffer_pos + 1];

            uint32_t dest;
            while (1) {
                /* convert relative -> absolute */
                dest = src + (now_pos + (uint32_t)buffer_pos + 5);
                if (state.prev_mask == 0)
                    break;
                const uint32_t i = MASK_TO_BIT_NUMBER[state.prev_mask >> 1];
                b = (uint8_t)(dest >> (24 - i * 8));
                if (!TEST_MS_BYTE(b))
                    break;
                src = dest ^ ((1U << (32 - i * 8)) - 1);
            }

            /* write back */
            buffer[buffer_pos + 4] = (uint8_t)(~(((dest >> 24) & 1U) - 1U));
            buffer[buffer_pos + 3] = (uint8_t)(dest >> 16);
            buffer[buffer_pos + 2] = (uint8_t)(dest >> 8);
            buffer[buffer_pos + 1] = (uint8_t)(dest);
            buffer_pos += 5;
            state.prev_mask = 0;
        } else {
            ++buffer_pos;
            state.prev_mask |= 1;
            if (TEST_MS_BYTE(b))
                state.prev_mask |= 0x10;
        }
    }

    return buffer_pos;
}
