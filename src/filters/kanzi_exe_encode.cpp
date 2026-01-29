/**
 * kanzi_exe_encode.cpp - Implementation of Kanzi EXE Filter (ENCODE ONLY)
 *
 * Copyright 2011-2025 Frederic Langlet
 * Licensed under the Apache License, Version 2.0
 */

#include "kanzi_exe_encode.hpp"

namespace kanzi {

int KanziEXEFilterEncode::forward(const byte* input, int inputSize, byte* output, int outputSize, int& processedSize) {
    if (inputSize == 0) {
        processedSize = 0;
        return 0;
    }

    if ((inputSize < MIN_BLOCK_SIZE) || (inputSize > MAX_BLOCK_SIZE))
        return 0;

    if (input == nullptr || output == nullptr)
        return 0;

    if (outputSize < getMaxEncodedLength(inputSize))
        return 0;

    int codeStart = 0;
    int codeEnd = inputSize - 8;
    byte mode = detectType(input, inputSize - 4, codeStart, codeEnd);

    if ((mode & NOT_EXE) != byte(0))
        return 0;

    mode &= ~MASK_DT;

    int outSize = outputSize;  // Initialize with output buffer size
    bool success = false;

    if (mode == X86)
        success = forwardX86(input, output, inputSize, codeStart, codeEnd, outSize);
    else if (mode == ARM64)
        success = forwardARM(input, output, inputSize, codeStart, codeEnd, outSize);

    if (success) {
        processedSize = inputSize;
        return outSize;
    }

    return 0;
}

int KanziEXEFilterEncode::forwardForceX86Range(const byte* input, int inputSize,
                                               byte* output, int outputSize,
                                               int& processedSize,
                                               int codeStart, int codeEnd) {
    if (inputSize == 0) {
        processedSize = 0;
        return 0;
    }

    if ((inputSize < MIN_BLOCK_SIZE) || (inputSize > MAX_BLOCK_SIZE))
        return 0;

    if (input == nullptr || output == nullptr)
        return 0;

    if (outputSize < getMaxEncodedLength(inputSize))
        return 0;

    if (codeStart < 0) codeStart = 0;
    if (codeEnd > inputSize) codeEnd = inputSize;
    if (codeEnd <= codeStart + 8)
        return 0;

    int outSize = outputSize;
    bool success = forwardX86(input, output, inputSize, codeStart, codeEnd, outSize);

    if (success) {
        processedSize = inputSize;
        return outSize;
    }

    return 0;
}

int KanziEXEFilterEncode::forwardForceARMRange(const byte* input, int inputSize,
                                               byte* output, int outputSize,
                                               int& processedSize,
                                               int codeStart, int codeEnd) {
    if (inputSize == 0) {
        processedSize = 0;
        return 0;
    }

    if ((inputSize < MIN_BLOCK_SIZE) || (inputSize > MAX_BLOCK_SIZE))
        return 0;

    if (input == nullptr || output == nullptr)
        return 0;

    if (outputSize < getMaxEncodedLength(inputSize))
        return 0;

    if (codeStart < 0) codeStart = 0;
    if (codeEnd > inputSize) codeEnd = inputSize;
    codeStart &= ~3;
    codeEnd &= ~3;

    if (codeEnd <= codeStart + 16)
        return 0;

    int outSize = outputSize;
    bool success = forwardARM(input, output, inputSize, codeStart, codeEnd, outSize);

    if (success) {
        processedSize = inputSize;
        return outSize;
    }

    return 0;
}

bool KanziEXEFilterEncode::forwardX86(const byte* src, byte* dst, int count, int codeStart, int codeEnd, int& outSize) {
    dst[0] = X86;
    int srcIdx = codeStart;
    int dstIdx = 9;
    int matches = 0;
    const int dstEnd = outSize - 5;

    if (codeEnd > count)
        return false;

    if (codeStart > 0) {
        memcpy(&dst[dstIdx], &src[0], size_t(codeStart));
        dstIdx += codeStart;
    }

    while ((srcIdx < codeEnd) && (dstIdx < dstEnd)) {
        if (src[srcIdx] == X86_TWO_BYTE_PREFIX) {
            dst[dstIdx++] = src[srcIdx++];

            if ((src[srcIdx] & X86_MASK_JCC) != X86_INSTRUCTION_JCC) {
                if (src[srcIdx] == X86_ESCAPE)
                    dst[dstIdx++] = X86_ESCAPE;

                dst[dstIdx++] = src[srcIdx++];
                continue;
            }
        } else if ((src[srcIdx] & X86_MASK_JUMP) != X86_INSTRUCTION_JUMP) {
            if (src[srcIdx] == X86_ESCAPE)
                dst[dstIdx++] = X86_ESCAPE;

            dst[dstIdx++] = src[srcIdx++];
            continue;
        }

        // Current instruction is a jump/call
        const int sgn = int(src[srcIdx + 4]);
        const int offset = LittleEndian::readInt32(&src[srcIdx + 1]);

        if (((sgn != 0) && (sgn != 0xFF)) || (offset == int(0xFF000000))) {
            dst[dstIdx++] = X86_ESCAPE;
            dst[dstIdx++] = src[srcIdx++];
            continue;
        }

        // Absolute target address = srcIdx + 5 + offset. Ignore +5
        const int addr = srcIdx + ((sgn == 0) ? offset : -(-offset & X86_ADDR_MASK));
        dst[dstIdx++] = src[srcIdx++];
        BigEndian::writeInt32(&dst[dstIdx], addr ^ MASK_ADDRESS);
        srcIdx += 4;
        dstIdx += 4;
        matches++;
    }

    if ((srcIdx < codeEnd) || (matches < 8))
        return false;

    if (dstIdx + (count - srcIdx) > dstEnd)
        return false;

    LittleEndian::writeInt32(&dst[1], codeStart);
    LittleEndian::writeInt32(&dst[5], dstIdx);
    memcpy(&dst[dstIdx], &src[srcIdx], size_t(count - srcIdx));
    dstIdx += (count - srcIdx);

    // Cap expansion due to false positives (allow up to +4%)
    if (dstIdx > count + (count / 25))
        return false;

    outSize = dstIdx;
    return true;
}

bool KanziEXEFilterEncode::forwardARM(const byte* src, byte* dst, int count, int codeStart, int codeEnd, int& outSize) {
    dst[0] = ARM64;
    int srcIdx = codeStart;
    int dstIdx = 9;
    int matches = 0;
    const int dstEnd = outSize - 8;

    if (codeEnd > count)
        return false;

    if (codeStart > 0) {
        memcpy(&dst[dstIdx], &src[0], size_t(codeStart));
        dstIdx += codeStart;
    }

    while ((srcIdx < codeEnd) && (dstIdx < dstEnd)) {
        const int instr = LittleEndian::readInt32(&src[srcIdx]);
        const int opcode1 = instr & ARM_B_OPCODE_MASK;
        bool isBL = (opcode1 == ARM_OPCODE_B) || (opcode1 == ARM_OPCODE_BL);
        bool isCB = false; // disabled for now

        if ((isBL == false) && (isCB == false)) {
            memcpy(&dst[dstIdx], &src[srcIdx], 4);
            srcIdx += 4;
            dstIdx += 4;
            continue;
        }

        int addr, val;

        if (isBL == true) {
            const int offset = instr & ARM_B_ADDR_MASK;
            const int sgn = instr & ARM_B_ADDR_SGN_MASK;
            addr = srcIdx + 4 * ((sgn == 0) ? offset : -(-offset & ARM_B_ADDR_MASK));

            if (addr < 0)
                addr = 0;

            val = opcode1 | (addr >> 2);
        } else { // isCB == true
            const int offset = (instr & ARM_CB_ADDR_MASK) >> ARM_CB_REG_BITS;
            const int sgn = instr & ARM_CB_ADDR_SGN_MASK;
            addr = srcIdx + 4 * ((sgn == 0) ? offset : -(-offset & ARM_B_ADDR_MASK));

            if (addr < 0)
                addr = 0;

            val = (instr & ~ARM_CB_ADDR_MASK) | ((addr >> 2) << ARM_CB_REG_BITS);
        }

        if (addr == 0) {
            LittleEndian::writeInt32(&dst[dstIdx], val);
            memcpy(&dst[dstIdx + 4], &src[srcIdx], 4);
            srcIdx += 4;
            dstIdx += 8;
            continue;
        }

        LittleEndian::writeInt32(&dst[dstIdx], val);
        srcIdx += 4;
        dstIdx += 4;
        matches++;
    }

    if ((srcIdx < codeEnd) || (matches < 16))
        return false;

    if (dstIdx + (count - srcIdx) > dstEnd)
        return false;

    LittleEndian::writeInt32(&dst[1], codeStart);
    LittleEndian::writeInt32(&dst[5], dstIdx);
    memcpy(&dst[dstIdx], &src[srcIdx], size_t(count - srcIdx));
    dstIdx += (count - srcIdx);

    // Cap expansion due to false positives
    if (dstIdx > count + (count / 50))
        return false;

    outSize = dstIdx;
    return true;
}

byte KanziEXEFilterEncode::detectType(const byte src[], int count, int& codeStart, int& codeEnd) {
    const uint32 magic = Magic::getType(src);
    int arch = 0;

    if (parseHeader(src, count, magic, arch, codeStart, codeEnd) == true) {
        switch(arch) {
            case ELF_X86_ARCH:
            case ELF_AMD64_ARCH:
            case WIN_X86_ARCH:
            case WIN_AMD64_ARCH:
            case MAC_AMD64_ARCH:
                return X86;

            case ELF_ARM64_ARCH:
            case WIN_ARM64_ARCH:
            case MAC_ARM64_ARCH:
                return ARM64;

            default:
                count = codeEnd - codeStart;
        }
    }

    int jumpsX86 = 0;
    int jumpsARM64 = 0;
    uint32 histo[256] = { 0 };

    for (int i = codeStart; i < codeEnd; i++) {
        histo[int(src[i])]++;

        // X86
        if ((src[i] & X86_MASK_JUMP) == X86_INSTRUCTION_JUMP) {
            if ((src[i + 4] == byte(0)) || (src[i + 4] == byte(0xFF))) {
                jumpsX86++;
                continue;
            }
        } else if (src[i] == X86_TWO_BYTE_PREFIX) {
            i++;

            if ((src[i] == byte(0x38)) || (src[i] == byte(0x3A)))
                i++;

            if ((src[i] & X86_MASK_JCC) == X86_INSTRUCTION_JCC) {
                jumpsX86++;
                continue;
            }
        }

        // ARM
        if ((i & 3) != 0)
            continue;

        const int instr = LittleEndian::readInt32(&src[i]);
        const int opcode1 = instr & ARM_B_OPCODE_MASK;
        const int opcode2 = instr & ARM_CB_OPCODE_MASK;

        if ((opcode1 == ARM_OPCODE_B) || (opcode1 == ARM_OPCODE_BL) ||
             (opcode2 == ARM_OPCODE_CBZ) || (opcode2 == ARM_OPCODE_CBNZ))
            jumpsARM64++;
    }

    Global::DataType dt = Global::detectSimpleType(count, histo);

    if (dt != Global::BIN)
        return NOT_EXE | byte(dt);

    // Filter out multimedia files
    if ((histo[0] < uint32(count / 10)) || (histo[255] < uint32(count / 100)))
        return NOT_EXE | byte(dt);

    int smallVals = 0;

    for (int i = 0; i < 16; i++)
        smallVals += histo[i];

    if (smallVals > (count / 2))
        return NOT_EXE | byte(dt);

    // Ad-hoc thresholds
    if ((jumpsX86 >= (count / 200)) && (histo[255] >= uint32(count / 50)))
        return X86;

    if (jumpsARM64 >= (count / 200))
        return ARM64;

    return NOT_EXE | byte(dt);
}

bool KanziEXEFilterEncode::parseHeader(const byte src[], int count, uint32 magic, int& arch, int& codeStart, int& codeEnd) {
    if (magic == Magic::WIN_MAGIC) {
        if (count >= 64) {
            const int posPE = LittleEndian::readInt32(&src[60]);

            if ((posPE > 0) && (posPE <= count - 48) && (LittleEndian::readInt32(&src[posPE]) == WIN_PE)) {
                const byte* pe = &src[posPE];
                codeStart = min(LittleEndian::readInt32(&pe[44]), count);
                codeEnd = min(codeStart + LittleEndian::readInt32(&pe[28]), count);
                arch = LittleEndian::readInt16(&pe[4]);
            }

            return true;
        }
    } else if (magic == Magic::ELF_MAGIC) {
        bool isLE = src[5] == byte(1);

        if (count >= 64) {
            codeStart = 0;
            codeEnd = 0;
            const int SHF_EXECINSTR = 0x4; // executable instruction section

            if (isLE) {
                if (src[4] == byte(2)) {
                    // ELF64 little-endian
                    int shnum = int(LittleEndian::readInt16(&src[0x3C]));
                    int shentsz = int(LittleEndian::readInt16(&src[0x3A]));
                    int shoff = int(LittleEndian::readLong64(&src[0x28]));
                    int shstrndx = int(LittleEndian::readInt16(&src[0x3E]));

                    // Resolve section header string table offset
                    int shstr_hdr = shoff + shstrndx * shentsz;
                    if (shstr_hdr + 0x28 < count) {
                        int shstr_off = int(LittleEndian::readLong64(&src[shstr_hdr + 0x18]));
                        // First, try to locate ".text" exactly
                        for (int i = 0; i < shnum; i++) {
                            int ent = shoff + i * shentsz;
                            if (ent + 0x28 >= count) return false;
                            int type = int(LittleEndian::readInt32(&src[ent + 4]));
                            int name_off = int(LittleEndian::readInt32(&src[ent + 0x00]));
                            int off = int(LittleEndian::readLong64(&src[ent + 0x18]));
                            int len = int(LittleEndian::readLong64(&src[ent + 0x20]));
                            if (type == 1 && len >= 16) {
                                int name_ptr = shstr_off + name_off;
                                if (name_ptr >= 0 && name_ptr + 6 <= count) {
                                    if (std::memcmp(&src[name_ptr], ".text", 5) == 0 && src[name_ptr + 5] == 0) {
                                        codeStart = off;
                                        codeEnd = off + len;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (codeEnd == 0) {
                        // Fallback: any executable PROGBITS (if section headers available)
                        int minOff = count, maxEnd = 0; bool found = false;
                        for (int i = 0; i < shnum; i++) {
                            int ent = shoff + i * shentsz;
                            if (ent + 0x28 >= count) return false;
                            int type = int(LittleEndian::readInt32(&src[ent + 4]));
                            int64_t flags = (int64_t)LittleEndian::readLong64(&src[ent + 0x08]);
                            int off = int(LittleEndian::readLong64(&src[ent + 0x18]));
                            int len = int(LittleEndian::readLong64(&src[ent + 0x20]));
                            if (type == 1 && (flags & SHF_EXECINSTR) && len >= 16) {
                                if (!found) { minOff = off; maxEnd = off + len; found = true; }
                                else {
                                    if (off < minOff) minOff = off;
                                    if (off + len > maxEnd) maxEnd = off + len;
                                }
                            }
                        }
                        if (found) { codeStart = minOff; codeEnd = maxEnd; }
                    }
                    if (codeEnd == 0) {
                        // Last resort when section headers are stripped: use PT_LOAD with PF_X
                        int phoff = int(LittleEndian::readLong64(&src[0x20]));
                        int phentsz = int(LittleEndian::readInt16(&src[0x36]));
                        int phnum = int(LittleEndian::readInt16(&src[0x38]));
                        int minOff = count, maxEnd = 0; bool foundPH = false;
                        for (int i = 0; i < phnum; i++) {
                            int ent = phoff + i * phentsz;
                            if (ent + 0x38 >= count) break; // sizeof(Elf64_Phdr)
                            int p_type  = int(LittleEndian::readInt32(&src[ent + 0x00]));
                            int p_flags = int(LittleEndian::readInt32(&src[ent + 0x04]));
                            if (p_type == 1 && (p_flags & 0x1)) { // PT_LOAD + PF_X
                                int off = int(LittleEndian::readLong64(&src[ent + 0x08]));
                                int filesz = int(LittleEndian::readLong64(&src[ent + 0x20]));
                                int end = off + filesz;
                                if (!foundPH) { minOff = off; maxEnd = end; foundPH = true; }
                                else { if (off < minOff) minOff = off; if (end > maxEnd) maxEnd = end; }
                            }
                        }
                        if (foundPH) { codeStart = minOff; codeEnd = maxEnd; }
                    }
                    arch = LittleEndian::readInt16(&src[18]);
                } else {
                    // ELF32 little-endian
                    int shnum = int(LittleEndian::readInt16(&src[0x30]));
                    int shentsz = int(LittleEndian::readInt16(&src[0x2E]));
                    int shoff = int(LittleEndian::readInt32(&src[0x20]));
                    int shstrndx = int(LittleEndian::readInt16(&src[0x32]));

                    int shstr_hdr = shoff + shstrndx * shentsz;
                    if (shstr_hdr + 0x18 < count) {
                        int shstr_off = int(LittleEndian::readInt32(&src[shstr_hdr + 0x10]));
                        for (int i = 0; i < shnum; i++) {
                            int ent = shoff + i * shentsz;
                            if (ent + 0x18 >= count) return false;
                            int type = int(LittleEndian::readInt32(&src[ent + 4]));
                            int name_off = int(LittleEndian::readInt32(&src[ent + 0x00]));
                            int off = int(LittleEndian::readInt32(&src[ent + 0x10]));
                            int len = int(LittleEndian::readInt32(&src[ent + 0x14]));
                            if (type == 1 && len >= 16) {
                                int name_ptr = shstr_off + name_off;
                                if (name_ptr >= 0 && name_ptr + 6 <= count) {
                                    if (std::memcmp(&src[name_ptr], ".text", 5) == 0 && src[name_ptr + 5] == 0) {
                                        codeStart = off;
                                        codeEnd = off + len;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (codeEnd == 0) {
                        int minOff = count, maxEnd = 0; bool found = false;
                        for (int i = 0; i < shnum; i++) {
                            int ent = shoff + i * shentsz;
                            if (ent + 0x18 >= count) return false;
                            int type = int(LittleEndian::readInt32(&src[ent + 4]));
                            int flags = int(LittleEndian::readInt32(&src[ent + 0x08]));
                            int off = int(LittleEndian::readInt32(&src[ent + 0x10]));
                            int len = int(LittleEndian::readInt32(&src[ent + 0x14]));
                            if (type == 1 && (flags & SHF_EXECINSTR) && len >= 16) {
                                if (!found) { minOff = off; maxEnd = off + len; found = true; }
                                else {
                                    if (off < minOff) minOff = off;
                                    if (off + len > maxEnd) maxEnd = off + len;
                                }
                            }
                        }
                        if (found) { codeStart = minOff; codeEnd = maxEnd; }
                    }
                    if (codeEnd == 0) {
                        int phoff = int(LittleEndian::readInt32(&src[0x1C]));
                        int phentsz = int(LittleEndian::readInt16(&src[0x2A]));
                        int phnum = int(LittleEndian::readInt16(&src[0x2C]));
                        int minOff = count, maxEnd = 0; bool foundPH = false;
                        for (int i = 0; i < phnum; i++) {
                            int ent = phoff + i * phentsz;
                            if (ent + 0x20 >= count) break; // sizeof(Elf32_Phdr)
                            int p_type  = int(LittleEndian::readInt32(&src[ent + 0x00]));
                            int p_flags = int(LittleEndian::readInt32(&src[ent + 0x18]));
                            if (p_type == 1 && (p_flags & 0x1)) {
                                int off = int(LittleEndian::readInt32(&src[ent + 0x04]));
                                int filesz = int(LittleEndian::readInt32(&src[ent + 0x10]));
                                int end = off + filesz;
                                if (!foundPH) { minOff = off; maxEnd = end; foundPH = true; }
                                else { if (off < minOff) minOff = off; if (end > maxEnd) maxEnd = end; }
                            }
                        }
                        if (foundPH) { codeStart = minOff; codeEnd = maxEnd; }
                    }
                    arch = LittleEndian::readInt16(&src[18]);
                }
            } else {
                if (src[4] == byte(2)) {
                    // ELF64 big-endian
                    int shnum = int(BigEndian::readInt16(&src[0x3C]));
                    int shentsz = int(BigEndian::readInt16(&src[0x3A]));
                    int shoff = int(BigEndian::readLong64(&src[0x28]));
                    int shstrndx = int(BigEndian::readInt16(&src[0x3E]));

                    int shstr_hdr = shoff + shstrndx * shentsz;
                    if (shstr_hdr + 0x28 < count) {
                        int shstr_off = int(BigEndian::readLong64(&src[shstr_hdr + 0x18]));
                        for (int i = 0; i < shnum; i++) {
                            int ent = shoff + i * shentsz;
                            if (ent + 0x28 >= count) return false;
                            int type = int(BigEndian::readInt32(&src[ent + 4]));
                            int name_off = int(BigEndian::readInt32(&src[ent + 0x00]));
                            int off = int(BigEndian::readLong64(&src[ent + 0x18]));
                            int len = int(BigEndian::readLong64(&src[ent + 0x20]));
                            if (type == 1 && len >= 16) {
                                int name_ptr = shstr_off + name_off;
                                if (name_ptr >= 0 && name_ptr + 6 <= count) {
                                    if (std::memcmp(&src[name_ptr], ".text", 5) == 0 && src[name_ptr + 5] == 0) {
                                        codeStart = off;
                                        codeEnd = off + len;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (codeEnd == 0) {
                        int minOff = count, maxEnd = 0; bool found = false;
                        for (int i = 0; i < shnum; i++) {
                            int ent = shoff + i * shentsz;
                            if (ent + 0x28 >= count) return false;
                            int type = int(BigEndian::readInt32(&src[ent + 4]));
                            int64_t flags = (int64_t)BigEndian::readLong64(&src[ent + 0x08]);
                            int off = int(BigEndian::readLong64(&src[ent + 0x18]));
                            int len = int(BigEndian::readLong64(&src[ent + 0x20]));
                            if (type == 1 && (flags & SHF_EXECINSTR) && len >= 16) {
                                if (!found) { minOff = off; maxEnd = off + len; found = true; }
                                else {
                                    if (off < minOff) minOff = off;
                                    if (off + len > maxEnd) maxEnd = off + len;
                                }
                            }
                        }
                        if (found) { codeStart = minOff; codeEnd = maxEnd; }
                    }
                    if (codeEnd == 0) {
                        int phoff = int(BigEndian::readLong64(&src[0x20]));
                        int phentsz = int(BigEndian::readInt16(&src[0x36]));
                        int phnum = int(BigEndian::readInt16(&src[0x38]));
                        int minOff = count, maxEnd = 0; bool foundPH = false;
                        for (int i = 0; i < phnum; i++) {
                            int ent = phoff + i * phentsz;
                            if (ent + 0x38 >= count) break;
                            int p_type  = int(BigEndian::readInt32(&src[ent + 0x00]));
                            int p_flags = int(BigEndian::readInt32(&src[ent + 0x04]));
                            if (p_type == 1 && (p_flags & 0x1)) {
                                int off = int(BigEndian::readLong64(&src[ent + 0x08]));
                                int filesz = int(BigEndian::readLong64(&src[ent + 0x20]));
                                int end = off + filesz;
                                if (!foundPH) { minOff = off; maxEnd = end; foundPH = true; }
                                else { if (off < minOff) minOff = off; if (end > maxEnd) maxEnd = end; }
                            }
                        }
                        if (foundPH) { codeStart = minOff; codeEnd = maxEnd; }
                    }
                    arch = BigEndian::readInt16(&src[18]);
                } else {
                    // ELF32 big-endian
                    int shnum = int(BigEndian::readInt16(&src[0x30]));
                    int shentsz = int(BigEndian::readInt16(&src[0x2E]));
                    int shoff = int(BigEndian::readInt32(&src[0x20]));
                    int shstrndx = int(BigEndian::readInt16(&src[0x32]));

                    int shstr_hdr = shoff + shstrndx * shentsz;
                    if (shstr_hdr + 0x18 < count) {
                        int shstr_off = int(BigEndian::readInt32(&src[shstr_hdr + 0x10]));
                        for (int i = 0; i < shnum; i++) {
                            int ent = shoff + i * shentsz;
                            if (ent + 0x18 >= count) return false;
                            int type = int(BigEndian::readInt32(&src[ent + 4]));
                            int name_off = int(BigEndian::readInt32(&src[ent + 0x00]));
                            int off = int(BigEndian::readInt32(&src[ent + 0x10]));
                            int len = int(BigEndian::readInt32(&src[ent + 0x14]));
                            if (type == 1 && len >= 16) {
                                int name_ptr = shstr_off + name_off;
                                if (name_ptr >= 0 && name_ptr + 6 <= count) {
                                    if (std::memcmp(&src[name_ptr], ".text", 5) == 0 && src[name_ptr + 5] == 0) {
                                        codeStart = off;
                                        codeEnd = off + len;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (codeEnd == 0) {
                        int minOff = count, maxEnd = 0; bool found = false;
                        for (int i = 0; i < shnum; i++) {
                            int ent = shoff + i * shentsz;
                            if (ent + 0x18 >= count) return false;
                            int type = int(BigEndian::readInt32(&src[ent + 4]));
                            int flags = int(BigEndian::readInt32(&src[ent + 0x08]));
                            int off = int(BigEndian::readInt32(&src[ent + 0x10]));
                            int len = int(BigEndian::readInt32(&src[ent + 0x14]));
                            if (type == 1 && (flags & SHF_EXECINSTR) && len >= 16) {
                                if (!found) { minOff = off; maxEnd = off + len; found = true; }
                                else {
                                    if (off < minOff) minOff = off;
                                    if (off + len > maxEnd) maxEnd = off + len;
                                }
                            }
                        }
                        if (found) { codeStart = minOff; codeEnd = maxEnd; }
                    }
                    if (codeEnd == 0) {
                        int phoff = int(BigEndian::readInt32(&src[0x1C]));
                        int phentsz = int(BigEndian::readInt16(&src[0x2A]));
                        int phnum = int(BigEndian::readInt16(&src[0x2C]));
                        int minOff = count, maxEnd = 0; bool foundPH = false;
                        for (int i = 0; i < phnum; i++) {
                            int ent = phoff + i * phentsz;
                            if (ent + 0x20 >= count) break;
                            int p_type  = int(BigEndian::readInt32(&src[ent + 0x00]));
                            int p_flags = int(BigEndian::readInt32(&src[ent + 0x18]));
                            if (p_type == 1 && (p_flags & 0x1)) {
                                int off = int(BigEndian::readInt32(&src[ent + 0x04]));
                                int filesz = int(BigEndian::readInt32(&src[ent + 0x10]));
                                int end = off + filesz;
                                if (!foundPH) { minOff = off; maxEnd = end; foundPH = true; }
                                else { if (off < minOff) minOff = off; if (end > maxEnd) maxEnd = end; }
                            }
                        }
                        if (foundPH) { codeStart = minOff; codeEnd = maxEnd; }
                    }
                    arch = BigEndian::readInt16(&src[18]);
                }
            }

            codeStart = min(codeStart, count);
            codeEnd = min(codeEnd, count);
            return true;
        }
    }
 else if ((magic == Magic::MAC_MAGIC32) || (magic == Magic::MAC_CIGAM32) ||
               (magic == Magic::MAC_MAGIC64) || (magic == Magic::MAC_CIGAM64)) {

        bool is64Bits = (magic == Magic::MAC_MAGIC64) || (magic == Magic::MAC_CIGAM64);
        codeStart = 0;
        static char MAC_TEXT_SEGMENT[] = "__TEXT";
        static char MAC_TEXT_SECTION[] = "__text";

        if (count >= 64) {
            int type = LittleEndian::readInt32(&src[12]);

            if (type != MAC_MH_EXECUTE)
                return false;

            arch = LittleEndian::readInt32(&src[4]);
            int nbCmds = LittleEndian::readInt32(&src[0x10]);
            int pos = (is64Bits == true) ? 0x20 : 0x1C;
            int cmd = 0;

            while (cmd < nbCmds) {
                int ldCmd = LittleEndian::readInt32(&src[pos]);
                int szCmd = LittleEndian::readInt32(&src[pos + 4]);
                int szSegHdr = (is64Bits == true) ? 0x48 : 0x38;

                if ((ldCmd == MAC_LC_SEGMENT) || (ldCmd == MAC_LC_SEGMENT64)) {
                    if (pos + 14 >= count)
                        return false;

                    if (memcmp(&src[pos + 8], reinterpret_cast<const byte*>(MAC_TEXT_SEGMENT), 6) == 0) {
                        int posSection = pos + szSegHdr;

                        if (posSection + 0x34 >= count)
                            return false;

                        if (memcmp(&src[posSection], reinterpret_cast<const byte*>(MAC_TEXT_SECTION), 6) == 0) {
                            // Text section in TEXT segment
                            if (is64Bits == true) {
                                codeStart = int(LittleEndian::readLong64(&src[posSection + 0x30]));
                                codeEnd = codeStart + LittleEndian::readInt32(&src[posSection + 0x28]);
                                break;
                            } else {
                                codeStart = LittleEndian::readInt32(&src[posSection + 0x2C]);
                                codeEnd = codeStart + LittleEndian::readInt32(&src[posSection + 0x28]);
                                break;
                            }
                        }
                    }
                }

                cmd++;
                pos += szCmd;
            }

            codeStart = min(codeStart, count);
            codeEnd = min(codeEnd, count);
            return true;
        }
    }

    return false;
}

} // namespace kanzi
