#include <stdint.h>
#include <stddef.h>
#include "codec_lz4.h"

// Tiny byte-copy helper (safe for overlap, smaller code than multi-path memcpy)
static inline void lz4_copy_bytes(uint8_t* d, const uint8_t* s, int n) {
    while (n-- > 0) *d++ = *s++;
}

static inline uint16_t LZ4_read16(const void* memPtr) {
    return *(const uint16_t*)memPtr;
}

#define MINMATCH 4
#define MFLIMIT 12
#define LASTLITERALS 5

int lz4_decompress(const char* src, char* dst, int compressedSize, int dstCapacity) {
    const uint8_t* ip = (const uint8_t*)src;
    const uint8_t* const iend = ip + compressedSize;
    uint8_t* op = (uint8_t*)dst;
    uint8_t* const oend = op + dstCapacity;

    if (compressedSize <= 0 || dstCapacity <= 0) return -1;

    for (;;) {
        if (ip >= iend) {
            return (int)(op - (uint8_t*)dst);
        }

        unsigned token = *ip++;
        int literalLength = token >> 4;

        if (literalLength == 15) {
            uint8_t len;
            do {
                if (ip >= iend) return -1;
                len = *ip++;
                literalLength += len;
            } while (len == 255);
        }

        uint8_t* const litEnd = op + literalLength;
        if (litEnd > oend) return -1;
        lz4_copy_bytes(op, ip, literalLength);
        ip += literalLength;
        op = litEnd;

        // last literals only (stream end)
        if (ip >= iend) return (int)(op - (uint8_t*)dst);

        if (ip + 2 > iend) return -1; // need offset
        uint16_t offset = LZ4_read16(ip); ip += 2;
        if (offset == 0) return -1;

        uint8_t* match = op - offset;
        if (match < (uint8_t*)dst) return -1;

        int matchLength = token & 0x0F;
        if (matchLength == 15) {
            uint8_t len;
            do {
                if (ip >= iend) return -1;
                len = *ip++;
                matchLength += len;
            } while (len == 255);
        }
        matchLength += MINMATCH;

        uint8_t* const matchEnd = op + matchLength;
        if (matchEnd > oend) return -1;

        // Always use byte-wise copy (safe for overlap, smaller code)
        lz4_copy_bytes(op, match, matchLength);
        op = matchEnd;
    }
}
