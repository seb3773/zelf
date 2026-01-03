// Standard and project headers
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "shrinkler_comp.h"

// Use bundled reference cruncher headers (header-only style)
#include "ref/LZDecoder.h"
#include "ref/LZEncoder.h"
#include "ref/Pack.h"
#include "ref/RangeCoder.h"
#include "ref/RangeDecoder.h"
#include "ref/Verifier.h"

// Global progress callback for the packer UI (absolute percentage [0..100])
static void (*g_shrinkler_progress_cb)(int) = NULL;

extern "C" void shrinkler_set_progress_cb(void (*cb)(int pct)) {
  g_shrinkler_progress_cb = cb;
}

// Lightweight progress adapter: mirrors PackProgress but calls our callback
// instead of printing
class CBProgress : public LZProgress {
  int size_;
  int last_pct_;
  int block_index_;
  int block_total_;
  int base_;
  int width_;

  static inline void compute_block(int idx, int total, int &base, int &width) {
    if (total <= 1) {
      base = 0;
      width = 100;
      return;
    }
    int w = 100 / total;
    int last_w = 100 - w * (total - 1);
    base = w * idx;
    width = (idx == total - 1) ? last_w : w;
  }
  inline void emit(int pct) {
    if (pct < 0)
      pct = 0;
    if (pct > 100)
      pct = 100;
    if (pct != last_pct_) {
      last_pct_ = pct;
      if (g_shrinkler_progress_cb)
        g_shrinkler_progress_cb(pct);
    }
  }

public:
  CBProgress()
      : size_(0), last_pct_(-1), block_index_(0), block_total_(1), base_(0),
        width_(100) {}
  void set_block(int idx, int total) {
    block_index_ = idx;
    block_total_ = total;
    compute_block(idx, total, base_, width_);
    last_pct_ = -1; // force refresh
  }
  virtual void begin(int size) {
    size_ = size;
    emit(base_);
  }
  virtual void update(int pos) {
    if (size_ <= 0)
      return;
    long long local = (long long)pos * (long long)width_ / (long long)size_;
    if (local < 0)
      local = 0;
    if (local > width_)
      local = width_;
    emit(base_ + (int)local);
  }
  virtual void end() {
    if (block_index_ == block_total_ - 1)
      emit(100);
  }
};

extern "C" {

// Big-endian writers
static inline void wr16_be(unsigned char *p, unsigned v) {
  p[0] = (unsigned char)((v >> 8) & 0xFF);
  p[1] = (unsigned char)(v & 0xFF);
}
static inline void wr32_be(unsigned char *p, unsigned v) {
  p[0] = (unsigned char)((v >> 24) & 0xFF);
  p[1] = (unsigned char)((v >> 16) & 0xFF);
  p[2] = (unsigned char)((v >> 8) & 0xFF);
  p[3] = (unsigned char)(v & 0xFF);
}

// Shrinkler data header constants (mirrors reference DataFile.h)
#define SHRINKLER_MAJOR_VERSION 4
#define SHRINKLER_MINOR_VERSION 7
#define FLAG_PARITY_CONTEXT (1 << 0)

int shrinkler_compress_memory(const uint8_t *in, size_t in_size,
                              uint8_t **out_buf, size_t *out_size,
                              const shrinkler_comp_params *opt) {
  if (!in || in_size == 0 || !out_buf || !out_size)
    return 0;

  // Parameters: fixed -r 400000; bytes mode (no parity context)
  PackParams params;
  params.parity_context = false;
  params.iterations = (opt && opt->iterations > 0) ? opt->iterations : 3;
  params.length_margin =
      (opt && opt->length_margin > 0) ? opt->length_margin : 3;
  params.skip_length = (opt && opt->skip_length > 0) ? opt->skip_length : 3000;
  params.match_patience = (opt && opt->effort > 0) ? opt->effort : 300;
  params.max_same_length =
      (opt && opt->same_length > 0) ? opt->same_length : 30;

  std::vector<unsigned char> pack_buffer;
  RefEdgeFactory edge_factory(400000);

  // Custom pack loop with progress callback
  // (Adapted from Pack::packData to avoid stdout prints)
  // Setup range coder target
  RangeCoder final_range(LZEncoder::NUM_CONTEXTS, pack_buffer);
  final_range.reset();

  // Build parser/match finder
  MatchFinder finder((unsigned char *)in, (int)in_size, 2,
                     params.match_patience, params.max_same_length);
  LZParser parser((unsigned char *)in, (int)in_size, 0, finder,
                  params.length_margin, params.skip_length, &edge_factory);

  // Progress object
  CBProgress cbprog;
  LZProgress *progress = g_shrinkler_progress_cb
                             ? (LZProgress *)&cbprog
                             : (LZProgress *)new NoProgress();

  // Iterative optimization
  result_size_t real_size = 0;
  result_size_t best_size = (result_size_t)1 << (32 + 3 + Coder::BIT_PRECISION);
  int best_result = 0;
  std::vector<LZParseResult> results(2);
  CountingCoder *counting_coder = new CountingCoder(LZEncoder::NUM_CONTEXTS);

  for (int i = 0; i < params.iterations; ++i) {
    if (progress == &cbprog)
      cbprog.set_block(i, params.iterations);
    // Parse data into LZ symbols
    LZParseResult &result = results[1 - best_result];
    Coder *measurer = new SizeMeasuringCoder(counting_coder);
    measurer->setNumberContexts(LZEncoder::NUMBER_CONTEXT_OFFSET,
                                LZEncoder::NUM_NUMBER_CONTEXTS, (int)in_size);
    finder.reset();
    result = parser.parse(LZEncoder(measurer, params.parity_context), progress);
    delete measurer;

    // Estimate size using adaptive range coding
    std::vector<unsigned char> dummy;
    RangeCoder *rc = new RangeCoder(LZEncoder::NUM_CONTEXTS, dummy);
    real_size = result.encode(LZEncoder(rc, params.parity_context));
    rc->finish();
    delete rc;

    if (real_size < best_size) {
      best_result = 1 - best_result;
      best_size = real_size;
    }

    // Update symbol frequencies for next iteration
    CountingCoder *new_counting = new CountingCoder(LZEncoder::NUM_CONTEXTS);
    result.encode(LZEncoder(counting_coder, params.parity_context));
    CountingCoder *old = counting_coder;
    counting_coder = new CountingCoder(old, new_counting);
    delete old;
    delete new_counting;
  }
  if (progress != &cbprog)
    delete progress;
  delete counting_coder;

  // Encode the best result into final_range
  results[best_result].encode(LZEncoder(&final_range, params.parity_context));
  final_range.finish();

  // Verify like the reference
  RangeDecoder decoder(LZEncoder::NUM_CONTEXTS, pack_buffer);
  LZDecoder lzd(&decoder, params.parity_context);
  LZVerifier verifier(0, (unsigned char *)in, (int)in_size, (int)in_size, 1);
  decoder.reset();
  decoder.setListener(&verifier);
  if (!lzd.decode(verifier) || verifier.size() != (int)in_size)
    return 0;

  int margin =
      verifier.front_overlap_margin + (int)pack_buffer.size() - (int)in_size;

  // Build Shrinkler data header (see reference DataFile::crunch())
  const int header_bytes = 24; // packed DataHeader layout
  std::vector<unsigned char> out;
  out.resize((size_t)header_bytes + pack_buffer.size());

  unsigned char *h = &out[0];
  h[0] = 'S';
  h[1] = 'h';
  h[2] = 'r';
  h[3] = 'i';
  h[4] = SHRINKLER_MAJOR_VERSION; // major
  h[5] = SHRINKLER_MINOR_VERSION; // minor
  wr16_be(&h[6], (unsigned)(header_bytes - 8));
  wr32_be(&h[8], (unsigned)pack_buffer.size());
  wr32_be(&h[12], (unsigned)in_size);
  wr32_be(&h[16], (unsigned)margin);
  wr32_be(&h[20], params.parity_context ? FLAG_PARITY_CONTEXT : 0);

  // Copy packed payload
  std::memcpy(&out[header_bytes], &pack_buffer[0], pack_buffer.size());

  // Return
  *out_size = out.size();
  *out_buf = (uint8_t *)std::malloc(*out_size);
  if (!*out_buf)
    return 0;
  std::memcpy(*out_buf, &out[0], *out_size);
  return 1;
}

} // extern "C"
