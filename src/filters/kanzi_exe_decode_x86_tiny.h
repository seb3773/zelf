/**
 * kanzi_exe_decode_x86_tiny.h - Kanzi EXE Filter DECODE ONLY (x86/x64, MINIMAL SIZE)
 *
 * Ultra-minimal version optimized for smallest possible code size:
 * - Pure C (no C++ overhead)
 * - No exceptions (return 0 on error)
 * - No classes, pure function
 * - Minimal constants
 * - Direct inline operations
 *
 * Size: ~1.7 KB code
 *
 * Copyright 2011-2025 Frederic Langlet
 * Licensed under the Apache License, Version 2.0
 */

#ifndef KANZI_EXE_DECODE_X86_TINY_H
#define KANZI_EXE_DECODE_X86_TINY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal inline helpers */
static inline int32_t read_le32(const uint8_t* p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

/* Minimal local memcpy to avoid libc dependency in the stub */
static inline void kanzi_copy(uint8_t* dst, const uint8_t* src, size_t n) {
    while (n-- != 0) {
        *dst++ = *src++;
    }
}

static inline void write_le32(uint8_t* p, int32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline int32_t read_be32(const uint8_t* p) {
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

/**
 * Unfilter x86/x64 executable data (minimal version)
 *
 * Input format:
 *   byte[0]     = 0x40 (x86 mode marker)
 *   byte[1..4]  = codeStart (little-endian)
 *   byte[5..8]  = codeEnd (little-endian)
 *   byte[9..]   = filtered data
 *
 * Returns: output size, or 0 on error
 * processed_size: set to input_size on success
 */
static inline int kanzi_exe_unfilter_x86(const uint8_t* input, int input_size,
                                          uint8_t* output, int output_size,
                                          int* processed_size) {
    /* Quick validation */
    if (input_size < 9 || !input || !output) {
        *processed_size = 0;
        return 0;
    }

    /* Check mode byte (0x40 = x86) */
    if (input[0] != 0x40) {
        *processed_size = 0;
        return 0;
    }

    int src_idx = 9;
    int dst_idx = 0;
    const int code_start = read_le32(&input[1]);
    const int code_end = read_le32(&input[5]);

    /* Validate ranges */
    if (code_end > input_size || code_start + src_idx > input_size) {
        *processed_size = 0;
        return 0;
    }

    /* Copy pre-code data */
    if (code_start > 0) {
        kanzi_copy(&output[dst_idx], &input[src_idx], (size_t)code_start);
        dst_idx += code_start;
        src_idx += code_start;
    }

    /* Process code section */
    while (src_idx < code_end) {
        uint8_t b = input[src_idx];

        /* Check for 2-byte prefix (0x0F) */
        if (b == 0x0F) {
            output[dst_idx++] = input[src_idx++];

            /* Check if it's a JCC instruction (0x8X) */
            if ((input[src_idx] & 0xF0) != 0x80) {
                /* Not JCC, skip escape if present */
                if (input[src_idx] == 0x9B)
                    src_idx++;
                output[dst_idx++] = input[src_idx++];
                continue;
            }
        }
        /* Check for CALL/JMP (0xE8 or 0xE9) */
        else if ((b & 0xFE) != 0xE8) {
            /* Not CALL/JMP, skip escape if present */
            if (b == 0x9B)
                src_idx++;
            output[dst_idx++] = input[src_idx++];
            continue;
        }

        /* Transform address: read as big-endian, XOR mask, convert to offset */
        const int addr = read_be32(&input[src_idx + 1]) ^ 0xF0F0F0F0;
        const int offset = addr - dst_idx;

        output[dst_idx++] = input[src_idx++];

        /* Write offset as little-endian, mask if negative */
        write_le32(&output[dst_idx], (offset >= 0) ? offset : -(-offset & 0xFFFFFF));

        src_idx += 4;
        dst_idx += 4;
    }

    /* Copy post-code data */
    if (src_idx < input_size) {
        kanzi_copy(&output[dst_idx], &input[src_idx], (size_t)(input_size - src_idx));
        dst_idx += (input_size - src_idx);
    }

    *processed_size = input_size;
    return dst_idx;
}

#ifdef __cplusplus
}
#endif

#endif /* KANZI_EXE_DECODE_X86_TINY_H */
