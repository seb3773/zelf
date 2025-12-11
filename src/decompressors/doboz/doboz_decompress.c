/*
 * Doboz Decompressor - Optimized C Implementation V2
 * Conservative optimizations with safety first
 */

#include <stdint.h>
#include <string.h>
#include <assert.h>

// Constants
#define WORD_SIZE 4
#define MIN_MATCH_LENGTH 3
#define MAX_MATCH_LENGTH 258
#define TAIL_LENGTH 8
#define VERSION 0

// Result codes
typedef enum {
    DOBOZ_OK = 0,
    DOBOZ_ERROR_BUFFER_TOO_SMALL = 1,
    DOBOZ_ERROR_CORRUPTED_DATA = 2,
    DOBOZ_ERROR_UNSUPPORTED_VERSION = 3
} doboz_result_t;

// Internal structures
typedef struct {
    int length;
    int offset;
} doboz_match_t;

typedef struct {
    uint64_t uncompressedSize;
    uint64_t compressedSize;
    int version;
    int isStored;
} doboz_header_t;

// Fast read/write functions (same as baseline but with better inlining)
static inline uint32_t fast_read_v2(const void* src, size_t size) {
    switch (size) {
        case 4: return *(const uint32_t*)src;
        case 3: return *(const uint32_t*)src;
        case 2: return *(const uint16_t*)src;
        case 1: return *(const uint8_t*)src;
        default: return 0;
    }
}

static inline void fast_write_v2(void* dst, uint32_t word, size_t size) {
    switch (size) {
        case 4: *(uint32_t*)dst = word; break;
        case 3: *(uint32_t*)dst = word; break;
        case 2: *(uint16_t*)dst = (uint16_t)word; break;
        case 1: *(uint8_t*)dst = (uint8_t)word; break;
    }
}

// Match decoding (same as baseline)
static inline int decode_match_v2(doboz_match_t* match, const void* source) {
    static const struct {
        uint32_t mask;
        uint8_t offsetShift;
        uint8_t lengthMask;
        uint8_t lengthShift;
        int8_t size;
    } lut[] = {
        {0xff,        2,   0, 0, 1}, // (0)00
        {0xffff,      2,   0, 0, 2}, // (0)01
        {0xffff,      6,  15, 2, 2}, // (0)10
        {0xffffff,    8,  31, 3, 3}, // (0)11
        {0xff,        2,   0, 0, 1}, // (1)00 = (0)00
        {0xffff,      2,   0, 0, 2}, // (1)01 = (0)01
        {0xffff,      6,  15, 2, 2}, // (1)10 = (0)10
        {0xffffffff, 11, 255, 3, 4}, // 111
    };

    uint32_t word = fast_read_v2(source, WORD_SIZE);
    uint32_t i = word & 7;
    
    match->offset = (int)((word & lut[i].mask) >> lut[i].offsetShift);
    match->length = (int)(((word >> lut[i].lengthShift) & lut[i].lengthMask) + MIN_MATCH_LENGTH);
    
    return lut[i].size;
}

// Header decoding (same as baseline)
static doboz_result_t decode_header_v2(doboz_header_t* header, const void* source, size_t sourceSize, int* headerSize) {
    const uint8_t* input = (const uint8_t*)source;
    
    if (sourceSize < 1) return DOBOZ_ERROR_BUFFER_TOO_SMALL;
    
    uint32_t attributes = *input++;
    header->version = attributes & 7;
    int sizeCodedSize = ((attributes >> 3) & 7) + 1;
    
    *headerSize = 1 + 2 * sizeCodedSize;
    
    if (sourceSize < (size_t)*headerSize) return DOBOZ_ERROR_BUFFER_TOO_SMALL;
    
    header->isStored = (attributes & 128) != 0;
    
    // Decode sizes
    switch (sizeCodedSize) {
        case 1:
            header->uncompressedSize = *(const uint8_t*)input;
            header->compressedSize = *(const uint8_t*)(input + sizeCodedSize);
            break;
        case 2:
            header->uncompressedSize = *(const uint16_t*)input;
            header->compressedSize = *(const uint16_t*)(input + sizeCodedSize);
            break;
        case 4:
            header->uncompressedSize = *(const uint32_t*)input;
            header->compressedSize = *(const uint32_t*)(input + sizeCodedSize);
            break;
        case 8:
            header->uncompressedSize = *(const uint64_t*)input;
            header->compressedSize = *(const uint64_t*)(input + sizeCodedSize);
            break;
        default:
            return DOBOZ_ERROR_CORRUPTED_DATA;
    }
    
    return DOBOZ_OK;
}

// Main decompression function with minimal optimizations
int doboz_decompress_v2(const char* src, char* dst, int srcSize, int dstCapacity) {
    if (!src || !dst || srcSize <= 0 || dstCapacity <= 0) return -1;
    
    const uint8_t* inputBuffer = (const uint8_t*)src;
    const uint8_t* inputIterator = inputBuffer;
    uint8_t* outputBuffer = (uint8_t*)dst;
    uint8_t* outputIterator = outputBuffer;
    
    // Decode header
    doboz_header_t header;
    int headerSize;
    doboz_result_t result = decode_header_v2(&header, src, srcSize, &headerSize);
    if (result != DOBOZ_OK) return -1;
    
    inputIterator += headerSize;
    
    if (header.version != VERSION) return -1;
    
    // Check buffer sizes
    if (srcSize < (int)header.compressedSize || dstCapacity < (int)header.uncompressedSize) {
        return -1;
    }
    
    size_t uncompressedSize = (size_t)header.uncompressedSize;
    
    // If data is stored (not compressed), just copy
    if (header.isStored) {
        memcpy(outputBuffer, inputIterator, uncompressedSize);
        return (int)uncompressedSize;
    }
    
    const uint8_t* inputEnd = inputBuffer + (size_t)header.compressedSize;
    uint8_t* outputEnd = outputBuffer + uncompressedSize;
    uint8_t* outputTail = (uncompressedSize > TAIL_LENGTH) ? (outputEnd - TAIL_LENGTH) : outputBuffer;
    
    uint32_t controlWord = 1;
    
    // Main decoding loop
    for (;;) {
        // Check if we have enough input data
        if (inputIterator + 2 * WORD_SIZE > inputEnd) return -1;
        
        // Read control word if needed
        if (controlWord == 1) {
            controlWord = fast_read_v2(inputIterator, WORD_SIZE);
            inputIterator += WORD_SIZE;
        }
        
        // Check if it's a literal or match
        if ((controlWord & 1) == 0) {
            // It's a literal
            if (outputIterator < outputTail) {
                // Fast path: copy up to 4 literals
                fast_write_v2(outputIterator, fast_read_v2(inputIterator, WORD_SIZE), WORD_SIZE);
                
                // Get run length from lookup table
                static const int8_t literalRunLengthTable[16] = {4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};
                int runLength = literalRunLengthTable[controlWord & 0xf];
                
                inputIterator += runLength;
                outputIterator += runLength;
                controlWord >>= runLength;
            } else {
                // Tail processing: copy literals one by one
                while (outputIterator < outputEnd) {
                    if (inputIterator + WORD_SIZE + 1 > inputEnd) return -1;
                    
                    if (controlWord == 1) {
                        controlWord = fast_read_v2(inputIterator, WORD_SIZE);
                        inputIterator += WORD_SIZE;
                    }
                    
                    *outputIterator++ = *inputIterator++;
                    controlWord >>= 1;
                }
                return (int)uncompressedSize;
            }
        } else {
            // It's a match
            doboz_match_t match;
            inputIterator += decode_match_v2(&match, inputIterator);
            
            // Copy matched string
            uint8_t* matchString = outputIterator - match.offset;
            
            if (matchString < outputBuffer || outputIterator + match.length > outputTail) {
                return -1;
            }
            
            int i = 0;
            
            // Handle overlapping matches
            if (match.offset < WORD_SIZE) {
                // Copy first 3 bytes one by one for small offsets
                do {
                    fast_write_v2(outputIterator + i, fast_read_v2(matchString + i, 1), 1);
                    ++i;
                } while (i < 3);
                
                // Adjust match string for fast copying
                matchString -= 2 + (match.offset & 1);
            }
            
            // Fast copying in word-sized chunks
            do {
                fast_write_v2(outputIterator + i, fast_read_v2(matchString + i, WORD_SIZE), WORD_SIZE);
                i += WORD_SIZE;
            } while (i < match.length);
            
            outputIterator += match.length;
            controlWord >>= 1;
        }
    }
}
