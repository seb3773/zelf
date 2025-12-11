// Auto-generated feature index mapping (input order) — common to all EXE filter predictors
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    PP_FEAT_file_size = 0,
    PP_FEAT_etype = 1,
    PP_FEAT_has_interp = 2,
    PP_FEAT_n_load = 3,
    PP_FEAT_text_sz = 4,
    PP_FEAT_ro_sz = 5,
    PP_FEAT_data_sz = 6,
    PP_FEAT_text_ratio = 7,
    PP_FEAT_ro_ratio = 8,
    PP_FEAT_data_ratio = 9,
    PP_FEAT_text_entropy = 10,
    PP_FEAT_ro_entropy = 11,
    PP_FEAT_data_entropy = 12,
    PP_FEAT_zeros_ratio_total = 13,
    PP_FEAT_zero_runs_16 = 14,
    PP_FEAT_zero_runs_32 = 15,
    PP_FEAT_ascii_ratio_rodata = 16,
    PP_FEAT_e8_cnt = 17,
    PP_FEAT_e9_cnt = 18,
    PP_FEAT_ff_calljmp_cnt = 19,
    PP_FEAT_eb_cnt = 20,
    PP_FEAT_jcc32_cnt = 21,
    PP_FEAT_branch_density_per_kb = 22,
    PP_FEAT_riprel_estimate = 23,
    PP_FEAT_nop_ratio_text = 24,
    PP_FEAT_imm64_mov_cnt = 25,
    PP_FEAT_align_pad_ratio = 26,
    PP_FEAT_ro_ptr_like_cnt = 27,
    PP_FEAT_rel32_intext_ratio = 28,
    PP_FEAT_rel32_intext_cnt = 29,
    PP_FEAT_rel32_disp_entropy8 = 30,
    PP_FEAT_x_load_cnt = 31,
    PP_FEAT_ro_load_cnt = 32,
    PP_FEAT_rw_load_cnt = 33,
    PP_FEAT_bss_sz = 34,
    PP_FEAT_avg_p_align_log2 = 35,
    PP_FEAT_max_p_align_log2 = 36,
    PP_FEAT_ret_cnt = 37,
    PP_FEAT_rel_branch_ratio = 38,
    PP_FEAT_avg_rel32_abs = 39,
    PP_FEAT_max_rel32_abs = 40,
} PpFeatIndex;
#define PP_FEAT_COUNT 41
#ifdef __cplusplus
}
#endif
