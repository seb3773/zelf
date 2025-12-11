#ifndef FILTER_SELECTION_H
#define FILTER_SELECTION_H
#include "packer_defs.h"

exe_filter_t decide_exe_filter_auto_v21(const unsigned char *combined,
                                        size_t actual_size, size_t text_off,
                                        size_t text_end);
exe_filter_t decide_exe_filter_auto_zx7b_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size);
exe_filter_t decide_exe_filter_auto_zx0_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size);
exe_filter_t decide_exe_filter_auto_qlz_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size);
exe_filter_t decide_exe_filter_auto_lz4_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size);
exe_filter_t decide_exe_filter_auto_lzav_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size);
exe_filter_t decide_exe_filter_auto_lzma_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size);
exe_filter_t decide_exe_filter_auto_exo_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size);
exe_filter_t decide_exe_filter_auto_zstd_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size);
exe_filter_t decide_exe_filter_auto_doboz_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count, size_t text_off,
    size_t text_end, size_t orig_size);
exe_filter_t decide_exe_filter_auto_snappy_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count, size_t text_off,
    size_t text_end, size_t orig_size);
exe_filter_t decide_exe_filter_auto_apultra_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count, size_t text_off,
    size_t text_end, size_t orig_size);
exe_filter_t decide_exe_filter_auto_shrinkler_model(
    const unsigned char *combined, size_t actual_size,
    const SegmentInfo *segments, size_t segment_count, size_t text_off,
    size_t text_end, size_t orig_size);
exe_filter_t decide_exe_filter_auto_pp_rule(const unsigned char *combined,
                                            size_t actual_size,
                                            const SegmentInfo *segments,
                                            size_t segment_count,
                                            size_t text_off, size_t text_end,
                                            size_t orig_size);
exe_filter_t decide_exe_filter_auto_blz_model(const unsigned char *combined,
                                              size_t actual_size,
                                              const SegmentInfo *segments,
                                              size_t segment_count,
                                              size_t text_off, size_t text_end,
                                              size_t orig_size);
exe_filter_t decide_exe_filter_auto_lzsa_model(const unsigned char *combined,
                                               size_t actual_size,
                                               const SegmentInfo *segments,
                                               size_t segment_count,
                                               size_t text_off, size_t text_end,
                                               size_t orig_size);
exe_filter_t decide_exe_filter_auto_sc_model(const unsigned char *combined,
                                             size_t actual_size,
                                             const SegmentInfo *segments,
                                             size_t segment_count,
                                             size_t text_off, size_t text_end,
                                             size_t orig_size);

#endif
