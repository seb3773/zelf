/*
 * ZX7b Decompressor Header
 * Ultra-compact decompression for ZX7b format
 *
 * Based on ZX7 by Einar Saukas / Antonio Villena
 * C adaptation for DecompLibs project & zELF packer by Seb3773
 *
 * Target size: ~150 bytes (C version)
 * Original ASM: 64-191 bytes depending on optimization level
 */

#ifndef ZX7B_DECOMPRESS_H
#define ZX7B_DECOMPRESS_H

/*
 * Decompress ZX7b format data
 *
 * IMPORTANT: ZX7b uses BACKWARDS decompression!
 * - Input data is processed from END to START
 * - Output data is written from END to START
 * - This function handles the reversal internally for standard forward buffers
 *
 * Parameters:
 *   in:       Compressed input data (forward buffer)
 *   in_size:  Size of compressed data
 *   out:      Output buffer (forward buffer)
 *   out_max:  Maximum output size
 *
 * Returns:
 *   Decompressed size on success, -1 on error
 */
int zx7b_decompress(const unsigned char *in, int in_size,
                    unsigned char *out, int out_max);

#endif /* ZX7B_DECOMPRESS_H */
