// Copyright 1999-2022 Aske Simon Christensen. See LICENSE.txt for usage terms.

/*

Pack a data block in multiple iterations, reporting progress along the way.

*/

#pragma once

#include <cstdio>

#include "RangeCoder.h"
#include "CountingCoder.h"
#include "SizeMeasuringCoder.h"
#include "LZEncoder.h"
#include "LZParser.h"

struct PackParams {
	bool parity_context;

	int iterations;
	int length_margin;
	int skip_length;
	int match_patience;
	int max_same_length;
};

class PackProgress : public LZProgress {
    int size;
    int last_pct;
    int block_index;
    int block_total;
    int block_base;
    int block_width;

    static inline void compute_block(int idx, int total, int& base, int& width) {
        if (total <= 1) { base = 0; width = 100; return; }
        int w = 100 / total;
        int last_w = 100 - w * (total - 1);
        base = w * idx;
        width = (idx == total - 1) ? last_w : w;
    }

public:
    PackProgress() : size(0), last_pct(-1), block_index(0), block_total(1), block_base(0), block_width(100) {}

    void set_block(int idx, int total) {
        block_index = idx;
        block_total = total;
        compute_block(idx, total, block_base, block_width);
        last_pct = -1; // force refresh on next begin/update
    }

    virtual void begin(int size) {
        this->size = size;
        // Print initial 0% for this block range
        int pct = block_base;
        if (pct != last_pct) {
            std::printf("\r%3d%%", pct);
            std::fflush(stdout);
            last_pct = pct;
        }
    }

    virtual void update(int pos) {
        if (size <= 0) return;
        int local = (int)(((long long)pos * block_width) / size);
        if (local < 0) local = 0; if (local > block_width) local = block_width;
        int pct = block_base + local;
        if (pct > 100) pct = 100;
        if (pct != last_pct) {
            std::printf("\r%3d%%", pct);
            std::fflush(stdout);
            last_pct = pct;
        }
    }
    virtual void end() {
        // At end of last block, ensure 100% printed
        if (block_index == block_total - 1 && last_pct != 100) {
            std::printf("\r%3d%%", 100);
            std::fflush(stdout);
            last_pct = 100;
        }
    }
};

class NoProgress : public LZProgress {
public:
    virtual void begin(int size) {}
    virtual void update(int pos) {}
    virtual void end() {}
};

void packData(unsigned char *data, int data_length, int zero_padding, PackParams *params, Coder *result_coder, RefEdgeFactory *edge_factory, bool show_progress) {
    MatchFinder finder(data, data_length, 2, params->match_patience, params->max_same_length);
    LZParser parser(data, data_length, zero_padding, finder, params->length_margin, params->skip_length, edge_factory);
    result_size_t real_size = 0;
    result_size_t best_size = (result_size_t)1 << (32 + 3 + Coder::BIT_PRECISION);
    int best_result = 0;
    vector<LZParseResult> results(2);
    CountingCoder *counting_coder = new CountingCoder(LZEncoder::NUM_CONTEXTS);
    LZProgress *progress;
    PackProgress *pp = NULL;
    if (show_progress) {
        pp = new PackProgress();
        progress = pp;
    } else {
        progress = new NoProgress();
    }
    for (int i = 0 ; i < params->iterations ; i++) {
        if (pp) pp->set_block(i, params->iterations);

        // Parse data into LZ symbols
        LZParseResult& result = results[1 - best_result];
        Coder *measurer = new SizeMeasuringCoder(counting_coder);
        measurer->setNumberContexts(LZEncoder::NUMBER_CONTEXT_OFFSET, LZEncoder::NUM_NUMBER_CONTEXTS, data_length);
        finder.reset();
        result = parser.parse(LZEncoder(measurer, params->parity_context), progress);
        delete measurer;

        // Encode result using adaptive range coding
        vector<unsigned char> dummy_result;
        RangeCoder *range_coder = new RangeCoder(LZEncoder::NUM_CONTEXTS, dummy_result);
        real_size = result.encode(LZEncoder(range_coder, params->parity_context));
        range_coder->finish();
        delete range_coder;

        // Choose if best
        if (real_size < best_size) {
            best_result = 1 - best_result;
            best_size = real_size;
        }

        // Count symbol frequencies
        CountingCoder *new_counting_coder = new CountingCoder(LZEncoder::NUM_CONTEXTS);
        result.encode(LZEncoder(counting_coder, params->parity_context));
    
		// New size measurer based on frequencies
		CountingCoder *old_counting_coder = counting_coder;
		counting_coder = new CountingCoder(old_counting_coder, new_counting_coder);
		delete old_counting_coder;
		delete new_counting_coder;
	}
	    delete progress;
    delete counting_coder;

	results[best_result].encode(LZEncoder(result_coder, params->parity_context));
}
