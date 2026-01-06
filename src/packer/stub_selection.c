#include "stub_selection.h"

// Embedded stub binaries (generated with objcopy/ld -b binary).
// Symbols follow the convention: _binary_build_stubs_<file>_start/end
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynexec_kexe_bin_end[];

extern const unsigned char _binary_build_stubs_stub_rnc_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_apultra_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_apultra_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_apultra_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_apultra_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_apultra_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_apultra_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_apultra_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_apultra_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_lzav_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_zstd_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_blz_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_blz_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_blz_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_blz_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_blz_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_blz_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_blz_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_blz_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_csc_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_csc_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_csc_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_csc_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_csc_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_csc_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_csc_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_csc_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_density_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_density_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_density_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_density_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_density_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_density_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_density_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_density_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_doboz_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_doboz_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_doboz_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_doboz_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_doboz_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_doboz_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_doboz_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_doboz_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_exo_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_exo_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_exo_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_exo_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_exo_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_exo_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_exo_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_exo_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_lzfse_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzfse_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzfse_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzfse_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzfse_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzfse_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzfse_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzfse_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_lzham_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzham_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzham_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzham_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzham_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzham_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzham_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzham_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_lzma_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_lzsa_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_nz1_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_nz1_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_nz1_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_nz1_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_nz1_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_nz1_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_nz1_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_nz1_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_pp_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_pp_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_pp_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_pp_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_qlz_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_sc_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_sc_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_sc_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_sc_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_shrinkler_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_snappy_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_snappy_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_snappy_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_snappy_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_snappy_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_snappy_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_snappy_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_snappy_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_zx0_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynexec_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_zx7b_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynexec_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynexec_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynexec_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynexec_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynexec_bcj_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynexec_kexe_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_rnc_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_rnc_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_static_kexe_bin_end[];

extern const unsigned char _binary_build_stubs_stub_apultra_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_static_bcj_bin_end[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynexec_kexe_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynexec_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_start[];

extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzsa_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_nz1_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_nz1_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_pp_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_pp_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_qlz_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_sc_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sc_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_apultra_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_apultra_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_sfx_apultra_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_apultra_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_blz_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_blz_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_csc_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_csc_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_density_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_density_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_doboz_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_doboz_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_exo_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_exo_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lz4_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lz4_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzav_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzav_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzfse_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzfse_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzham_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzham_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzma_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzma_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzsa_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_lzsa_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_nz1_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_nz1_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_pp_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_pp_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_qlz_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_qlz_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_rnc_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_rnc_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_sc_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_sc_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_shrinkler_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_shrinkler_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_snappy_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_snappy_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_zstd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_zstd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_zx0_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_zx0_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_zx7b_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_sfx_zx7b_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_shrinkler_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_shrinkler_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_snappy_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_snappy_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zstd_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx0_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_zx7b_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_blz_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_blz_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_csc_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_csc_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_density_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_density_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_doboz_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_exo_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_exo_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzav_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_lzfse_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzfse_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_lzham_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzham_static_kexe_pwd_bin_start[] __attribute__((weak));

extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_static_bcj_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_static_bcj_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_static_bcj_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_static_bcj_pwd_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_static_kexe_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_static_kexe_bin_start[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_static_kexe_pwd_bin_end[] __attribute__((weak));
extern const unsigned char _binary_build_stubs_stub_lzma_static_kexe_pwd_bin_start[] __attribute__((weak));
extern const unsigned char
    _binary_build_stubs_stub_rnc_dynamic_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_rnc_static_kexe_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_static_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_lz4_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynexec_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynexec_bcj_bin_end[];

extern const unsigned char _binary_build_stubs_stub_rnc_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_rnc_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_static_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynexec_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynexec_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_rnc_dynamic_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_rnc_static_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_rnc_static_bcj_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_nz1_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_nz1_static_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_nz1_static_bcj_pwd_bin_end[];

void select_embedded_stub(const char *codec, int mode, int password,
                          const unsigned char **start,
                          const unsigned char **end) {
  const unsigned char *s = NULL, *e = NULL;
  int is_dyn = (mode == 1) ? 1 : 0;
  int is_dynexec = (mode == 2) ? 1 : 0;

  if (is_dynexec) {
    if (password) {
      if (strcmp(codec, "lz4") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_lz4_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_lz4_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_lz4_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_lz4_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "rnc") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_rnc_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_rnc_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_rnc_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_rnc_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "apultra") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_apultra_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_apultra_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_apultra_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_apultra_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "lzav") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_lzav_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_lzav_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzav_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_lzav_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "zstd") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_zstd_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_zstd_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_zstd_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_zstd_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "blz") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_blz_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_blz_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_blz_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_blz_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "zx7b") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_zx7b_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_zx7b_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_zx7b_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_zx7b_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "zx0") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_zx0_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_zx0_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_zx0_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_zx0_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "exo") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_exo_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_exo_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_exo_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_exo_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "pp") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_pp_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_pp_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_pp_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_pp_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "qlz") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_qlz_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_qlz_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_qlz_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_qlz_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "doboz") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_doboz_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_doboz_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_doboz_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_doboz_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "snappy") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_snappy_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_snappy_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_snappy_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_snappy_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "lzma") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_lzma_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_lzma_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzma_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_lzma_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "shrinkler") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_shrinkler_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_shrinkler_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_shrinkler_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_shrinkler_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "sc") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_sc_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_sc_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_sc_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_sc_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "lzsa") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_lzsa_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_lzsa_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzsa_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_lzsa_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "density") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_density_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_density_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_density_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_density_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "lzham") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_lzham_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_lzham_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzham_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_lzham_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "lzfse") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_lzfse_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_lzfse_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzfse_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_lzfse_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "csc") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_csc_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_csc_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_csc_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_csc_dynexec_kexe_pwd_bin_end;
        }
      } else if (strcmp(codec, "nz1") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_nz1_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_nz1_dynexec_bcj_pwd_bin_end;
        } else {
          s = _binary_build_stubs_stub_nz1_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_nz1_dynexec_kexe_pwd_bin_end;
        }
      }
    } else {
      if (strcmp(codec, "lz4") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_lz4_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_lz4_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_lz4_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_lz4_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "rnc") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_rnc_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_rnc_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_rnc_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_rnc_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "apultra") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_apultra_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_apultra_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_apultra_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_apultra_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "lzav") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_lzav_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_lzav_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzav_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_lzav_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "zstd") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_zstd_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_zstd_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_zstd_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_zstd_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "blz") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_blz_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_blz_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_blz_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_blz_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "zx7b") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_zx7b_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_zx7b_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_zx7b_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_zx7b_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "zx0") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_zx0_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_zx0_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_zx0_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_zx0_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "exo") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_exo_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_exo_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_exo_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_exo_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "pp") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_pp_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_pp_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_pp_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_pp_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "qlz") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_qlz_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_qlz_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_qlz_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_qlz_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "doboz") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_doboz_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_doboz_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_doboz_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_doboz_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "snappy") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_snappy_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_snappy_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_snappy_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_snappy_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "lzma") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_lzma_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_lzma_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzma_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_lzma_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "shrinkler") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_shrinkler_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_shrinkler_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_shrinkler_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_shrinkler_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "sc") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_sc_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_sc_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_sc_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_sc_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "lzsa") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_lzsa_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_lzsa_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzsa_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_lzsa_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "density") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ) {
          s = _binary_build_stubs_stub_density_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_density_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_density_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_density_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "lzham") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_lzham_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_lzham_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzham_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_lzham_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "lzfse") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_lzfse_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_lzfse_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_lzfse_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_lzfse_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "csc") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_csc_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_csc_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_csc_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_csc_dynexec_kexe_bin_end;
        }
      } else if (strcmp(codec, "nz1") == 0) {
        if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
          s = _binary_build_stubs_stub_nz1_dynexec_bcj_bin_start;
          e = _binary_build_stubs_stub_nz1_dynexec_bcj_bin_end;
        } else {
          s = _binary_build_stubs_stub_nz1_dynexec_kexe_bin_start;
          e = _binary_build_stubs_stub_nz1_dynexec_kexe_bin_end;
        }
      }
    }

    *start = s;
    *end = e;
    return;
  }

  if (password) {
    if (strcmp(codec, "lz4") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        if (is_dynexec) {
          s = _binary_build_stubs_stub_lz4_dynexec_bcj_pwd_bin_start;
          e = _binary_build_stubs_stub_lz4_dynexec_bcj_pwd_bin_end;
        } else {
          s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_bcj_pwd_bin_start
                     : _binary_build_stubs_stub_lz4_static_bcj_pwd_bin_start;
          e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_bcj_pwd_bin_end
                     : _binary_build_stubs_stub_lz4_static_bcj_pwd_bin_end;
        }
      } else {
        if (is_dynexec) {
          s = _binary_build_stubs_stub_lz4_dynexec_kexe_pwd_bin_start;
          e = _binary_build_stubs_stub_lz4_dynexec_kexe_pwd_bin_end;
        } else {
          s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_start
                     : _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_start;
          e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_end
                     : _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_end;
        }
      }
    } else if (strcmp(codec, "rnc") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
        s = is_dyn ? _binary_build_stubs_stub_rnc_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_rnc_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_rnc_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_rnc_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_rnc_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_rnc_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_rnc_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_rnc_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "apultra") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_apultra_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_apultra_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_apultra_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_apultra_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_apultra_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_apultra_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_apultra_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_apultra_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "lzav") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_lzav_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_lzav_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_lzav_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_lzav_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "zstd") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_zstd_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_zstd_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_zstd_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_zstd_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "blz") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_blz_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_blz_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_blz_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_blz_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_blz_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_blz_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_blz_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_blz_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "zx7b") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_zx7b_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_zx7b_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_zx7b_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_zx7b_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_zx7b_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_zx7b_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_zx7b_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_zx7b_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "zx0") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_zx0_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_zx0_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_zx0_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_zx0_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_zx0_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_zx0_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_zx0_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_zx0_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "blz") == 0) {
      s = is_dyn ? _binary_build_stubs_stub_blz_dynamic_kexe_pwd_bin_start
                 : _binary_build_stubs_stub_blz_static_kexe_pwd_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_blz_dynamic_kexe_pwd_bin_end
                 : _binary_build_stubs_stub_blz_static_kexe_pwd_bin_end;
    } else if (strcmp(codec, "exo") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_exo_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_exo_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_exo_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_exo_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_exo_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_exo_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_exo_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_exo_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "pp") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_pp_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_pp_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_pp_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_pp_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_pp_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_pp_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_pp_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_pp_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "qlz") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_qlz_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_qlz_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_qlz_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_qlz_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_qlz_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_qlz_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_qlz_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_qlz_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "lzav") == 0) {
      s = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_kexe_pwd_bin_start
                 : _binary_build_stubs_stub_lzav_static_kexe_pwd_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_kexe_pwd_bin_end
                 : _binary_build_stubs_stub_lzav_static_kexe_pwd_bin_end;
    } else if (strcmp(codec, "doboz") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_doboz_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_doboz_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_doboz_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_doboz_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_doboz_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_doboz_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_doboz_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_doboz_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "snappy") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_snappy_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_snappy_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_snappy_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_snappy_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_snappy_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_snappy_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_snappy_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_snappy_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "lzma") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_lzma_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_lzma_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzma_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_lzma_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_lzma_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_lzma_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzma_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_lzma_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "zstd") == 0) {
      s = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_kexe_pwd_bin_start
                 : _binary_build_stubs_stub_zstd_static_kexe_pwd_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_kexe_pwd_bin_end
                 : _binary_build_stubs_stub_zstd_static_kexe_pwd_bin_end;
    } else if (strcmp(codec, "shrinkler") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn
                ? _binary_build_stubs_stub_shrinkler_dynamic_bcj_pwd_bin_start
                : _binary_build_stubs_stub_shrinkler_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_shrinkler_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_shrinkler_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn
                ? _binary_build_stubs_stub_shrinkler_dynamic_kexe_pwd_bin_start
                : _binary_build_stubs_stub_shrinkler_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_shrinkler_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_shrinkler_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "sc") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_sc_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_sc_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_sc_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_sc_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_sc_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_sc_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_sc_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_sc_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "lzsa") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_lzsa_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_lzsa_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzsa_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_lzsa_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_lzsa_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_lzsa_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzsa_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_lzsa_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "density") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_density_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_density_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_density_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_density_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_density_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_density_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_density_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_density_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "lzham") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
        s = is_dyn ? _binary_build_stubs_stub_lzham_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_lzham_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzham_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_lzham_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_lzham_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_lzham_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzham_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_lzham_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "lzfse") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
        s = is_dyn ? _binary_build_stubs_stub_lzfse_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_lzfse_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzfse_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_lzfse_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_lzfse_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_lzfse_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lzfse_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_lzfse_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "csc") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
        s = is_dyn ? _binary_build_stubs_stub_csc_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_csc_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_csc_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_csc_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_csc_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_csc_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_csc_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_csc_static_kexe_pwd_bin_end;
      }
    } else if (strcmp(codec, "nz1") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
        s = is_dyn ? _binary_build_stubs_stub_nz1_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_nz1_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_nz1_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_nz1_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_nz1_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_nz1_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_nz1_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_nz1_static_kexe_pwd_bin_end;
      }
    } else {
      // Fallback: use LZ4 password stub (Kanzi EXE)
      s = NULL;
      e = NULL;
    }
  } else if (strcmp(codec, "lz4") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      if (is_dynexec) {
        s = _binary_build_stubs_stub_lz4_dynexec_bcj_bin_start;
        e = _binary_build_stubs_stub_lz4_dynexec_bcj_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_bcj_bin_start
                   : _binary_build_stubs_stub_lz4_static_bcj_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_bcj_bin_end
                   : _binary_build_stubs_stub_lz4_static_bcj_bin_end;
      }
    } else {
      if (is_dynexec) {
        s = _binary_build_stubs_stub_lz4_dynexec_kexe_bin_start;
        e = _binary_build_stubs_stub_lz4_dynexec_kexe_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_bin_start
                   : _binary_build_stubs_stub_lz4_static_kexe_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_bin_end
                   : _binary_build_stubs_stub_lz4_static_kexe_bin_end;
      }
    }
  } else if (strcmp(codec, "rnc") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
      s = is_dyn ? _binary_build_stubs_stub_rnc_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_rnc_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_rnc_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_rnc_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_rnc_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_rnc_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_rnc_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_rnc_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "apultra") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_apultra_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_apultra_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_apultra_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_apultra_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_apultra_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_apultra_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_apultra_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_apultra_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "lzav") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_lzav_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_lzav_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_lzav_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_lzav_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "zstd") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_zstd_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_zstd_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_zstd_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_zstd_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "blz") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_blz_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_blz_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_blz_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_blz_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_blz_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_blz_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_blz_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_blz_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "zx7b") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_zx7b_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_zx7b_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_zx7b_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_zx7b_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_zx7b_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_zx7b_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_zx7b_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_zx7b_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "zx0") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_zx0_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_zx0_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_zx0_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_zx0_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_zx0_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_zx0_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_zx0_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_zx0_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "blz") == 0) {
    s = is_dyn ? _binary_build_stubs_stub_blz_dynamic_kexe_bin_start
               : _binary_build_stubs_stub_blz_static_kexe_bin_start;
    e = is_dyn ? _binary_build_stubs_stub_blz_dynamic_kexe_bin_end
               : _binary_build_stubs_stub_blz_static_kexe_bin_end;
  } else if (strcmp(codec, "exo") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_exo_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_exo_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_exo_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_exo_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_exo_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_exo_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_exo_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_exo_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "pp") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_pp_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_pp_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_pp_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_pp_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_pp_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_pp_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_pp_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_pp_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "qlz") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_qlz_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_qlz_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_qlz_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_qlz_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_qlz_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_qlz_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_qlz_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_qlz_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "lzav") == 0) {
    s = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_kexe_bin_start
               : _binary_build_stubs_stub_lzav_static_kexe_bin_start;
    e = is_dyn ? _binary_build_stubs_stub_lzav_dynamic_kexe_bin_end
               : _binary_build_stubs_stub_lzav_static_kexe_bin_end;
  } else if (strcmp(codec, "doboz") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_doboz_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_doboz_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_doboz_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_doboz_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_doboz_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_doboz_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_doboz_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_doboz_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "snappy") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_snappy_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_snappy_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_snappy_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_snappy_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_snappy_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_snappy_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_snappy_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_snappy_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "lzma") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_lzma_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_lzma_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzma_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_lzma_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_lzma_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_lzma_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzma_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_lzma_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "zstd") == 0) {
    s = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_kexe_bin_start
               : _binary_build_stubs_stub_zstd_static_kexe_bin_start;
    e = is_dyn ? _binary_build_stubs_stub_zstd_dynamic_kexe_bin_end
               : _binary_build_stubs_stub_zstd_static_kexe_bin_end;
  } else if (strcmp(codec, "shrinkler") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_shrinkler_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_shrinkler_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_shrinkler_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_shrinkler_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_shrinkler_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_shrinkler_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_shrinkler_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_shrinkler_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "sc") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_sc_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_sc_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_sc_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_sc_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_sc_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_sc_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_sc_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_sc_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "lzsa") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_lzsa_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_lzsa_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzsa_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_lzsa_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_lzsa_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_lzsa_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzsa_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_lzsa_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "density") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_density_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_density_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_density_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_density_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_density_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_density_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_density_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_density_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "lzham") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
      s = is_dyn ? _binary_build_stubs_stub_lzham_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_lzham_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzham_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_lzham_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_lzham_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_lzham_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzham_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_lzham_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "lzfse") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
      s = is_dyn ? _binary_build_stubs_stub_lzfse_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_lzfse_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzfse_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_lzfse_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_lzfse_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_lzfse_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lzfse_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_lzfse_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "csc") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
      s = is_dyn ? _binary_build_stubs_stub_csc_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_csc_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_csc_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_csc_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_csc_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_csc_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_csc_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_csc_static_kexe_bin_end;
    }
  } else if (strcmp(codec, "nz1") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) {
      s = is_dyn ? _binary_build_stubs_stub_nz1_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_nz1_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_nz1_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_nz1_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_nz1_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_nz1_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_nz1_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_nz1_static_kexe_bin_end;
    }
  } else {
    // Fallback: use LZ4 password stub (Kanzi EXE)
    s = NULL;
    e = NULL;
  }
  *start = s;
  *end = e;
}

 void select_embedded_sfx_stub(const char *codec, const unsigned char **start,
                               const unsigned char **end) {
   const unsigned char *s = NULL, *e = NULL;

   if (strcmp(codec, "lz4") == 0) {
     s = _binary_build_stubs_stub_sfx_lz4_bin_start;
     e = _binary_build_stubs_stub_sfx_lz4_bin_end;
   } else if (strcmp(codec, "rnc") == 0) {
     s = _binary_build_stubs_stub_sfx_rnc_bin_start;
     e = _binary_build_stubs_stub_sfx_rnc_bin_end;
   } else if (strcmp(codec, "apultra") == 0) {
     s = _binary_build_stubs_stub_sfx_apultra_bin_start;
     e = _binary_build_stubs_stub_sfx_apultra_bin_end;
   } else if (strcmp(codec, "lzav") == 0) {
     s = _binary_build_stubs_stub_sfx_lzav_bin_start;
     e = _binary_build_stubs_stub_sfx_lzav_bin_end;
   } else if (strcmp(codec, "zstd") == 0) {
     s = _binary_build_stubs_stub_sfx_zstd_bin_start;
     e = _binary_build_stubs_stub_sfx_zstd_bin_end;
   } else if (strcmp(codec, "blz") == 0) {
     s = _binary_build_stubs_stub_sfx_blz_bin_start;
     e = _binary_build_stubs_stub_sfx_blz_bin_end;
   } else if (strcmp(codec, "doboz") == 0) {
     s = _binary_build_stubs_stub_sfx_doboz_bin_start;
     e = _binary_build_stubs_stub_sfx_doboz_bin_end;
   } else if (strcmp(codec, "exo") == 0) {
     s = _binary_build_stubs_stub_sfx_exo_bin_start;
     e = _binary_build_stubs_stub_sfx_exo_bin_end;
   } else if (strcmp(codec, "pp") == 0) {
     s = _binary_build_stubs_stub_sfx_pp_bin_start;
     e = _binary_build_stubs_stub_sfx_pp_bin_end;
   } else if (strcmp(codec, "qlz") == 0) {
     s = _binary_build_stubs_stub_sfx_qlz_bin_start;
     e = _binary_build_stubs_stub_sfx_qlz_bin_end;
   } else if (strcmp(codec, "snappy") == 0) {
     s = _binary_build_stubs_stub_sfx_snappy_bin_start;
     e = _binary_build_stubs_stub_sfx_snappy_bin_end;
   } else if (strcmp(codec, "shrinkler") == 0) {
     s = _binary_build_stubs_stub_sfx_shrinkler_bin_start;
     e = _binary_build_stubs_stub_sfx_shrinkler_bin_end;
   } else if (strcmp(codec, "lzma") == 0) {
     // minlzma is stored as lzma codec, marker is handled in archive format
     // SFX stub still compiled with CODEC_LZMA.
     s = _binary_build_stubs_stub_sfx_lzma_bin_start;
     e = _binary_build_stubs_stub_sfx_lzma_bin_end;
   } else if (strcmp(codec, "zx7b") == 0) {
     s = _binary_build_stubs_stub_sfx_zx7b_bin_start;
     e = _binary_build_stubs_stub_sfx_zx7b_bin_end;
   } else if (strcmp(codec, "zx0") == 0) {
     s = _binary_build_stubs_stub_sfx_zx0_bin_start;
     e = _binary_build_stubs_stub_sfx_zx0_bin_end;
   } else if (strcmp(codec, "density") == 0) {
     s = _binary_build_stubs_stub_sfx_density_bin_start;
     e = _binary_build_stubs_stub_sfx_density_bin_end;
   } else if (strcmp(codec, "sc") == 0) {
     s = _binary_build_stubs_stub_sfx_sc_bin_start;
     e = _binary_build_stubs_stub_sfx_sc_bin_end;
   } else if (strcmp(codec, "lzsa") == 0) {
     s = _binary_build_stubs_stub_sfx_lzsa_bin_start;
     e = _binary_build_stubs_stub_sfx_lzsa_bin_end;
   } else if (strcmp(codec, "lzham") == 0) {
     s = _binary_build_stubs_stub_sfx_lzham_bin_start;
     e = _binary_build_stubs_stub_sfx_lzham_bin_end;
   } else if (strcmp(codec, "lzfse") == 0) {
     s = _binary_build_stubs_stub_sfx_lzfse_bin_start;
     e = _binary_build_stubs_stub_sfx_lzfse_bin_end;
   } else if (strcmp(codec, "csc") == 0) {
     s = _binary_build_stubs_stub_sfx_csc_bin_start;
     e = _binary_build_stubs_stub_sfx_csc_bin_end;
   } else if (strcmp(codec, "nz1") == 0) {
     s = _binary_build_stubs_stub_sfx_nz1_bin_start;
     e = _binary_build_stubs_stub_sfx_nz1_bin_end;
   }

   *start = s;
   *end = e;
 }
