#include "snappy_decompress.h"
#include <string.h>

/* Read varint-encoded uncompressed length */
static int read_uncompressed_length(const unsigned char* src, size_t src_len,
                                    size_t* length, size_t* bytes_read) {
    size_t result = 0;
    int shift = 0;
    size_t i;

    for (i = 0; i < src_len && i < 5; i++) {
        unsigned char c = src[i];
        result |= (size_t)(c & 0x7F) << shift;

        if ((c & 0x80) == 0) {
            *length = result;
            *bytes_read = i + 1;
            return 1; /* Success */
        }
        shift += 7;
    }

    return 0; /* Invalid varint */
}

/* Copy with overlap support (for RLE-style copies) */
static void copy_with_overlap(unsigned char* dst, const unsigned char* src,
                              size_t length) {
    while (length-- > 0) {
        *dst++ = *src++;
    }
}

/* Tiny memcpy without vectorized over-reads; used to avoid libc memcpy */
static void tiny_memcpy(unsigned char* dst, const unsigned char* src, size_t length) {
    while (length-- > 0) {
        *dst++ = *src++;
    }
}

/*
 * Decompress Snappy-compressed data.
 *
 * src: compressed data
 * src_len: compressed data size
 * dst: output buffer (must be large enough)
 * dst_len: output buffer size
 *
 * Returns: number of decompressed bytes, or 0 on error
 */
size_t snappy_decompress_simple(const unsigned char* src, size_t src_len,
                                unsigned char* dst, size_t dst_len) {
    size_t uncompressed_len;
    size_t varint_bytes;
    size_t src_pos;
    size_t dst_pos = 0;

    if (!src || !dst || src_len == 0) {
        return 0;
    }

    /* Read uncompressed length from header */
    if (!read_uncompressed_length(src, src_len, &uncompressed_len, &varint_bytes)) {
        return 0;
    }

    if (uncompressed_len > dst_len) {
        return 0; /* Output buffer too small */
    }

    src_pos = varint_bytes;

    /* Decompress loop */
    while (src_pos < src_len) {
        unsigned char tag = src[src_pos++];
        unsigned int tag_type = tag & 0x03;

        if (tag_type == 0x00) {
            /* Literal */
            size_t literal_len = (tag >> 2) + 1;

            if (literal_len > 60) {
                /* Long literal */
                size_t extra_bytes = literal_len - 60;
                if (src_pos + extra_bytes > src_len) return 0;

                literal_len = 0;
                for (size_t i = 0; i < extra_bytes; i++) {
                    literal_len |= (size_t)src[src_pos++] << (i * 8);
                }
                literal_len += 1;
            }

            if (src_pos + literal_len > src_len) return 0;
            if (dst_pos + literal_len > dst_len) return 0;

            tiny_memcpy(dst + dst_pos, src + src_pos, literal_len);
            src_pos += literal_len;
            dst_pos += literal_len;
            if (dst_pos == uncompressed_len) break;

        } else if (tag_type == 0x01) {
            /* Copy with 1-byte offset */
            if (src_pos >= src_len) return 0;

            size_t length = ((tag >> 2) & 0x07) + 4;
            size_t offset = ((tag & 0xE0) << 3) | src[src_pos++];

            if (offset == 0 || offset > dst_pos) return 0;
            if (dst_pos + length > dst_len) return 0;

            copy_with_overlap(dst + dst_pos, dst + dst_pos - offset, length);
            dst_pos += length;
            if (dst_pos == uncompressed_len) break;

        } else if (tag_type == 0x02) {
            /* Copy with 2-byte offset */
            if (src_pos + 1 >= src_len) return 0;

            size_t length = (tag >> 2) + 1;
            size_t offset = src[src_pos] | (src[src_pos + 1] << 8);
            src_pos += 2;

            if (offset == 0 || offset > dst_pos) return 0;
            if (dst_pos + length > dst_len) return 0;

            copy_with_overlap(dst + dst_pos, dst + dst_pos - offset, length);
            dst_pos += length;

        } else {
            /* Copy with 4-byte offset */
            if (src_pos + 3 >= src_len) return 0;

            size_t length = (tag >> 2) + 1;
            size_t offset = src[src_pos] | (src[src_pos + 1] << 8) |
                          (src[src_pos + 2] << 16) | (src[src_pos + 3] << 24);
            src_pos += 4;

            if (offset == 0 || offset > dst_pos) return 0;
            if (dst_pos + length > dst_len) return 0;

            copy_with_overlap(dst + dst_pos, dst + dst_pos - offset, length);
            dst_pos += length;
        }
    }

    if (dst_pos != uncompressed_len) {
        return 0; /* Decompression mismatch */
    }

    return dst_pos;
}

/*
 * Get uncompressed size from Snappy header.
 *
 * src: compressed data
 * src_len: compressed data size
 *
 * Returns: uncompressed size, or 0 on error
 */
size_t snappy_get_uncompressed_length(const unsigned char* src, size_t src_len) {
    size_t length;
    size_t bytes_read;

    if (!src || src_len == 0) {
        return 0;
    }

    if (read_uncompressed_length(src, src_len, &length, &bytes_read)) {
        return length;
    }

    return 0;
}
