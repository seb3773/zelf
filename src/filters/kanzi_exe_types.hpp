/**
 * kanzi_exe_types.hpp - Type definitions for Kanzi EXE filter
 * Extracted from Kanzi-cpp project
 *
 * Copyright 2011-2025 Frederic Langlet
 * Licensed under the Apache License, Version 2.0
 */

#pragma once
#ifndef _KANZI_EXE_TYPES_
#define _KANZI_EXE_TYPES_

#include <stdint.h>
#include <cstring>

namespace kanzi {

// Type definitions
    typedef uint8_t byte;

typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int64_t int64;

// Little Endian read/write functions
class LittleEndian {
public:
    static inline int32_t readInt32(const byte* p) {
        return static_cast<int32_t>(
            static_cast<uint32_t>(p[0]) |
            (static_cast<uint32_t>(p[1]) << 8) |
            (static_cast<uint32_t>(p[2]) << 16) |
            (static_cast<uint32_t>(p[3]) << 24)
        );
    }

    static inline void writeInt32(byte* p, int32_t v) {
        p[0] = static_cast<byte>(v);
        p[1] = static_cast<byte>(v >> 8);
        p[2] = static_cast<byte>(v >> 16);
        p[3] = static_cast<byte>(v >> 24);
    }

    static inline int16_t readInt16(const byte* p) {
        return static_cast<int16_t>(
            static_cast<uint16_t>(p[0]) |
            (static_cast<uint16_t>(p[1]) << 8)
        );
    }

    static inline void writeInt16(byte* p, int16_t v) {
        p[0] = static_cast<byte>(v);
        p[1] = static_cast<byte>(v >> 8);
    }

    static inline int64_t readLong64(const byte* p) {
        return static_cast<int64_t>(
            static_cast<uint64_t>(p[0]) |
            (static_cast<uint64_t>(p[1]) << 8) |
            (static_cast<uint64_t>(p[2]) << 16) |
            (static_cast<uint64_t>(p[3]) << 24) |
            (static_cast<uint64_t>(p[4]) << 32) |
            (static_cast<uint64_t>(p[5]) << 40) |
            (static_cast<uint64_t>(p[6]) << 48) |
            (static_cast<uint64_t>(p[7]) << 56)
        );
    }

    static inline void writeLong64(byte* p, int64_t v) {
        p[0] = static_cast<byte>(v);
        p[1] = static_cast<byte>(v >> 8);
        p[2] = static_cast<byte>(v >> 16);
        p[3] = static_cast<byte>(v >> 24);
        p[4] = static_cast<byte>(v >> 32);
        p[5] = static_cast<byte>(v >> 40);
        p[6] = static_cast<byte>(v >> 48);
        p[7] = static_cast<byte>(v >> 56);
    }
};

// Big Endian read/write functions
class BigEndian {
public:
    static inline int32_t readInt32(const byte* p) {
        return static_cast<int32_t>(
            (static_cast<uint32_t>(p[0]) << 24) |
            (static_cast<uint32_t>(p[1]) << 16) |
            (static_cast<uint32_t>(p[2]) << 8) |
            static_cast<uint32_t>(p[3])
        );
    }

    static inline void writeInt32(byte* p, int32_t v) {
        p[0] = static_cast<byte>(v >> 24);
        p[1] = static_cast<byte>(v >> 16);
        p[2] = static_cast<byte>(v >> 8);
        p[3] = static_cast<byte>(v);
    }

    static inline int16_t readInt16(const byte* p) {
        return static_cast<int16_t>(
            (static_cast<uint16_t>(p[0]) << 8) |
            static_cast<uint16_t>(p[1])
        );
    }

    static inline void writeInt16(byte* p, int16_t v) {
        p[0] = static_cast<byte>(v >> 8);
        p[1] = static_cast<byte>(v);
    }

    static inline int64_t readLong64(const byte* p) {
        return static_cast<int64_t>(
            (static_cast<uint64_t>(p[0]) << 56) |
            (static_cast<uint64_t>(p[1]) << 48) |
            (static_cast<uint64_t>(p[2]) << 40) |
            (static_cast<uint64_t>(p[3]) << 32) |
            (static_cast<uint64_t>(p[4]) << 24) |
            (static_cast<uint64_t>(p[5]) << 16) |
            (static_cast<uint64_t>(p[6]) << 8) |
            static_cast<uint64_t>(p[7])
        );
    }

    static inline void writeLong64(byte* p, int64_t v) {
        p[0] = static_cast<byte>(v >> 56);
        p[1] = static_cast<byte>(v >> 48);
        p[2] = static_cast<byte>(v >> 40);
        p[3] = static_cast<byte>(v >> 32);
        p[4] = static_cast<byte>(v >> 24);
        p[5] = static_cast<byte>(v >> 16);
        p[6] = static_cast<byte>(v >> 8);
        p[7] = static_cast<byte>(v);
    }
};

// min/max templates
template<typename T>
inline T min(T a, T b) { return (a < b) ? a : b; }

template<typename T>
inline T max(T a, T b) { return (a > b) ? a : b; }

} // namespace kanzi

#endif // _KANZI_EXE_TYPES_
