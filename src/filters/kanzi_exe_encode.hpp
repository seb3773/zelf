/**
 * kanzi_exe_encode.hpp - Kanzi EXE Filter ENCODING ONLY
 *
 * This is the encode-only version (forward/filter only) of the Kanzi EXE filter.
 * Use this in compression tools or executable packers that only need to filter (not unfilter).
 *
 * Benefits:
 * - ~40% smaller than full version (~18 KB vs ~30 KB)
 * - No inverse/unfilter code = smaller binaries
 * - Cleaner API for encode-only use cases
 *
 * Supports:
 * - Windows PE (x86, x64, ARM64)
 * - Linux ELF (x86, x64, ARM64)
 * - macOS Mach-O (x64, ARM64)
 * - Auto-detection of executable type
 *
 * For decompression stubs, see: libs_decode_only/executable/kanzi_exe_decode.hpp
 * For full version, see: executable/kanzi_exefilter.hpp
 *
 * Copyright 2011-2025 Frederic Langlet
 * Licensed under the Apache License, Version 2.0
 */

#pragma once
#ifndef _KANZI_EXE_ENCODE_
#define _KANZI_EXE_ENCODE_

#include "kanzi_exe_types.hpp"
#include <stdexcept>
#include <cstring>

namespace kanzi {

// Magic numbers for file format detection
struct Magic {
    static const uint32 NO_MAGIC = 0;
    static const uint32 WIN_MAGIC = 0x5A4D;      // MZ
    static const uint32 ELF_MAGIC = 0x7F454C46;   // 0x7F E L F
    static const uint32 MAC_MAGIC32 = 0xFEEDFACE;
    static const uint32 MAC_CIGAM32 = 0xCEFAEDFE;
    static const uint32 MAC_MAGIC64 = 0xFEEDFACF;
    static const uint32 MAC_CIGAM64 = 0xCFFAEDFE;

    static inline uint32 getType(const byte* src) {
        uint32 magic32 = LittleEndian::readInt32(src);

        if (magic32 == ELF_MAGIC) return ELF_MAGIC;
        if (magic32 == MAC_MAGIC32) return MAC_MAGIC32;
        if (magic32 == MAC_CIGAM32) return MAC_CIGAM32;
        if (magic32 == MAC_MAGIC64) return MAC_MAGIC64;
        if (magic32 == MAC_CIGAM64) return MAC_CIGAM64;

        uint16 magic16 = LittleEndian::readInt16(src);
        if (magic16 == WIN_MAGIC) return WIN_MAGIC;

        return NO_MAGIC;
    }
};

// Data type detection
struct Global {
    enum DataType { UNDEFINED, TEXT, MULTIMEDIA, EXE, NUMERIC, BASE64, DNA, BIN, UTF8, SMALL_ALPHABET };

    static inline DataType detectSimpleType(int count, const uint32 histo[]) {
        (void)count;
        uint32 sum = 0;
        for (int i = 0; i < 256; i++)
            sum += histo[i];

        if (sum == 0) return BIN;

        uint32 printable = 0;
        for (int i = 32; i < 127; i++)
            printable += histo[i];

        if (printable > sum * 90 / 100)
            return TEXT;

        return BIN;
    }
};

/**
 * Kanzi EXE Filter - ENCODE ONLY
 *
 * This filter detects and transforms executable code (x86/x64/ARM64) to improve compression.
 * This version contains only the forward (encoding) functionality.
 */
class KanziEXEFilterEncode {
public:
    KanziEXEFilterEncode() {}
    ~KanziEXEFilterEncode() {}

    /**
     * Filter (encode) executable data
     *
     * @param input Input buffer containing executable data
     * @param inputSize Size of input data
     * @param output Output buffer for filtered data
     * @param outputSize Size of output buffer (must be >= getMaxEncodedLength(inputSize))
     * @param processedSize On success, contains the size of processed input
     * @return Size of output data, or 0 on failure
     */
    int forward(const byte* input, int inputSize, byte* output, int outputSize, int& processedSize);

    // Force filter on a specified code range (x86 mode). Returns 0 on failure.
    int forwardForceX86Range(const byte* input, int inputSize,
                             byte* output, int outputSize,
                             int& processedSize,
                             int codeStart, int codeEnd);

    /**
     * Get maximum encoded length
     */
    static inline int getMaxEncodedLength(int srcLen) {
        return (srcLen <= 256) ? srcLen + 32 : srcLen + srcLen / 8;
    }

    /**
     * Get minimum block size for filtering
     */
    static inline int getMinBlockSize() { return MIN_BLOCK_SIZE; }

    /**
     * Get maximum block size for filtering
     */
    static inline int getMaxBlockSize() { return MAX_BLOCK_SIZE; }

private:
    static const byte X86_MASK_JUMP = byte(0xFE);
    static const byte X86_INSTRUCTION_JUMP = byte(0xE8);
    static const byte X86_INSTRUCTION_JCC = byte(0x80);
    static const byte X86_TWO_BYTE_PREFIX = byte(0x0F);
    static const byte X86_MASK_JCC = byte(0xF0);
    static const byte X86_ESCAPE = byte(0x9B);
    static const byte NOT_EXE = byte(0x80);
    static const byte X86 = byte(0x40);
    static const byte ARM64 = byte(0x20);
    static const byte MASK_DT = byte(0x0F);
    static const int X86_ADDR_MASK = (1 << 24) - 1;
    static const int MASK_ADDRESS = 0xF0F0F0F0;
    static const int ARM_B_ADDR_MASK = (1 << 26) - 1;
    static const int ARM_B_OPCODE_MASK = 0xFFFFFFFF ^ ARM_B_ADDR_MASK;
    static const int ARM_B_ADDR_SGN_MASK = 1 << 25;
    static const int ARM_OPCODE_B = 0x14000000;
    static const int ARM_OPCODE_BL = 0x94000000;
    static const int ARM_CB_REG_BITS = 5;
    static const int ARM_CB_ADDR_MASK = 0x00FFFFE0;
    static const int ARM_CB_ADDR_SGN_MASK = 1 << 18;
    static const int ARM_CB_OPCODE_MASK = 0x7F000000;
    static const int ARM_OPCODE_CBZ = 0x34000000;
    static const int ARM_OPCODE_CBNZ = 0x35000000;
    static const int WIN_PE = 0x00004550;
    static const uint16 WIN_X86_ARCH = 0x014C;
    static const uint16 WIN_AMD64_ARCH = 0x8664;
    static const uint16 WIN_ARM64_ARCH = 0xAA64;
    static const int ELF_X86_ARCH = 0x03;
    static const int ELF_AMD64_ARCH = 0x3E;
    static const int ELF_ARM64_ARCH = 0xB7;
    static const int MAC_AMD64_ARCH = 0x01000007;
    static const int MAC_ARM64_ARCH = 0x0100000C;
    static const int MAC_MH_EXECUTE = 0x02;
    static const int MAC_LC_SEGMENT = 0x01;
    static const int MAC_LC_SEGMENT64 = 0x19;
    static const int MIN_BLOCK_SIZE = 4096;
    static const int MAX_BLOCK_SIZE = (1 << (26 + 2)) - 1;

    bool forwardARM(const byte* src, byte* dst, int count, int codeStart, int codeEnd, int& outSize);
    bool forwardX86(const byte* src, byte* dst, int count, int codeStart, int codeEnd, int& outSize);
    static byte detectType(const byte src[], int count, int& codeStart, int& codeEnd);
    static bool parseHeader(const byte src[], int count, uint32 magic, int& arch, int& codeStart, int& codeEnd);
};

} // namespace kanzi

#endif // _KANZI_EXE_ENCODE_
