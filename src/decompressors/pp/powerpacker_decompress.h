/*
 * PowerPacker Decompressor - Standalone C Implementation
 * Based on pplib 1.2 by Stuart Caie (Public Domain)
 *
 * PowerPacker format: PP20 (popular on Amiga in late 80s/early 90s)
 *
 * This is a minimal standalone decompressor for integration into
 * ELF packers or embedded systems.
 */

#ifndef POWERPACKER_DECOMPRESS_H
#define POWERPACKER_DECOMPRESS_H

#include <stdint.h>

/*
 * Decompress PowerPacker data
 *
 * Parameters:
 *   src: pointer to compressed data (PP20 format)
 *   src_len: length of compressed data
 *   dest: pointer to output buffer
 *   dest_len: size of output buffer
 *
 * Returns:
 *   > 0: number of bytes decompressed (success)
 *   0: decompression error (buffer overflow, format error, etc.)
 *
 * Note: The PowerPacker format stores the decompressed size at the end
 *       of the compressed data (last 4 bytes before efficiency table)
 */
int powerpacker_decompress(const unsigned char *src, unsigned int src_len,
                           unsigned char *dest, unsigned int dest_len);

/*
 * Get the decompressed size from PowerPacker data
 *
 * Parameters:
 *   src: pointer to compressed data (PP20 format)
 *   src_len: length of compressed data
 *
 * Returns:
 *   Decompressed size in bytes, or 0 on error
 */
unsigned int powerpacker_get_decompressed_size(const unsigned char *src,
                                                unsigned int src_len);

#endif /* POWERPACKER_DECOMPRESS_H */
