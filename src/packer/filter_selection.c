#include "filter_selection.h"
#include "apultra_predict_dt.h"
#include "bcj_x86_enc.h"
#include "blz_predict_dt.h"
#include "doboz_predict_dt.h"
#include "exe_predict_feature_index.h"
#include "exo_predict_dt.h"
#include "kanzi_exe_encode_c.h"
#include "lz4_predict_dt.h"
#include "lzav_predict_dt.h"
#include "lzma_predict_dt.h"
#include "lzsa2_predict_dt.h"
#include "pp_predict_dt.h"
#include "qlz_predict_dt.h"
#include "shrinkler_predict_dt.h"
#include "snappy_predict_dt.h"
#include "stonecracker_predict.h"
#include "zstd_predict_dt.h"
#include "zx0_predict_dt.h"
#include "zx7b_predict_dt.h"
#include <lz4.h>
#include <lz4hc.h>
#include <math.h>

// --- Feature scan helpers for BLZ predictor (CPU-bound, small) ---
static inline double entropy_from_hist(const uint64_t hist[256],
                                       uint64_t total) {
  if (total == 0)
    return 0.0;
  double H = 0.0;
  for (int i = 0; i < 256; ++i) {
    if (!hist[i])
      continue;
    double p = (double)hist[i] / (double)total;
    H -= p * (log(p) / log(2.0));
  }

  return H;
}

static inline uint64_t jcc32_count(const uint8_t *p, size_t n) {
  uint64_t c = 0;
  for (size_t i = 0; i + 1 < n; i++) {
    if (p[i] == 0x0F) {
      uint8_t b = p[i + 1];
      if ((b & 0xF0u) == 0x80u)
        c++;
    }
  }
  return c;
}
static inline uint64_t ff_calljmp_count(const uint8_t *p, size_t n) {
  uint64_t c = 0;
  for (size_t i = 0; i + 1 < n; i++) {
    if (p[i] == 0xFF) {
      uint8_t m = p[i + 1];
      uint8_t r = (m >> 3) & 7u;
      if (r == 2u || r == 4u)
        c++;
    }
  }
  return c;
}
static inline uint64_t riprel_estimate_cnt(const uint8_t *p, size_t n) {
  uint64_t c = 0;
  for (size_t i = 0; i + 2 < n; i++) {
    if (p[i] == 0x48 && (p[i + 1] == 0x8B || p[i + 1] == 0x8D)) {
      uint8_t m = p[i + 2];
      if (((m >> 6) & 3u) == 0 && (m & 7u) == 5)
        c++;
    }
  }
  return c;
}
static inline uint64_t imm64_mov_count(const uint8_t *p, size_t n) {
  uint64_t c = 0;
  for (size_t i = 0; i + 1 < n; i++) {
    if (p[i] == 0x48) {
      uint8_t op = p[i + 1];
      if (op >= 0xB8 && op <= 0xBF)
        c++;
    }
  }
  return c;
}
static inline uint64_t count_zero_runs_ge(const uint8_t *p, size_t n,
                                          size_t thr) {
  uint64_t runs = 0;
  size_t i = 0;
  while (i < n) {
    if (p[i] == 0) {
      size_t j = i + 1;
      while (j < n && p[j] == 0)
        j++;
      if ((j - i) >= thr)
        runs++;
      i = j;
    } else {
      i++;
    }
  }
  return runs;
}
static inline uint64_t ro_ptr_like_cnt_scan(const uint8_t *p, size_t n) {
  uint64_t cnt = 0;
  size_t off = ((uintptr_t)p) & 7u;
  if (off) {
    size_t adv = 8 - off;
    if (adv > n)
      return 0;
    p += adv;
    n -= adv;
  }
  for (size_t j = 0; j + 8 <= n; j += 8) {
    uint64_t v;
    memcpy(&v, p + j, 8);
    if ((v & 0xFFFull) == 0)
      cnt++;
  }
  return cnt;
}

// Auto selector v2.1: choose EXE filter using LZ4 proxy compression of filtered
// streams
exe_filter_t decide_exe_filter_auto_v21(const unsigned char *combined,
                                        size_t actual_size, size_t text_off,
                                        size_t text_end) {
  // Bound code range as done in the main Kanzi path
  int code_start = (int)((text_off > actual_size) ? actual_size : text_off);
  int code_end = (int)((text_end > actual_size) ? actual_size : text_end);
  if (code_end < code_start + 9)
    code_end = (code_start + 9 <= (int)actual_size) ? code_start + 9
                                                    : (int)actual_size;

  // Prepare BCJ filtered copy (in-place encoder)
  unsigned char *bcj_buf = (unsigned char *)malloc(actual_size);
  if (!bcj_buf)
    return EXE_FILTER_KANZIEXE;
  memcpy(bcj_buf, combined, actual_size);
  size_t bcj_done = bcj_x86_encode((uint8_t *)bcj_buf, actual_size, 0);
  if (bcj_done == 0) {
    free(bcj_buf);
    return EXE_FILTER_KANZIEXE;
  }

  // Prepare Kanzi filtered buffer
  int processed_kanzi = 0;
  int maxf = kanzi_exe_get_max_encoded_length((int)actual_size);
  if (maxf < (int)actual_size + 64)
    maxf = (int)actual_size + 64; // margin
  unsigned char *kanzi_buf = (unsigned char *)malloc((size_t)maxf);
  if (!kanzi_buf) {
    free(bcj_buf);
    return EXE_FILTER_KANZIEXE;
  }
  int kanzi_out = kanzi_exe_filter_encode_force_x86_range(
      (const unsigned char *)combined, (int)actual_size, kanzi_buf, maxf,
      &processed_kanzi, code_start, code_end);
  size_t kanzi_len = 0;
  if (kanzi_out > 0 && processed_kanzi == (int)actual_size) {
    kanzi_len = (size_t)kanzi_out;
  } else {
    // Synthesize identity header (9 bytes) + raw data
    if ((size_t)maxf < actual_size + 16) {
      unsigned char *tmp =
          (unsigned char *)realloc(kanzi_buf, actual_size + 16);
      if (!tmp) {
        free(kanzi_buf);
        free(bcj_buf);
        return EXE_FILTER_KANZIEXE;
      }
      kanzi_buf = tmp;
      maxf = (int)(actual_size + 16);
    }
    kanzi_buf[0] = 0x40;
    kanzi_buf[1] = 0;
    kanzi_buf[2] = 0;
    kanzi_buf[3] = 0;
    kanzi_buf[4] = 0;
    kanzi_buf[5] = 9;
    kanzi_buf[6] = 0;
    kanzi_buf[7] = 0;
    kanzi_buf[8] = 0;
    memcpy(&kanzi_buf[9], combined, actual_size);
    kanzi_len = actual_size + 9;
  }

  // KEXP guard: if Kanzi expands by >1.5%, prefer BCJ immediately
  double kexp = ((double)kanzi_len / (double)actual_size) - 1.0;
  if (kexp > 0.015) {
    free(kanzi_buf);
    free(bcj_buf);
    return EXE_FILTER_BCJ;
  }

  // LZ4 proxy compression sizes
  int lz4_cap_b = LZ4_compressBound((int)actual_size);
  int lz4_cap_k = LZ4_compressBound((int)kanzi_len);
  char *lz4_out_b = (char *)malloc((size_t)lz4_cap_b);
  char *lz4_out_k = (char *)malloc((size_t)lz4_cap_k);
  if (!lz4_out_b || !lz4_out_k) {
    free(lz4_out_b);
    free(lz4_out_k);
    free(kanzi_buf);
    free(bcj_buf);
    return EXE_FILTER_KANZIEXE;
  }
  int l4_b = LZ4_compress_default((const char *)bcj_buf, lz4_out_b,
                                  (int)actual_size, lz4_cap_b);
  int l4_k = LZ4_compress_default((const char *)kanzi_buf, lz4_out_k,
                                  (int)kanzi_len, lz4_cap_k);
  if (l4_b <= 0 || l4_k <= 0) {
    // If compression failed, fall back to Kanzi
    free(lz4_out_b);
    free(lz4_out_k);
    free(kanzi_buf);
    free(bcj_buf);
    return EXE_FILTER_KANZIEXE;
  }

  long delta = (long)l4_k - (long)l4_b; // positive if BCJ smaller
  double rel = (double)delta / (double)actual_size;

  // Optionally compute LZ4HC agreement for large files
  int agree_hc = 0;
  if (actual_size >= (1u << 20)) {
    int hc_cap_b = lz4_cap_b, hc_cap_k = lz4_cap_k;
    char *hc_out_b = (char *)malloc((size_t)hc_cap_b);
    char *hc_out_k = (char *)malloc((size_t)hc_cap_k);
    if (hc_out_b && hc_out_k) {
      int hc_b = LZ4_compress_HC((const char *)bcj_buf, hc_out_b,
                                 (int)actual_size, hc_cap_b, 12);
      int hc_k = LZ4_compress_HC((const char *)kanzi_buf, hc_out_k,
                                 (int)kanzi_len, hc_cap_k, 12);
      if (hc_b > 0 && hc_k > 0)
        agree_hc = ((long)hc_k - (long)hc_b) > 0;
    }
    free(hc_out_b);
    free(hc_out_k);
  }

  exe_filter_t chosen;
  // R2: if predictor says Kanzi clearly better (<= -0.3%), choose Kanzi
  // directly
  if (rel <= -0.003) {
    chosen = EXE_FILTER_KANZIEXE;
  } else if (actual_size >= (1u << 20)) {
    int strong = 0;
    if (delta >= 8192)
      strong = 1;
    if (rel >= 0.007)
      strong = 1;
    if (agree_hc)
      strong = 1;
    chosen = strong ? EXE_FILTER_BCJ : EXE_FILTER_KANZIEXE;
  } else {
    if (rel >= 0.003)
      chosen = EXE_FILTER_BCJ;
    else {
      // tie zone: prefer Kanzi unless Kanzi expanded notably (>0.2%)
      chosen = (kexp <= 0.002) ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
    }
  }

  free(lz4_out_b);
  free(lz4_out_k);
  free(kanzi_buf);
  free(bcj_buf);
  return chosen;
}

// ... (Copier toutes les autres fonctions decide_exe_filter_auto_*)

exe_filter_t decide_exe_filter_auto_zx7b_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size) {
  (void)text_off;
  (void)text_end;
  uint64_t text_sz = 0, ro_sz = 0, data_sz = 0;
  uint64_t z_text = 0, nz_text = 0, z_ro = 0, nz_ro = 0, z_data = 0,
           nz_data = 0;
  uint64_t e8 = 0, e9 = 0, ffcj = 0, eb = 0, jcc = 0, rip = 0, imm64 = 0,
           ret_cnt = 0;
  uint64_t hist_text[256] = {0}, hist_ro[256] = {0}, hist_data[256] = {0};
  uint64_t disp8_hist[256] = {0};
  uint64_t ro_ascii_bytes = 0;
  uint64_t text_nop_bytes = 0, text_pad_bytes = 0;
  uint64_t rel32_intext_all = 0;
  uint64_t rel32_abs_sum_all = 0, rel32_cnt_all = 0;
  struct {
    uint64_t lo, hi;
  } xseg[64];
  size_t xseg_n = 0;
  for (size_t i = 0; i < segment_count && xseg_n < 64; i++) {
    const SegmentInfo *s = &segments[i];
    if ((s->flags & PF_X) != 0) {
      xseg[xseg_n].lo = s->vaddr;
      xseg[xseg_n].hi = s->vaddr + (uint64_t)s->filesz;
      xseg_n++;
    }
  }
  for (size_t i = 0; i < segment_count; i++) {
    const SegmentInfo *s = &segments[i];
    if (s->filesz == 0)
      continue;
    if ((size_t)s->offset >= (size_t)actual_size)
      continue;
    if ((size_t)s->filesz > (size_t)actual_size - (size_t)s->offset)
      continue;
    const uint8_t *p = (const uint8_t *)(combined + (size_t)s->offset);
    size_t sz = (size_t)s->filesz;
    int is_x = (s->flags & PF_X) != 0;
    int is_r = (s->flags & PF_R) != 0;
    int is_w = (s->flags & PF_W) != 0;
    if (is_x) {
      text_sz += sz;
      for (size_t j = 0; j < sz; j++) {
        uint8_t b = p[j];
        hist_text[b]++;
        if (b == 0)
          z_text++;
        else
          nz_text++;
        eb += (b == 0xEB);
        text_nop_bytes += (b == 0x90);
      }
      uint64_t rel32_intext = 0, rel32_abs_sum = 0, rel32_abs_max = 0;
      for (size_t j2 = 0; j2 + 4 < sz; j2++) {
        uint8_t b = p[j2];
        if (b == 0xE8 || b == 0xE9) {
          if (b == 0xE8)
            e8++;
          else
            e9++;
          int32_t d;
          memcpy(&d, p + j2 + 1, 4);
          uint8_t top = (uint8_t)(((uint32_t)d) >> 24);
          disp8_hist[top]++;
          uint64_t ad = (d < 0) ? (uint64_t)(-(int64_t)d) : (uint64_t)d;
          rel32_abs_sum += ad;
          if (ad > rel32_abs_max)
            rel32_abs_max = ad;
          rel32_abs_sum_all += ad;
          rel32_cnt_all++;
          uint64_t insn_va = (uint64_t)s->vaddr + (uint64_t)j2;
          uint64_t target = (uint64_t)((int64_t)insn_va + 5 + (int64_t)d);
          for (size_t xs = 0; xs < xseg_n; ++xs) {
            if (target >= xseg[xs].lo && target < xseg[xs].hi) {
              rel32_intext++;
              break;
            }
          }
        }
      }
      size_t j = 0;
      while (j < sz) {
        uint8_t b = p[j];
        if (b == 0x00 || b == 0x90) {
          size_t k = j + 1;
          while (k < sz && p[k] == b)
            k++;
          size_t run = k - j;
          if (run >= 4)
            text_pad_bytes += run;
          j = k;
        } else {
          j++;
        }
      }
      jcc += jcc32_count(p, sz);
      ffcj += ff_calljmp_count(p, sz);
      rip += riprel_estimate_cnt(p, sz);
      imm64 += imm64_mov_count(p, sz);
      for (size_t t = 0; t < sz; t++) {
        uint8_t b = p[t];
        if (b == 0xC3)
          ret_cnt++;
        else if (b == 0xC2 && t + 2 < sz)
          ret_cnt++;
      }
      rel32_intext_all += rel32_intext;
    } else if (is_r && !is_w) {
      ro_sz += sz;
      for (size_t j = 0; j < sz; j++) {
        uint8_t b = p[j];
        hist_ro[b]++;
        if (b == 0)
          z_ro++;
        else
          nz_ro++;
        if (!is_x &&
            ((b >= 0x20 && b <= 0x7E) || b == '\t' || b == '\n' || b == '\r'))
          ro_ascii_bytes++;
      }
    } else if (is_w) {
      data_sz += sz;
      for (size_t j = 0; j < sz; j++) {
        uint8_t b = p[j];
        hist_data[b]++;
        if (b == 0)
          z_data++;
        else
          nz_data++;
      }
    }
  }
  uint64_t zr16 = 0, zr32 = 0, ro_ptr_like = 0;
  for (size_t i = 0; i < segment_count; i++) {
    const SegmentInfo *s = &segments[i];
    if (s->filesz == 0)
      continue;
    if ((size_t)s->offset >= (size_t)actual_size)
      continue;
    if ((size_t)s->filesz > (size_t)actual_size - (size_t)s->offset)
      continue;
    const uint8_t *p = (const uint8_t *)(combined + (size_t)s->offset);
    size_t sz = (size_t)s->filesz;
    int is_x = (s->flags & PF_X) != 0;
    int is_r = (s->flags & PF_R) != 0;
    int is_w = (s->flags & PF_W) != 0;
    if (is_x || (is_r && !is_w)) {
      zr16 += count_zero_runs_ge(p, sz, 16);
      zr32 += count_zero_runs_ge(p, sz, 32);
    }
    if (is_r && !is_w && !(s->flags & PF_X)) {
      ro_ptr_like += ro_ptr_like_cnt_scan(p, sz);
    }
  }
  double kb = (text_sz > 0) ? ((double)text_sz / 1024.0) : 1.0;
  double comb = (double)(text_sz + ro_sz);
  double mb = (comb > 0.0) ? (comb / (1024.0 * 1024.0)) : 1.0;
  double bd = ((double)(e8 + e9 + ffcj)) / kb;
  double te = entropy_from_hist(hist_text, z_text + nz_text);
  double ro_ent = entropy_from_hist(hist_ro, z_ro + nz_ro);
  double data_ent = entropy_from_hist(hist_data, z_data + nz_data);
  double ro_ascii_ratio =
      (ro_sz ? (double)ro_ascii_bytes / (double)ro_sz : 0.0);
  double zeros_ratio_total =
      ((text_sz + ro_sz) ? (double)(z_text + z_ro) / (double)(text_sz + ro_sz)
                         : 0.0);
  double text_ratio =
      (orig_size > 0) ? ((double)text_sz / (double)orig_size) : 0.0;
  double ro_ratio = (orig_size > 0) ? ((double)ro_sz / (double)orig_size) : 0.0;
  double data_ratio =
      (orig_size > 0) ? ((double)data_sz / (double)orig_size) : 0.0;
  double disp8_ent = 0.0;
  {
    uint64_t sum = e8 + e9;
    if (sum > 0) {
      double H = 0.0;
      for (int i = 0; i < 256; i++) {
        if (!disp8_hist[i])
          continue;
        double p = (double)disp8_hist[i] / (double)sum;
        H -= p * (log(p) / log(2.0));
      }
      disp8_ent = H;
    }
  }
  double rel32_intext_ratio =
      ((e8 + e9) ? (double)rel32_intext_all / (double)(e8 + e9) : 0.0);
  double avg_rel32_abs =
      (rel32_cnt_all ? (double)rel32_abs_sum_all / (double)rel32_cnt_all : 0.0);
  double nop_ratio = (text_sz ? (double)text_nop_bytes / (double)text_sz : 0.0);
  double align_pad_ratio =
      (text_sz ? (double)text_pad_bytes / (double)text_sz : 0.0);
  double zr16pm = ((double)zr16) / mb;
  double zr32pm = ((double)zr32) / mb;

  double in[PP_FEAT_COUNT];
  for (int i = 0; i < PP_FEAT_COUNT; i++)
    in[i] = 0.0;
  in[PP_FEAT_file_size] = (double)orig_size;
  in[PP_FEAT_n_load] = (double)segment_count;
  in[PP_FEAT_text_sz] = (double)text_sz;
  in[PP_FEAT_ro_sz] = (double)ro_sz;
  in[PP_FEAT_data_sz] = (double)data_sz;
  in[PP_FEAT_text_ratio] = text_ratio;
  in[PP_FEAT_ro_ratio] = ro_ratio;
  in[PP_FEAT_data_ratio] = data_ratio;
  in[PP_FEAT_text_entropy] = te;
  in[PP_FEAT_ro_entropy] = ro_ent;
  in[PP_FEAT_data_entropy] = data_ent;
  in[PP_FEAT_zeros_ratio_total] = zeros_ratio_total;
  in[PP_FEAT_zero_runs_16] = (double)zr16;
  in[PP_FEAT_zero_runs_32] = (double)zr32;
  in[PP_FEAT_ascii_ratio_rodata] = ro_ascii_ratio;
  in[PP_FEAT_e8_cnt] = (double)e8;
  in[PP_FEAT_e9_cnt] = (double)e9;
  in[PP_FEAT_ff_calljmp_cnt] = (double)ffcj;
  in[PP_FEAT_eb_cnt] = (double)eb;
  in[PP_FEAT_jcc32_cnt] = (double)jcc;
  in[PP_FEAT_branch_density_per_kb] = bd;
  in[PP_FEAT_riprel_estimate] = (double)rip;
  in[PP_FEAT_nop_ratio_text] = nop_ratio;
  in[PP_FEAT_imm64_mov_cnt] = (double)imm64;
  in[PP_FEAT_align_pad_ratio] = align_pad_ratio;
  in[PP_FEAT_ro_ptr_like_cnt] = (double)ro_ptr_like;
  in[PP_FEAT_rel32_disp_entropy8] = disp8_ent;
  in[PP_FEAT_avg_rel32_abs] = avg_rel32_abs;
  in[PP_FEAT_rel32_intext_ratio] = rel32_intext_ratio;
  in[PP_FEAT_rel32_intext_cnt] = (double)rel32_intext_all;
  in[PP_FEAT_ret_cnt] = (double)ret_cnt;
  {
    double rel_b = (double)(e8 + e9 + jcc + eb);
    double abs_b = (double)ffcj;
    in[PP_FEAT_rel_branch_ratio] =
        (rel_b + abs_b) ? (rel_b / (rel_b + abs_b)) : 0.0;
  }
  {
    uint64_t xcnt = 0, rocnt = 0, rwcnt = 0;
    uint64_t bss_total = 0;
    for (size_t i = 0; i < segment_count; i++) {
      uint32_t f = segments[i].flags;
      int x = (f & PF_X) != 0;
      int r = (f & PF_R) != 0;
      int w = (f & PF_W) != 0;
      if (x)
        xcnt++;
      else if (r && !w)
        rocnt++;
      else if (w)
        rwcnt++;
    }
    in[PP_FEAT_x_load_cnt] = (double)xcnt;
    in[PP_FEAT_ro_load_cnt] = (double)rocnt;
    in[PP_FEAT_rw_load_cnt] = (double)rwcnt;
    for (size_t i = 0; i < segment_count; i++) {
      if (segments[i].memsz > segments[i].filesz) {
        bss_total += (uint64_t)(segments[i].memsz - segments[i].filesz);
      }
    }
    in[PP_FEAT_bss_sz] = (double)bss_total;
  }

  int pred = zx7b_dt_predict_from_pvec(in); // 1=KANZIEXE, 0=BCJ
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

// Helper to extract features (shared by all models)
static void extract_features_for_model(const unsigned char *combined,
                                       size_t actual_size,
                                       const SegmentInfo *segments,
                                       size_t segment_count, size_t text_off,
                                       size_t text_end, size_t orig_size,
                                       double *in) {
  (void)text_off;
  (void)text_end;
  // Aggregate stats (TEXT/RO/DATA)
  uint64_t text_sz = 0, ro_sz = 0, data_sz = 0;
  uint64_t z_text = 0, nz_text = 0, z_ro = 0, nz_ro = 0, z_data = 0,
           nz_data = 0;
  uint64_t e8 = 0, e9 = 0, ffcj = 0, eb = 0, jcc = 0, rip = 0, imm64 = 0,
           ret_cnt = 0;
  uint64_t hist_text[256] = {0}, hist_ro[256] = {0}, hist_data[256] = {0};
  uint64_t disp8_hist[256] = {0};
  uint64_t ro_ascii_bytes = 0;
  uint64_t text_nop_bytes = 0, text_pad_bytes = 0;
  uint64_t rel32_intext_all = 0;
  uint64_t rel32_abs_sum_all = 0, rel32_cnt_all = 0, rel32_abs_max_all = 0;
  struct {
    uint64_t lo, hi;
  } xseg[64];
  size_t xseg_n = 0;
  for (size_t i = 0; i < segment_count && xseg_n < 64; i++) {
    const SegmentInfo *s = &segments[i];
    if ((s->flags & PF_X) != 0) {
      xseg[xseg_n].lo = s->vaddr;
      xseg[xseg_n].hi = s->vaddr + (uint64_t)s->filesz;
      xseg_n++;
    }
  }
  for (size_t i = 0; i < segment_count; i++) {
    const SegmentInfo *s = &segments[i];
    if (s->filesz == 0)
      continue;
    if ((size_t)s->offset >= (size_t)actual_size)
      continue;
    if ((size_t)s->filesz > (size_t)actual_size - (size_t)s->offset)
      continue;
    const uint8_t *p = (const uint8_t *)(combined + (size_t)s->offset);
    size_t sz = (size_t)s->filesz;
    int is_x = (s->flags & PF_X) != 0;
    int is_r = (s->flags & PF_R) != 0;
    int is_w = (s->flags & PF_W) != 0;
    if (is_x) {
      text_sz += sz;
      for (size_t j = 0; j < sz; j++) {
        uint8_t b = p[j];
        hist_text[b]++;
        if (b == 0)
          z_text++;
        else
          nz_text++;
        eb += (b == 0xEB);
        text_nop_bytes += (b == 0x90);
      }
      uint64_t rel32_intext = 0, rel32_abs_sum = 0, rel32_abs_max = 0;
      for (size_t j2 = 0; j2 + 4 < sz; j2++) {
        uint8_t b = p[j2];
        if (b == 0xE8 || b == 0xE9) {
          if (b == 0xE8)
            e8++;
          else
            e9++;
          int32_t d;
          memcpy(&d, p + j2 + 1, 4);
          uint8_t top = (uint8_t)(((uint32_t)d) >> 24);
          disp8_hist[top]++;
          uint64_t ad = (d < 0) ? (uint64_t)(-(int64_t)d) : (uint64_t)d;
          rel32_abs_sum += ad;
          if (ad > rel32_abs_max)
            rel32_abs_max = ad;
          rel32_abs_sum_all += ad;
          rel32_cnt_all++;
          uint64_t insn_va = (uint64_t)s->vaddr + (uint64_t)j2;
          uint64_t target = (uint64_t)((int64_t)insn_va + 5 + (int64_t)d);
          for (size_t xs = 0; xs < xseg_n; ++xs) {
            if (target >= xseg[xs].lo && target < xseg[xs].hi) {
              rel32_intext++;
              break;
            }
          }
        }
      }
      size_t j = 0;
      while (j < sz) {
        uint8_t b = p[j];
        if (b == 0x00 || b == 0x90) {
          size_t k = j + 1;
          while (k < sz && p[k] == b)
            k++;
          size_t run = k - j;
          if (run >= 4)
            text_pad_bytes += run;
          j = k;
        } else {
          j++;
        }
      }
      jcc += jcc32_count(p, sz);
      ffcj += ff_calljmp_count(p, sz);
      rip += riprel_estimate_cnt(p, sz);
      imm64 += imm64_mov_count(p, sz);
      for (size_t t = 0; t < sz; t++) {
        uint8_t b = p[t];
        if (b == 0xC3)
          ret_cnt++;
        else if (b == 0xC2 && t + 2 < sz)
          ret_cnt++;
      }
      rel32_intext_all += rel32_intext;
      if (rel32_abs_max > rel32_abs_max_all)
        rel32_abs_max_all = rel32_abs_max;
    } else if (is_r && !is_w) {
      ro_sz += sz;
      for (size_t j = 0; j < sz; j++) {
        uint8_t b = p[j];
        hist_ro[b]++;
        if (b == 0)
          z_ro++;
        else
          nz_ro++;
        if (!is_x &&
            ((b >= 0x20 && b <= 0x7E) || b == '\t' || b == '\n' || b == '\r'))
          ro_ascii_bytes++;
      }
    } else if (is_w) {
      data_sz += sz;
      for (size_t j = 0; j < sz; j++) {
        uint8_t b = p[j];
        hist_data[b]++;
        if (b == 0)
          z_data++;
        else
          nz_data++;
      }
    }
  }
  uint64_t zr16 = 0, zr32 = 0, ro_ptr_like = 0;
  for (size_t i = 0; i < segment_count; i++) {
    const SegmentInfo *s = &segments[i];
    if (s->filesz == 0)
      continue;
    if ((size_t)s->offset >= (size_t)actual_size)
      continue;
    if ((size_t)s->filesz > (size_t)actual_size - (size_t)s->offset)
      continue;
    const uint8_t *p = (const uint8_t *)(combined + (size_t)s->offset);
    size_t sz = (size_t)s->filesz;
    int is_x = (s->flags & PF_X) != 0;
    int is_r = (s->flags & PF_R) != 0;
    int is_w = (s->flags & PF_W) != 0;
    if (is_x || (is_r && !is_w)) {
      zr16 += count_zero_runs_ge(p, sz, 16);
      zr32 += count_zero_runs_ge(p, sz, 32);
    }
    if (is_r && !is_w && !(s->flags & PF_X)) {
      ro_ptr_like += ro_ptr_like_cnt_scan(p, sz);
    }
  }
  double kb = (text_sz > 0) ? ((double)text_sz / 1024.0) : 1.0;
  double comb = (double)(text_sz + ro_sz);
  double mb = (comb > 0.0) ? (comb / (1024.0 * 1024.0)) : 1.0;
  double bd = ((double)(e8 + e9 + ffcj)) / kb;
  double te = entropy_from_hist(hist_text, z_text + nz_text);
  double ro_ent = entropy_from_hist(hist_ro, z_ro + nz_ro);
  double data_ent = entropy_from_hist(hist_data, z_data + nz_data);
  double ro_ascii_ratio =
      (ro_sz ? (double)ro_ascii_bytes / (double)ro_sz : 0.0);
  double zeros_ratio_total =
      ((text_sz + ro_sz) ? (double)(z_text + z_ro) / (double)(text_sz + ro_sz)
                         : 0.0);
  double text_ratio =
      (orig_size > 0) ? ((double)text_sz / (double)orig_size) : 0.0;
  double ro_ratio = (orig_size > 0) ? ((double)ro_sz / (double)orig_size) : 0.0;
  double data_ratio =
      (orig_size > 0) ? ((double)data_sz / (double)orig_size) : 0.0;
  double disp8_ent = 0.0;
  {
    uint64_t sum = e8 + e9;
    if (sum > 0) {
      double H = 0.0;
      for (int i = 0; i < 256; i++) {
        if (!disp8_hist[i])
          continue;
        double p = (double)disp8_hist[i] / (double)sum;
        H -= p * (log(p) / log(2.0));
      }
      disp8_ent = H;
    }
  }
  double rel32_intext_ratio =
      ((e8 + e9) ? (double)rel32_intext_all / (double)(e8 + e9) : 0.0);
  double avg_rel32_abs =
      (rel32_cnt_all ? (double)rel32_abs_sum_all / (double)rel32_cnt_all : 0.0);
  double nop_ratio = (text_sz ? (double)text_nop_bytes / (double)text_sz : 0.0);
  double align_pad_ratio =
      (text_sz ? (double)text_pad_bytes / (double)text_sz : 0.0);
  double zr16pm = ((double)zr16) / mb;
  double zr32pm = ((double)zr32) / mb;

  for (int i = 0; i < PP_FEAT_COUNT; i++)
    in[i] = 0.0;
  in[PP_FEAT_file_size] = (double)orig_size;
  in[PP_FEAT_n_load] = (double)segment_count;
  in[PP_FEAT_text_sz] = (double)text_sz;
  in[PP_FEAT_ro_sz] = (double)ro_sz;
  in[PP_FEAT_data_sz] = (double)data_sz;
  in[PP_FEAT_text_ratio] = text_ratio;
  in[PP_FEAT_ro_ratio] = ro_ratio;
  in[PP_FEAT_data_ratio] = data_ratio;
  in[PP_FEAT_text_entropy] = te;
  in[PP_FEAT_ro_entropy] = ro_ent;
  in[PP_FEAT_data_entropy] = data_ent;
  in[PP_FEAT_zeros_ratio_total] = zeros_ratio_total;
  in[PP_FEAT_zero_runs_16] = (double)zr16;
  in[PP_FEAT_zero_runs_32] = (double)zr32;
  in[PP_FEAT_ascii_ratio_rodata] = ro_ascii_ratio;
  in[PP_FEAT_e8_cnt] = (double)e8;
  in[PP_FEAT_e9_cnt] = (double)e9;
  in[PP_FEAT_ff_calljmp_cnt] = (double)ffcj;
  in[PP_FEAT_eb_cnt] = (double)eb;
  in[PP_FEAT_jcc32_cnt] = (double)jcc;
  in[PP_FEAT_branch_density_per_kb] = bd;
  in[PP_FEAT_riprel_estimate] = (double)rip;
  in[PP_FEAT_nop_ratio_text] = nop_ratio;
  in[PP_FEAT_imm64_mov_cnt] = (double)imm64;
  in[PP_FEAT_align_pad_ratio] = align_pad_ratio;
  in[PP_FEAT_ro_ptr_like_cnt] = (double)ro_ptr_like;
  in[PP_FEAT_rel32_disp_entropy8] = disp8_ent;
  in[PP_FEAT_avg_rel32_abs] = avg_rel32_abs;
  in[PP_FEAT_rel32_intext_ratio] = rel32_intext_ratio;
  in[PP_FEAT_rel32_intext_cnt] = (double)rel32_intext_all;
  in[PP_FEAT_ret_cnt] = (double)ret_cnt;
  {
    double rel_b = (double)(e8 + e9 + jcc + eb);
    double abs_b = (double)ffcj;
    in[PP_FEAT_rel_branch_ratio] =
        (rel_b + abs_b) ? (rel_b / (rel_b + abs_b)) : 0.0;
  }
  {
    uint64_t xcnt = 0, rocnt = 0, rwcnt = 0;
    uint64_t bss_total = 0;
    for (size_t i = 0; i < segment_count; i++) {
      uint32_t f = segments[i].flags;
      int x = (f & PF_X) != 0;
      int r = (f & PF_R) != 0;
      int w = (f & PF_W) != 0;
      if (x)
        xcnt++;
      else if (r && !w)
        rocnt++;
      else if (w)
        rwcnt++;
    }
    in[PP_FEAT_x_load_cnt] = (double)xcnt;
    in[PP_FEAT_ro_load_cnt] = (double)rocnt;
    in[PP_FEAT_rw_load_cnt] = (double)rwcnt;
    for (size_t i = 0; i < segment_count; i++) {
      if (segments[i].memsz > segments[i].filesz) {
        bss_total += (uint64_t)(segments[i].memsz - segments[i].filesz);
      }
    }
    in[PP_FEAT_bss_sz] = (double)bss_total;
  }
  in[PP_FEAT_max_rel32_abs] = (double)rel32_abs_max_all;
  // PP_FEAT_etype not needed for most models
}

// --- Wrappers calling the common feature extractor ---

exe_filter_t decide_exe_filter_auto_qlz_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = qlz_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_lz4_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = lz4_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_lzav_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  // Set etype for LZAV
  uint16_t etype = 0;
  if (actual_size >= sizeof(Elf64_Ehdr)) {
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)combined;
    etype = eh->e_type;
  }
  in[PP_FEAT_etype] = (double)etype;

  int pred = lzav_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_zstd_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = zstd_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_doboz_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count, size_t text_off,
    size_t text_end, size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = doboz_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_snappy_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count, size_t text_off,
    size_t text_end, size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = snappy_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_apultra_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count, size_t text_off,
    size_t text_end, size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = apultra_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_blz_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = blz_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_shrinkler_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count, size_t text_off,
    size_t text_end, size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = shrinkler_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_lzma_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = lzma_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_exo_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = exo_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_pp_rule(const unsigned char *combined,
                                            size_t actual_size,
                                            const SegmentInfo *segments,
                                            size_t segment_count,
                                            size_t text_off, size_t text_end,
                                            size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = pp_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_zx0_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = zx0_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_lzsa_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = lzsa2_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}

exe_filter_t decide_exe_filter_auto_sc_model(const unsigned char *combined,
                                             size_t actual_size,
                                             const SegmentInfo *segments,
                                             size_t segment_count,
                                             size_t text_off, size_t text_end,
                                             size_t orig_size) {
  double in[PP_FEAT_COUNT];
  extract_features_for_model(combined, actual_size, segments, segment_count,
                             text_off, text_end, orig_size, in);
  int pred = sc_dt_predict_from_pvec(in);
  return pred ? EXE_FILTER_KANZIEXE : EXE_FILTER_BCJ;
}
