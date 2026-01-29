#include "sc_compress.h"
#include "bitstream.h"
#include <stdio.h>
#include <string.h>

// S403 / ZULU Header
// 0-3: "ZULU"
// 4-7: 0
// 8-11: Raw Size (BE)
// 12-15: Packed Size - 2 (BE)

static void write_be32(uint8_t *p, uint32_t v) {
  p[0] = (v >> 24) & 0xFF;
  p[1] = (v >> 16) & 0xFF;
  p[2] = (v >> 8) & 0xFF;
  p[3] = v & 0xFF;
}

size_t sc_compress(const uint8_t *input, size_t input_size, uint8_t *output,
                   size_t output_capacity) {
  BitStream bs;
  bitstream_init(&bs);

  // LZ77 Compression
  // Process input BACKWARDS
  // Window: input[i+1 ... i+window_size]
  // Max distance: 5408

  size_t i = input_size;
  while (i > 0) {
    // Current position is i-1 (we are processing byte at input[i-1])
    // But wait, if we match a string of length L starting at i-L.
    // We want to match input[i-L ... i-1] with input[i-L+dist ... i-1+dist].

    // Let's define current cursor 'pos' = i.
    // We want to encode data ending at 'pos'.
    // No, we are generating the stream backwards.
    // The decompressor generates data backwards.
    // So the LAST command we generate (which is the FIRST in the file)
    // corresponds to the START of the file (input[0]). But we are processing
    // from End to Start. So the FIRST command we generate corresponds to
    // input[end-1].

    // So at step 'i' (cursor at input[i-1]), we are encoding input[i-1].
    // If we find a match of length L starting at i-1 (going backwards? No).
    // Decompressor copies L bytes.
    // It copies `*--dst = *--src`.
    // So it copies input[i-1], input[i-2]...
    // So the match must be:
    // input[i-1] == input[i-1+dist]
    // input[i-2] == input[i-2+dist]
    // ...

    // So we look for a string starting at i-1 and going BACKWARDS (towards 0)
    // that matches a string starting at i-1+dist and going BACKWARDS.
    // Effectively, we look for a match for the string ending at i-1.

    // Let's simplify:
    // We are at `pos = i`. We want to encode `input[pos-1]`.
    // Can we encode `input[pos-len ... pos-1]` as a match?
    // Match must exist at `pos-1+dist`.
    // So `input[pos-k] == input[pos-k+dist]` for k=1..len.

    // Search window: `input[pos ... pos+window_size]`.
    // We look for `input[pos-len ... pos-1]` inside the window?
    // No, the window contains the data *already processed* (higher indices).
    // The match reference is `pos + dist`.
    // So we look for the sequence `input[pos-len ... pos-1]` appearing at
    // `input[pos-len+dist ... pos-1+dist]`. Wait. `src = current + distance`.
    // `current` points to `pos`.
    // So `src` points to `pos + distance`.
    // Copy loop:
    // `*--dst = *--src`.
    // `dst` goes `pos-1`, `pos-2`...
    // `src` goes `pos+dist-1`, `pos+dist-2`...
    // So `input[pos-1]` must match `input[pos+dist-1]`.
    // `input[pos-2]` must match `input[pos+dist-2]`.

    // So we look for a string starting at `pos-1` (going down) that matches a
    // string starting at `pos+dist-1` (going down). Or, equivalently: String
    // `input[pos-len ... pos-1]` matches `input[pos+dist-len ... pos+dist-1]`.

    // So we look for the longest match for the string *ending* at `pos-1`.
    // The match must *end* at `pos+dist-1`.

    // Max distance = 5408.
    // We search for `input[pos-1]` in `input[pos ... pos+5408]`.
    // If found at `pos+k`, then `dist = k+1`.
    // Then we check how many bytes match backwards.

    size_t pos = i;
    if (pos == 0)
      break;

    int best_len = 0;
    int best_dist = 0;

    // Brute force search
    // Search for input[pos-1] in input[pos ... min(pos+5408, input_size)]
    size_t max_search = (pos + 5408 > input_size) ? input_size : pos + 5408;

    for (size_t candidate = pos; candidate < max_search; candidate++) {
      if (input[candidate] == input[pos - 1]) {
        // Found a match for the first byte.
        // Check how long the match extends backwards.
        int len = 1;
        // Max length?
        // We can match up to `pos`.
        // And `candidate` can go back up to `pos`? No, `candidate` is > `pos`.
        // We check `input[pos-1-k] == input[candidate-k]`.

        size_t max_len = pos; // Can go back to 0.
        // Also limited by encoding?
        // Christmas tree encoding can handle arbitrary length?
        // "Loop: Read 3 bits -> tmp. Count += tmp. If tmp != 7, break."
        // Yes, arbitrary length.

        while (len < (int)max_len &&
               input[pos - 1 - len] == input[candidate - len]) {
          len++;
        }

        // Prefer longer matches, or closer matches if length is same?
        // Usually closer is better for bit cost, but here distance cost varies.
        // Distance cost:
        // 1-32: 5 bits + 2 = 7 bits.
        // 33-288: 8 bits + 2 = 10 bits.
        // 289-1312: 10 bits + 2 = 12 bits.
        // 1313-5408: 12 bits + 2 = 14 bits.

        // Literal cost: 9 bits per byte.
        // Match cost: DistBits + CountBits + 1 (Match flag).
        // CountBits:
        // 2: 2 bits (1, 0)
        // 3: 3 bits (1, 1, 0)
        // 4: 4 bits (1, 1, 1, 0)
        // 5: 5 bits + 3 (1, 1, 1, 1, val) = 8 bits.

        // Simple heuristic: Take longest match.
        if (len > best_len) {
          best_len = len;
          best_dist = candidate - (pos - 1);
        }
      }
    }

    // Threshold for match
    // If len=2. Cost ~7+2+1 = 10 bits. Literals: 18 bits. Gain.
    // Even worst case distance: 14+2+1 = 17 bits. Gain.
    // So len >= 2 is always good?
    // Wait, Count=2 encoding:
    // `Count=2`. Read bit. If 0 -> End.
    // So `1` (Match) + Dist + `0` (Count=2).
    // Total bits = 1 + DistBits + 1.
    // Best dist: 1 + 7 + 1 = 9 bits.
    // 2 Literals: 18 bits.
    // So len=2 is worth it.

    if (best_len >= 2) {
      // Emit Match
      bitstream_put_bit(&bs, 1);

      // Emit Distance
      int dist = best_dist; // 1-based
      int vlc_value = dist - 1;

      if (vlc_value < 32) {
        bitstream_put_bits(&bs, 0, 2); // Index 00
        bitstream_put_bits(&bs, vlc_value, 5);
      } else if (vlc_value < 288) {
        bitstream_put_bits(&bs, 1, 2); // Index 01
        bitstream_put_bits(&bs, vlc_value - 32, 8);
      } else if (vlc_value < 1312) {
        bitstream_put_bits(&bs, 2, 2); // Index 10
        bitstream_put_bits(&bs, vlc_value - 288, 10);
      } else {
        bitstream_put_bits(&bs, 3, 2); // Index 11
        bitstream_put_bits(&bs, vlc_value - 1312, 12);
      }

      // Emit Count
      // Count starts at 2.
      // Logic: 1 means STOP (Count is current). 0 means CONTINUE (Count++).
      int count = best_len;
      if (count == 2) {
        bitstream_put_bit(&bs, 1); // Count = 2
      } else {
        bitstream_put_bit(&bs, 0); // Count >= 3
        if (count == 3) {
          bitstream_put_bit(&bs, 1); // Count = 3
        } else {
          bitstream_put_bit(&bs, 0); // Count >= 4
          if (count == 4) {
            bitstream_put_bit(&bs, 1); // Count = 4
          } else {
            bitstream_put_bit(&bs, 0); // Count >= 5
            // Loop
            int remaining = count - 5;
            while (remaining >= 7) {
              bitstream_put_bits(&bs, 7, 3);
              remaining -= 7;
            }
            bitstream_put_bits(&bs, remaining, 3);
          }
        }
      }

      i -= best_len;
    } else {
      // Emit Literal
      bitstream_put_bit(&bs, 0);
      bitstream_put_bits(&bs, input[pos - 1], 8);
      i--;
    }
  }

  bitstream_flush(&bs);

  size_t bs_size = bitstream_get_size_bytes(&bs);
  size_t total_size = 16 + bs_size;

  if (total_size > output_capacity) {
    bitstream_free(&bs);
    return 0; // Output buffer too small
  }

  // Write Header
  memcpy(output, "ZULU", 4);
  memset(output + 4, 0, 4);
  write_be32(output + 8, (uint32_t)input_size);
  write_be32(output + 12, (uint32_t)(bs_size - 2));

  // Write Bitstream
  bitstream_write_to_buffer(&bs, output + 16);

  bitstream_free(&bs);
  return total_size;
}
