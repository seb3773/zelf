/**
 * kanzi_exe_decode_arm64_tiny.h - Kanzi EXE Filter DECODE ONLY (ARM64, MINIMAL SIZE)
 *
 * Ultra-minimal version optimized for smallest possible code size:
 * - Pure C (no C++ overhead)
 * - No exceptions (return 0 on error)
 * - No classes, pure function
 * - Minimal constants
 * - Direct inline operations
 *
 * Copyright 2011-2025 Frederic Langlet
 * Licensed under the Apache License, Version 2.0
 */

#ifndef KANZI_EXE_DECODE_ARM64_TINY_H
#define KANZI_EXE_DECODE_ARM64_TINY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal inline helpers */
static inline int32_t arm64_read_le32(const uint8_t* p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static inline void arm64_write_le32(uint8_t* p, int32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* Minimal local memcpy to avoid libc dependency in the stub */
static inline void arm64_kanzi_copy(uint8_t* dst, const uint8_t* src, size_t n) {
    while (n-- != 0) {
        *dst++ = *src++;
    }
}

/**
 * Unfilter ARM64 executable data (minimal version)
 *
 * Input format:
 *   byte[0]     = 0x20 (ARM64 mode marker)
 *   byte[1..4]  = codeStart (little-endian)
 *   byte[5..8]  = codeEnd (little-endian)
 *   byte[9..]   = filtered data
 *
 * Returns: output size, or 0 on error
 * processed_size: set to input_size on success
 */
static inline int kanzi_exe_unfilter_arm64(const uint8_t* input, int input_size,
                                           uint8_t* output, int output_size,
                                           int* processed_size) {
    /* Quick validation */
    if (input_size < 9 || !input || !output) {
        *processed_size = 0;
        return 0;
    }

    /* Check mode byte (0x20 = ARM64) */
    if (input[0] != 0x20) {
        *processed_size = 0;
        return 0;
    }

    int src_idx = 9;
    int dst_idx = 0;
    const int code_start = arm64_read_le32(&input[1]);
    const int code_end = arm64_read_le32(&input[5]);

    /* Validate ranges */
    if (code_end > input_size || code_start + src_idx > input_size ||
        code_start < 0 || code_end < src_idx || output_size <= 0) {
        *processed_size = 0;
        return 0;
    }

    /* Copy pre-code data */
    if (code_start > 0) {
        if (code_start > output_size) {
            *processed_size = 0;
            return 0;
        }
        arm64_kanzi_copy(&output[dst_idx], &input[src_idx], (size_t)code_start);
        dst_idx += code_start;
        src_idx += code_start;
    }

    const int ARM_B_OPCODE_MASK = 0xFC000000;
    const int ARM_OPCODE_B = 0x14000000;
    const int ARM_OPCODE_BL = 0x94000000;
    const int ARM_B_ADDR_MASK = 0x03FFFFFF;

    /* Process code section */
    while (src_idx < code_end) {
        if (src_idx + 4 > input_size) {
            *processed_size = 0;
            return 0;
        }
        if (dst_idx + 4 > output_size) {
            *processed_size = 0;
            return 0;
        }

        /* Read filtered instruction/value */
        int32_t val = arm64_read_le32(&input[src_idx]);
        int32_t opcode1 = val & ARM_B_OPCODE_MASK;
        
        if ((opcode1 == ARM_OPCODE_B) || (opcode1 == ARM_OPCODE_BL)) {
            src_idx += 4;
            int32_t addr = (val & ARM_B_ADDR_MASK) << 2;

            if (addr == 0) {
                if (src_idx + 4 > input_size) {
                    *processed_size = 0;
                    return 0;
                }
                arm64_kanzi_copy(&output[dst_idx], &input[src_idx], 4);
                src_idx += 4;
                dst_idx += 4;
            } else {
                int32_t offset = (addr - dst_idx) >> 2;
                int32_t instr = opcode1 | (offset & ARM_B_ADDR_MASK);
                arm64_write_le32(&output[dst_idx], instr);
                dst_idx += 4;
            }
        } else {
            arm64_write_le32(&output[dst_idx], val);
            src_idx += 4;
            dst_idx += 4;
        }
    }

    /* Copy post-code data */
    if (src_idx < input_size) {
        if (dst_idx + (input_size - src_idx) > output_size) {
            *processed_size = 0;
            return 0;
        }
        arm64_kanzi_copy(&output[dst_idx], &input[src_idx], (size_t)(input_size - src_idx));
        dst_idx += (input_size - src_idx);
    }

    *processed_size = input_size;
    return dst_idx;
}

#ifdef __cplusplus
}
#endif

#endif /* KANZI_EXE_DECODE_ARM64_TINY_H */
