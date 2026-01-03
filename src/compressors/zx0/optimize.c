/*
 * (c) Copyright 2021 by Einar Saukas. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of its author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Force O3 optimization for compression (performance critical)
#pragma GCC optimize("O3")

#include <stdio.h>
#include <stdlib.h>

#include "zx0_internal.h"

#define MAX_SCALE 50

static inline int offset_ceiling(int index, int offset_limit) {
  return index > offset_limit     ? offset_limit
         : index < INITIAL_OFFSET ? INITIAL_OFFSET
                                  : index;
}

/* Optimized with GCC builtin for faster execution */
static inline int elias_gamma_bits(int value) {
#ifdef __GNUC__
  if (value <= 0)
    return 1;
  return ((32 - __builtin_clz(value)) << 1) - 1;
#else
  int bits = 1;
  while (value >>= 1)
    bits += 2;
  return bits;
#endif
}

/* Cache-friendly structure: group data accessed together */
typedef struct {
  BLOCK *last_literal;
  BLOCK *last_match;
  int match_length;
} OffsetData;

BLOCK *zx0_optimize(unsigned char *input_data, int input_size, int skip,
                    int offset_limit) {
  unsigned char *__restrict in = input_data;
  OffsetData *offset_data; /* grouped data for better cache locality */
  BLOCK **optimal;
  int *best_length;
  int best_length_size;
  int bits;
  int index;
  int offset;
  int length;
  int bits2;
  /* progress emission: update every ~0.5% */
  int step = input_size / 200;
  if (step <= 0)
    step = 1;
  int next_emit = step;
  zx0_emit_progress(0, (size_t)input_size);

  int max_offset = offset_ceiling(input_size - 1, offset_limit);

  /* allocate all main data structures - grouped for cache locality */
  offset_data =
      (OffsetData *)calloc((size_t)max_offset + 1, sizeof(OffsetData));
  optimal = (BLOCK **)calloc((size_t)input_size, sizeof(BLOCK *));
  best_length = (int *)malloc((size_t)input_size * sizeof(int));
  if (!offset_data || !optimal || !best_length) {
    /* Error: Insufficient memory */
    exit(1);
  }
  if (input_size > 2)
    best_length[2] = 2;

  /* start with fake block */
  zx0_assign(&offset_data[INITIAL_OFFSET].last_match,
             zx0_allocate(-1, skip - 1, INITIAL_OFFSET, NULL));

  /* process remaining bytes */
  for (index = skip; index < input_size; index++) {
    if (index >= next_emit) {
      zx0_emit_progress((size_t)index, (size_t)input_size);
      next_emit += step;
    }
    unsigned char current_byte = in[index]; /* Cache byte */
    const int can_match = (index != skip);
    best_length_size = 2;
    max_offset = offset_ceiling(index, offset_limit);
    for (offset = 1; offset <= max_offset; offset++) {
      OffsetData *od =
          &offset_data[offset]; /* Cache-friendly: single pointer */

      if (__builtin_expect(can_match && current_byte == in[index - offset], 0)) {
        /* copy from last offset */
        if (od->last_literal) {
          length = index - od->last_literal->index;
          bits = od->last_literal->bits + 1 + elias_gamma_bits(length);
          zx0_assign(&od->last_match,
                     zx0_allocate(bits, index, offset, od->last_literal));
          if (!optimal[index] || optimal[index]->bits > bits)
            zx0_assign(&optimal[index], od->last_match);
        }
        /* copy from new offset */
        if (++od->match_length > 1) {
          if (best_length_size < od->match_length) {
            bits = optimal[index - best_length[best_length_size]]->bits +
                   elias_gamma_bits(best_length[best_length_size] - 1);
            do {
              best_length_size++;
              bits2 = optimal[index - best_length_size]->bits +
                      elias_gamma_bits(best_length_size - 1);
              if (bits2 <= bits) {
                best_length[best_length_size] = best_length_size;
                bits = bits2;
              } else {
                best_length[best_length_size] =
                    best_length[best_length_size - 1];
              }
            } while (best_length_size < od->match_length);
          }
          length = best_length[od->match_length];
          int offset_msb = (offset - 1) >> 7; /* Optimize division by 128 */
          bits = optimal[index - length]->bits + 8 +
                 elias_gamma_bits(offset_msb + 1) +
                 elias_gamma_bits(length - 1);
          if (!od->last_match || od->last_match->index != index ||
              od->last_match->bits > bits) {
            zx0_assign(&od->last_match, zx0_allocate(bits, index, offset,
                                                     optimal[index - length]));
            if (!optimal[index] || optimal[index]->bits > bits)
              zx0_assign(&optimal[index], od->last_match);
          }
        }
      } else {
        /* copy literals */
        od->match_length = 0;
        if (od->last_match) {
          length = index - od->last_match->index;
          bits = od->last_match->bits + 1 + elias_gamma_bits(length) +
                 (length << 3);
          zx0_assign(&od->last_literal,
                     zx0_allocate(bits, index, 0, od->last_match));
          if (!optimal[index] || optimal[index]->bits > bits)
            zx0_assign(&optimal[index], od->last_literal);
        }
      }
    }
  }

  return optimal[input_size - 1];
}
