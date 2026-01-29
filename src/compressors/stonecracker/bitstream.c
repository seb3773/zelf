#include "bitstream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void bitstream_init(BitStream *bs) {
    bs->capacity = 1024;
    bs->words = (uint16_t*)malloc(bs->capacity * sizeof(uint16_t));
    bs->count = 0;
    bs->current_word = 0;
    bs->current_bit_count = 0;
    bs->first_word_bits = 0;
}

void bitstream_free(BitStream *bs) {
    free(bs->words);
}

static void bitstream_push_word(BitStream *bs, uint16_t word) {
    if (bs->count == bs->capacity) {
        bs->capacity *= 2;
        bs->words = (uint16_t*)realloc(bs->words, bs->capacity * sizeof(uint16_t));
    }
    bs->words[bs->count++] = word;
}

void bitstream_put_bit(BitStream *bs, int bit) {
    if (bit) {
        bs->current_word |= (1 << bs->current_bit_count);
    }
    bs->current_bit_count++;
    if (bs->current_bit_count == 16) {
        if (bs->count == 0) bs->first_word_bits = 16;
        bitstream_push_word(bs, bs->current_word);
        bs->current_word = 0;
        bs->current_bit_count = 0;
    }
}

void bitstream_put_bits(BitStream *bs, uint32_t value, int count) {
    for (int i = 0; i < count; i++) {
        bitstream_put_bit(bs, (value >> i) & 1);
    }
}

void bitstream_flush(BitStream *bs) {
    if (bs->current_bit_count > 0) {
        if (bs->count == 0) bs->first_word_bits = bs->current_bit_count;
        bitstream_push_word(bs, bs->current_word);
    }
}

size_t bitstream_get_size_bytes(BitStream *bs) {
    // Total size = (Num Words) * 2 + 2 (for bit count)
    // If current_bit_count > 0, it's already pushed by flush
    return bs->count * 2 + 2;
}

void bitstream_write_to_buffer(BitStream *bs, uint8_t *buffer) {
    // Write words in REVERSE order
    // [Word Z] ... [Word B] [Word A] [BitCount]
    // Word A is words[0]
    // Word Z is words[count-1]
    
    size_t offset = 0;
    for (size_t i = 0; i < bs->count; i++) {
        // Write words[count - 1 - i]
        uint16_t w = bs->words[bs->count - 1 - i];
        buffer[offset++] = (w >> 8) & 0xFF;
        buffer[offset++] = w & 0xFF;
    }
    
    // Write BitCount of the FIRST word (words[0])
    // If we have words, words[0] is the first one filled.
    // If we have >1 words, words[0] must be full (16 bits).
    // If we have 1 word, words[0] has current_bit_count (before flush) or 16?
    // Wait, flush pushes the last word.
    // If we have >1 words, words[0] was pushed long ago, so it was full (16).
    // If we have 1 word, it was pushed by flush. Its valid bits are what was in current_bit_count before flush.
    // But we lost that info? No, we need to track it.
    
    // Actually, the decompressor reads the LAST 2 bytes as BitCount.
    // And uses it for the FIRST word read (which is the LAST word in the file, i.e. words[0]).
    
    uint16_t bit_count = bs->first_word_bits;
    
    buffer[offset++] = (bit_count >> 8) & 0xFF;
    buffer[offset++] = bit_count & 0xFF;
}
