/*
 * Exomizer Decompressor - Optimized versions for modern CPUs
 * 
 * VERSIONS AVAILABLE:
 * 
 * - exo_baseline.c (+0%, 114.8 MB/s)
 *   Original implementation, no optimizations
 * 
 * - exo_test_a.c (+15.1%, 132.1 MB/s)
 *   Basic optimizations: inline functions, restrict pointers
 * 
 * - exo_final.c (+10.3%, 126.6 MB/s)
 *   Prefetch + inline optimizations
 * 
 * - exo_test_f.c (+5.7%, 121.3 MB/s)
 *   memcpy for literals >= 16, uint64_t copies, prefetch
 * 
 * - exo_test_n.c (+11.5%, 128.0 MB/s)
 *   memcpy threshold lowered to >= 8 bytes
 * 
 * - exo_test_q.c (+22.8%, 141.0 MB/s) ⭐ BEST
 *   Smart copies: memset RLE (offset=1), REP MOVSB for overlaps,
 *   unrolled small copies, memcpy >= 8 bytes
 * 
 * USAGE:
 *   #include "exo_decompress.h"
 *   
 *   int result = exo_decompress(compressed_data, compressed_size,
 *                                output_buffer, output_buffer_size);
 *   if (result < 0) { error handling }
 *   // result = decompressed size
 * 
 * COMPILE:
 *   gcc -O3 -march=native -flto -fno-strict-aliasing your_code.c exo_test_q.c
 */

#ifndef EXO_DECOMPRESS_H
#define EXO_DECOMPRESS_H

/**
 * Decompress Exomizer raw format data
 * 
 * @param in        Compressed input buffer
 * @param in_size   Size of compressed data
 * @param out       Output buffer for decompressed data
 * @param out_max   Maximum size of output buffer
 * @return          Decompressed size on success, negative on error
 */
int exo_decompress(const unsigned char *in, int in_size,
                   unsigned char *out, int out_max);

#endif
