// Copyright 1999-2019 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Suffix array construction based on the SA-IS algorithm.

*/

#pragma once

#include <algorithm>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif
#include <stdint.h>

using std::vector;
using std::fill;

#define UNINITIALIZED (-1)
#define IS_LMS(i) ((i) > 0 && stype[(i)] && !stype[(i) - 1])

void induced(const int * __restrict data, int * __restrict suffix_array, int length, int alphabet_size, const vector<bool>& stype, const int * __restrict buckets, int * __restrict bucket_index) {
	// Induce L suffixes
	for (int b = 0 ; b < alphabet_size ; b++) {
		bucket_index[b] = buckets[b];
	}
	for (int s = 0 ; s < length ; s++) {
		int index = suffix_array[s];
		if (index > 0 && !stype[index - 1]) {
			suffix_array[bucket_index[data[index - 1]]++] = index - 1;
		}
	}
	// Induce S suffixes
	for (int b = 0 ; b < alphabet_size ; b++) {
		bucket_index[b] = buckets[b + 1];
	}
	for (int s = length - 1 ; s >= 0 ; s--) {
		int index = suffix_array[s];
		assert(index != UNINITIALIZED);
		if (index > 0 && stype[index - 1]) {
			suffix_array[--bucket_index[data[index - 1]]] = index - 1;
		}
	}
}

// --- BEGIN LCP U8 HELPERS ---
// Byte-wise LCP using AVX2; safe on non-AVX2 via runtime dispatch.
static inline int lcp_eq_scalar_u8(const unsigned char* a, const unsigned char* b, int max) {
    int k = 0;
    while (k < max && a[k] == b[k]) k++;
    return k;
}

#if defined(__x86_64__) || defined(__i386__)
static inline __attribute__((target("avx2"))) int lcp_eq_avx2_u8(const unsigned char* a, const unsigned char* b, int max) {
    int k = 0;
    while ((k + 32) <= max) {
        __m256i va = _mm256_loadu_si256((const __m256i*)(a + k));
        __m256i vb = _mm256_loadu_si256((const __m256i*)(b + k));
        __m256i cmp = _mm256_cmpeq_epi8(va, vb);
        unsigned mask = (unsigned)_mm256_movemask_epi8(cmp);
        if (__builtin_expect(mask != 0xFFFFFFFFu, 0)) {
            unsigned inv = ~mask;
            int rel = __builtin_ctz(inv);
            return k + rel;
        }
        k += 32;
    }
    return k + lcp_eq_scalar_u8(a + k, b + k, max - k);
}

static inline __attribute__((target("sse2"))) int lcp_eq_sse2_u8(const unsigned char* a, const unsigned char* b, int max) {
    int k = 0;
    while ((k + 16) <= max) {
        __m128i va = _mm_loadu_si128((const __m128i*)(a + k));
        __m128i vb = _mm_loadu_si128((const __m128i*)(b + k));
        __m128i cmp = _mm_cmpeq_epi8(va, vb);
        unsigned mask = (unsigned)_mm_movemask_epi8(cmp);
        if (__builtin_expect(mask != 0xFFFFu, 0)) {
            unsigned inv = ~mask;
            int rel = __builtin_ctz(inv);
            return k + rel;
        }
        k += 16;
    }
    return k + lcp_eq_scalar_u8(a + k, b + k, max - k);
}

static inline __attribute__((target("avx512bw,avx512vl"))) int lcp_eq_avx512_u8(const unsigned char* a, const unsigned char* b, int max) {
    int k = 0;
    while ((k + 64) <= max) {
        __m512i va = _mm512_loadu_si512((const void*)(a + k));
        __m512i vb = _mm512_loadu_si512((const void*)(b + k));
        __mmask64 m = _mm512_cmpeq_epi8_mask(va, vb);
        if (__builtin_expect(m != ~(__mmask64)0, 0)) {
            __mmask64 inv = ~m;
            unsigned long long u = (unsigned long long)inv;
            int rel = __builtin_ctzll(u);
            return k + rel;
        }
        k += 64;
    }
    return k + lcp_eq_scalar_u8(a + k, b + k, max - k);
}

static inline int lcp_eq_runtime_u8(const unsigned char* a, const unsigned char* b, int max) {
    return lcp_eq_sse2_u8(a, b, max);
}
#else
static inline int lcp_eq_runtime_u8(const unsigned char* a, const unsigned char* b, int max) {
    return lcp_eq_scalar_u8(a, b, max);
}
#endif

// --- END LCP U8 HELPERS ---

bool substrings_equal(const int *data, int i1, int i2, const vector<bool>& stype) {
    while (data[i1++] == data[i2++]) {
        if (IS_LMS(i1) && IS_LMS(i2)) return true;
    }
    return false;
}

// Compute the suffix array of a string over an integer alphabet.
// The last character in the string (the sentinel) must be uniquely smallest in the string.
void computeSuffixArray(const int * __restrict data, int * __restrict suffix_array, int length, int alphabet_size) {
	assert(length >= 1);
	if (length == 1) {
		suffix_array[0] = 0;
		return;
	}

	vector<bool> stype(length);
	vector<int> buckets(alphabet_size + 1, 0);
	vector<int> bucket_index(alphabet_size);

	// Compute suffix types and count symbols
	stype[length - 1] = true;
	buckets[data[length - 1]] = 1;
	bool is_s = true;
	int lms_count = 0;
	for (int i = length - 2; i >= 0; i--) {
		buckets[data[i]]++;
		if (data[i] > data[i + 1]) {
			if (is_s) lms_count++;
			is_s = false;
		} else if (data[i] < data[i + 1]) {
			is_s = true;
		}
		stype[i] = is_s;
	}
	// Induce to sort LMS strings
	induce(data, suffix_array, length, alphabet_size, stype, &buckets[0], &bucket_index[0]);

	// Compact LMS indices at the beginning of the suffix array
	int j = 0;
	for (int s = 0; s < length; s++) {
		int index = suffix_array[s];
		if (IS_LMS(index)) {
			suffix_array[j++] = index;
		}
	}
	assert(j == lms_count);

	// Name LMS strings, using the second half of the suffix array
	int *sub_data = &suffix_array[length / 2];
	int sub_capacity = length - length / 2;
	fill(sub_data, &sub_data[sub_capacity], UNINITIALIZED);
	int name = 0;
	int prev_index = UNINITIALIZED;
	for (int s = 0; s < lms_count; s++) {
		int index = suffix_array[s];
		assert(index != UNINITIALIZED);
		if (prev_index != UNINITIALIZED && !substrings_equal(data, prev_index, index, stype)) {
			name += 1;
		}
		assert(sub_data[index / 2] == UNINITIALIZED);
		sub_data[index / 2] = name;
		prev_index = index;
	}
	int new_alphabet_size = name + 1;

	if (new_alphabet_size != lms_count) {
		// Order LMS strings using suffix array of named LMS symbols

		// Compact named LMS symbols
		j = 0;
		for (int i = 0; i < sub_capacity; i++) {
			int name2 = sub_data[i];
			if (name2 != UNINITIALIZED) {
				sub_data[j++] = name2;
			}
		}
		assert(j == lms_count);

		// Sort named LMS symbols recursively
		computeSuffixArray(sub_data, suffix_array, lms_count, new_alphabet_size);

		// Map named LMS symbol indices to LMS string indices in input string
		j = 0;
		for (int i = 1; i < length ; i++) {
			if (IS_LMS(i)) {
				sub_data[j++] = i;
			}
		}
		assert(j == lms_count);
		for (int s = 0 ; s < lms_count ; s++) {
			assert(suffix_array[s] < lms_count);
			suffix_array[s] = sub_data[suffix_array[s]];
		}
	}

	// Put LMS suffixes in sorted order at the ends of buckets
	j = length;
	int s = lms_count - 1;
	for (int b = alphabet_size - 1; b >= 0; b--) {
		while (s >= 0 && data[suffix_array[s]] == b) {
			suffix_array[--j] = suffix_array[s--];
		}
		assert(j >= buckets[b]);
		while (j > buckets[b]) {
			suffix_array[--j] = UNINITIALIZED;
		}
	}

	// Induce from sorted LMS strings to sort all suffixes
	induce(data, suffix_array, length, alphabet_size, stype, &buckets[0], &bucket_index[0]);
}
