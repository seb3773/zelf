#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t *words;
    size_t capacity;
    size_t count;
    uint16_t current_word;
    int current_bit_count;
    int first_word_bits;
} BitStream;

void bitstream_init(BitStream *bs);
void bitstream_free(BitStream *bs);
void bitstream_put_bit(BitStream *bs, int bit);
void bitstream_put_bits(BitStream *bs, uint32_t value, int count);
void bitstream_flush(BitStream *bs);
size_t bitstream_get_size_bytes(BitStream *bs);
void bitstream_write_to_buffer(BitStream *bs, uint8_t *buffer);

#endif
