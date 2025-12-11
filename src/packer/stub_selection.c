#include "stub_selection.h"

// Embedded stub binaries (generated with objcopy/ld -b binary).
// Symbols follow the convention: _binary_build_stubs_<file>_start/end
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_apultra_dynamic_kexe_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_static_kexe_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_zx7b_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx7b_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_exo_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_exo_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_exo_static_kexe_bin_end[];

extern const unsigned char _binary_build_stubs_stub_pp_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_pp_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_qlz_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_qlz_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_qlz_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_lzav_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzav_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_doboz_dynamic_kexe_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_doboz_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_snappy_dynamic_kexe_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_static_kexe_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_lzma_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzma_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_zstd_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zstd_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_shrinkler_dynamic_kexe_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_static_kexe_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_zx0_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx0_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_blz_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_blz_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_blz_static_kexe_bin_end[];

// Password-enabled stubs
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_lz4_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lz4_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lz4_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_apultra_dynamic_bcj_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_dynamic_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_static_bcj_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_lzav_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzav_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzav_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzav_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_zstd_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zstd_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zstd_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zstd_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_static_bcj_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_blz_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_blz_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_blz_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_blz_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_blz_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_blz_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_blz_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_blz_static_bcj_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_exo_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_exo_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_exo_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_exo_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_exo_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_exo_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_exo_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_exo_static_bcj_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_pp_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_pp_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_pp_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_pp_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_pp_static_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_pp_static_bcj_pwd_bin_end[];

extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_qlz_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_qlz_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_qlz_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_qlz_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_qlz_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_qlz_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_qlz_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_snappy_dynamic_bcj_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_dynamic_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_snappy_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_lzma_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzma_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzma_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzma_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_shrinkler_dynamic_bcj_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_dynamic_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_static_bcj_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_zx7b_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx7b_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx7b_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx7b_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_zx0_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx0_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_static_kexe_bin_end[];

extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_zx0_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_zx0_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zx0_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zx0_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zx0_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zx0_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_doboz_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_doboz_dynamic_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_doboz_static_bcj_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_static_bcj_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_apultra_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_apultra_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zx7b_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_exo_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_exo_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_exo_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_exo_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_pp_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_pp_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_pp_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_pp_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_qlz_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_qlz_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_qlz_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_qlz_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzav_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_doboz_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_snappy_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzma_static_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zstd_static_kexe_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_shrinkler_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_shrinkler_static_kexe_pwd_bin_end[];
// ZX0 password-enabled stubs
extern const unsigned char
    _binary_build_stubs_stub_zx0_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zx0_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_zx0_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_zx0_static_kexe_pwd_bin_end[];
// BriefLZ password-enabled stubs
extern const unsigned char
    _binary_build_stubs_stub_blz_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_blz_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_blz_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_blz_static_kexe_pwd_bin_end[];

// StoneCracker stubs
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_kexe_bin_end[];
extern const unsigned char _binary_build_stubs_stub_sc_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_static_kexe_bin_end[];

extern const unsigned char _binary_build_stubs_stub_sc_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_sc_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_static_bcj_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_sc_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_sc_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_sc_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_sc_static_kexe_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_sc_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_sc_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_sc_static_bcj_pwd_bin_start[];
extern const unsigned char _binary_build_stubs_stub_sc_static_bcj_pwd_bin_end[];

// LZSA stubs
extern const unsigned char
    _binary_build_stubs_stub_lzsa_dynamic_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_kexe_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzsa_static_kexe_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzsa_static_kexe_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_lzsa_dynamic_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzsa_dynamic_bcj_bin_end[];
extern const unsigned char _binary_build_stubs_stub_lzsa_static_bcj_bin_start[];
extern const unsigned char _binary_build_stubs_stub_lzsa_static_bcj_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_lzsa_dynamic_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzsa_dynamic_kexe_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzsa_static_kexe_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzsa_static_kexe_pwd_bin_end[];

extern const unsigned char
    _binary_build_stubs_stub_lzsa_dynamic_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzsa_dynamic_bcj_pwd_bin_end[];
extern const unsigned char
    _binary_build_stubs_stub_lzsa_static_bcj_pwd_bin_start[];
extern const unsigned char
    _binary_build_stubs_stub_lzsa_static_bcj_pwd_bin_end[];

// Select the embedded stub bytes for (codec,dynamic,password)
void select_embedded_stub(const char *codec, int dynamic, int password,
                          const unsigned char **start,
                          const unsigned char **end) {
  const unsigned char *s = NULL, *e = NULL;
  int is_dyn = dynamic ? 1 : 0;

  if (password) {
    if (strcmp(codec, "lz4") == 0) {
      if (g_exe_filter == EXE_FILTER_BCJ) {
        s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_bcj_pwd_bin_start
                   : _binary_build_stubs_stub_lz4_static_bcj_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_bcj_pwd_bin_end
                   : _binary_build_stubs_stub_lz4_static_bcj_pwd_bin_end;
      } else {
        s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_start
                   : _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_start;
        e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_end
                   : _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_end;
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
    } else {
      // Fallback: use LZ4 password stub (Kanzi EXE)
      s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_start
                 : _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_pwd_bin_end
                 : _binary_build_stubs_stub_lz4_static_kexe_pwd_bin_end;
    }
  } else if (strcmp(codec, "lz4") == 0) {
    if (g_exe_filter == EXE_FILTER_BCJ) {
      s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_bcj_bin_start
                 : _binary_build_stubs_stub_lz4_static_bcj_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_bcj_bin_end
                 : _binary_build_stubs_stub_lz4_static_bcj_bin_end;
    } else {
      s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_bin_start
                 : _binary_build_stubs_stub_lz4_static_kexe_bin_start;
      e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_bin_end
                 : _binary_build_stubs_stub_lz4_static_kexe_bin_end;
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
  } else {
    // Fallback: use LZ4 password stub (Kanzi EXE)
    s = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_bin_start
               : _binary_build_stubs_stub_lz4_static_kexe_bin_start;
    e = is_dyn ? _binary_build_stubs_stub_lz4_dynamic_kexe_bin_end
               : _binary_build_stubs_stub_lz4_static_kexe_bin_end;
  }
  *start = s;
  *end = e;
}
