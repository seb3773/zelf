// Force O3 optimization for compression functions (performance critical)
// while packer uses -Os globally for size
#pragma GCC optimize("O3")

#include "compression.h"
#include "LzmaDec.h"
#include "LzmaLib.h"
#include "blz_compress_wrapper.h"
#include "buf.h"
#include "density_compress_wrapper.h"
#include "doboz_compress_wrapper.h"
#include "elf_utils.h" // for fail()
#include "exo_helper.h"
#include "log.h"
#include "lzav_compress.h"
#include "lzham_compressor.h"
#include "lzsa2_compress.h"
#include "pp_compress.h"
#include "quicklz.h"
#include "rnc_compress_wrapper.h"
#include "sc_compress.h"
#include "shrinkler_comp.h"
#include "snappy_compress.h"
#include "zelf_packer.h"
#include "zstd.h"
#include "zx0_compress_wrapper.h"
#include "zx7b_compress.h"
#include "../compressors/lzfse/lzfse.h"
#include "../compressors/csc/csc_compressor.h"
#include "../compressors/nz1/nz1_compressor.h"
#include <lz4.h>
#include <lz4hc.h>
#include <limits.h>
#include <sys/resource.h>

// Console progress (header-only) – include only progress bar #8, use raw clone
// to avoid pthread
#define CONSOLE_USE_PROGRESS_8
#define CONSOLE_USE_RAW_CLONE
#define CONSOLE_PROGRESS_IMPLEMENTATION
// Rename underlying symbols to *_real so we can provide gating wrappers
#define start_progress start_progress_real
#define update_progress update_progress_real
#define stop_progress stop_progress_real
#include "console_progress.h"
#undef start_progress
#undef update_progress
#undef stop_progress

// Gated wrappers honoring --no-progress (g_no_progress from packer_defs.h)
static inline void start_progress(const char *msg, int id) {
  if (!g_no_progress)
    start_progress_real(msg, id);
}

// Common error message (deduplicated)
static const char *ERR_COMP = "Compression error";
static inline void update_progress(int pct, int id) {
  if (!g_no_progress)
    update_progress_real(pct, id);
}
static inline void stop_progress(int id) {
  if (!g_no_progress)
    stop_progress_real(id);
}

// Helper to clear the progress line safely
static inline void progress_clear_line(void) {
  if (!g_no_progress) {
    usleep(200000);
    printf("\033[1A\033[2K\r");
    fflush(stdout);
  }
}

// Progress UI gating (set by --no-progress in CLI)
// Note: g_no_progress is declared in packer_defs.h (via compression.h)
#define PROG_START(msg, id)                                                    \
  do {                                                                         \
    if (!g_no_progress)                                                        \
      start_progress((msg), (id));                                             \
  } while (0)
#define PROG_UPDATE(pct, id)                                                   \
  do {                                                                         \
    if (!g_no_progress)                                                        \
      update_progress((pct), (id));                                            \
  } while (0)
#define PROG_STOP(id)                                                          \
  do {                                                                         \
    if (!g_no_progress)                                                        \
      stop_progress((id));                                                     \
  } while (0)
#define PROG_CLEAR_LINE()                                                      \
  do {                                                                         \
    if (!g_no_progress) {                                                      \
      usleep(200000);                                                          \
      printf("\033[1A\033[2K\r");                                              \
      fflush(stdout);                                                          \
    }                                                                          \
  } while (0)

// ZX7B progress adapter: maps processed/total to progress bar #8
static void zx7b_progress_cb(size_t processed, size_t total) {
  if (!total)
    return;
  int pct = (int)((processed * 100ULL) / total);
  if (pct > 100)
    pct = 100;
  update_progress(pct, 8);
}

// EXO progress adapter: absolute percentage [0..100] per phase
extern void exo_set_progress_cb(void (*cb)(int));
static void exo_progress_cb(int pct) {
  static int last = -1;
  if (pct < 0)
    pct = 0;
  if (pct > 100)
    pct = 100;
  if (pct <= last)
    return; // monotonic guard to prevent resets/glitches
  last = pct;
  update_progress(pct, 8);
}

// Zstd progress adapter: processed/total to progress bar #8
static void zstd_progress_cb(size_t processed, size_t total) {
  if (!total)
    return;
  int pct = (int)((processed * 100ULL) / total);
  if (pct > 100)
    pct = 100;
  update_progress(pct, 8);
}

// ZX0 progress adapter: processed/total to progress bar #8
static void zx0_progress_cb(size_t processed, size_t total) {
  if (!total)
    return;
  int pct = (int)((processed * 100ULL) / total);
  if (pct > 100)
    pct = 100;
  update_progress(pct, 8);
}

// Snappy progress adapter: processed/total to progress bar #8
static void snappy_progress_cb(size_t processed, size_t total) {
  if (!total)
    return;
  int pct = (int)((processed * 100ULL) / total);
  if (pct > 100)
    pct = 100;
  update_progress(pct, 8);
}

// Shrinkler progress adapter: absolute percentage [0..100]
static int g_shrinkler_last_pct = -1;
static void shrinkler_progress_cb(int pct) {
  if (pct < 0)
    pct = 0;
  if (pct > 100)
    pct = 100;
  if (pct <= g_shrinkler_last_pct)
    return;
  g_shrinkler_last_pct = pct;
  update_progress(pct, 8);
}

static size_t get_available_ram() {
  FILE *fp = fopen("/proc/meminfo", "r");
  if (fp) {
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
      if (strncmp(line, "MemAvailable:", 13) == 0) {
        size_t kb;
        if (sscanf(line + 13, "%zu", &kb) == 1) {
          fclose(fp);
          return kb * 1024;
        }
      }
    }
    fclose(fp);
  }
  // Fallback
  long pages = sysconf(_SC_AVPHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages > 0 && page_size > 0)
    return (size_t)pages * (size_t)page_size;
  return (size_t)-1; // Unknown
}

static size_t get_rlimit_as_bytes(void) {
  struct rlimit rl;
  if (getrlimit(RLIMIT_AS, &rl) != 0)
    return (size_t)-1;
  if (rl.rlim_cur == RLIM_INFINITY)
    return (size_t)-1;
  if (rl.rlim_cur > (rlim_t)SIZE_MAX)
    return (size_t)SIZE_MAX;
  return (size_t)rl.rlim_cur;
}

// --- Lightweight Apultra declarations (avoid including shrink.h and
// libdivsufsort headers) --- We only need the compressor entry points; pass
// NULL for optional callbacks/stats.
size_t apultra_get_max_compressed_size(size_t nInputSize);
size_t apultra_compress(const unsigned char *pInputData,
                        unsigned char *pOutBuffer, const size_t nInputSize,
                        const size_t nMaxOutBufferSize,
                        const unsigned int nFlags, const size_t nMaxWindowSize,
                        const size_t nDictionarySize,
                        void (*progress)(long long nOriginalSize,
                                         long long nCompressedSize),
                        void *pStats);

// Simple progress display for Apultra using console_progress bar #8
static size_t g_apultra_progress_total = 0;
static int g_apultra_last_pct = -1;
static void apultra_progress_cb(long long nOriginalSize,
                                long long nCompressedSize) {
  (void)nCompressedSize; // not used for percentage
  if (!g_apultra_progress_total)
    return;
  int pct =
      (int)((nOriginalSize * 100LL) / (long long)g_apultra_progress_total);
  if (pct > 100)
    pct = 100;
  if (pct != g_apultra_last_pct) {
    g_apultra_last_pct = pct;
    update_progress(pct, 8);
  }
  if ((size_t)nOriginalSize >= g_apultra_progress_total) {
    update_progress(100, 8);
  }
}

int compress_data_with_codec(const char *codec, const unsigned char *input,
                             size_t input_size, unsigned char **output,
                             int *output_size) {
  size_t size_to_compress = input_size;
  const unsigned char *comp_input = input;
  unsigned char *compressed = NULL;
  int comp_size = 0;

  int use_apultra = (strcmp(codec, "apultra") == 0);
  int use_zx7b = (strcmp(codec, "zx7b") == 0);
  int use_exo = (strcmp(codec, "exo") == 0);
  int use_zx0 = (strcmp(codec, "zx0") == 0);
  int use_blz = (strcmp(codec, "blz") == 0);
  int use_pp = (strcmp(codec, "pp") == 0);
  int use_snappy = (strcmp(codec, "snappy") == 0);
  int use_doboz = (strcmp(codec, "doboz") == 0);
  int use_qlz = (strcmp(codec, "qlz") == 0);
  int use_lzav = (strcmp(codec, "lzav") == 0);
  int use_lzma = (strcmp(codec, "lzma") == 0);
  int use_zstd = (strcmp(codec, "zstd") == 0);
  int use_shrinkler = (strcmp(codec, "shrinkler") == 0);
  int use_sc = (strcmp(codec, "sc") == 0);
  int use_lzsa = (strcmp(codec, "lzsa") == 0);
  int use_density = (strcmp(codec, "density") == 0);
  int use_lzham = (strcmp(codec, "lzham") == 0);
  int use_rnc = (strcmp(codec, "rnc") == 0);
  int use_lzfse = (strcmp(codec, "lzfse") == 0);
  int use_csc = (strcmp(codec, "csc") == 0);
  int use_nz1 = (strcmp(codec, "nz1") == 0);

  const char *comp_name = "LZ4";
  if (use_apultra)
    comp_name = "Apultra";
  else if (use_zx7b)
    comp_name = "ZX7B";
  else if (use_zx0)
    comp_name = "ZX0";
  else if (use_blz)
    comp_name = "BriefLZ";
  else if (use_exo)
    comp_name = "EXO";
  else if (use_pp)
    comp_name = "PP";
  else if (use_snappy)
    comp_name = "Snappy";
  else if (use_doboz)
    comp_name = "DoboZ";
  else if (use_qlz)
    comp_name = "QuickLZ";
  else if (use_lzav)
    comp_name = "LZAV";
  else if (use_lzma)
    comp_name = "LZMA";
  else if (use_zstd)
    comp_name = "Zstd";
  else if (use_shrinkler)
    comp_name = "Shrinkler";
  else if (use_sc)
    comp_name = "StoneCracker";
  else if (use_lzsa)
    comp_name = "LZSA2";
  else if (use_density)
    comp_name = "Density";
  else if (use_lzham)
    comp_name = "LZHAM";
  else if (use_rnc)
    comp_name = "RNC";
  else if (use_lzfse)
    comp_name = "LZFSE";
  else if (use_csc)
    comp_name = "CSC";
  else if (use_nz1)
    comp_name = "NZ1";

  VPRINTF("[%s%s%s] Compressing %s: %zu bytes...\n", PK_ARROW, PK_SYM_ARROW,
          PK_RES, comp_name, size_to_compress);

  if (use_apultra) {
    size_t max = apultra_get_max_compressed_size(size_to_compress);
    compressed = malloc(max);
    if (!compressed)
      fail("malloc compressed");
    g_apultra_progress_total = size_to_compress;
    g_apultra_last_pct = -1;
    start_progress("Apultra compression", 8);
    size_t tmp = apultra_compress((const unsigned char *)comp_input, compressed,
                                  size_to_compress, max, 0, 0, 0,
                                  apultra_progress_cb, NULL);
    if (tmp == (size_t)-1) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "Apultra");
      return 1;
    }
    if (tmp > 0x7fffffff) {
      printf("[%s%s%s] Compressed data too large\n", PK_ERR, PK_SYM_ERR,
             PK_RES);
      return 1;
    }
    comp_size = (int)tmp;
    stop_progress(8);
    progress_clear_line();
    VPRINTF("[%s%s%s] Apultra compression: %zu → %d bytes "
            "(%.1f%%)\n",
            PK_INFO, PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_zx7b) {
    // ZX7B: output buffer allocated by wrapper; free() later
    start_progress("ZX7B compression", 8);
    zx7b_set_progress_cb(zx7b_progress_cb);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    int rc = zx7b_compress_c((const unsigned char *)comp_input,
                             size_to_compress, &out, &out_sz);
    zx7b_set_progress_cb(NULL);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc != 0 || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "ZX7B");
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] ZX7B compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_zx0) {
    // ZX0: output buffer allocated by wrapper; free() later
    start_progress("ZX0 compression", 8);
    zx0_set_progress_cb(zx0_progress_cb);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    int rc = zx0_compress_c((const unsigned char *)comp_input, size_to_compress,
                            &out, &out_sz);
    zx0_set_progress_cb(NULL);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc != 0 || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "ZX0");
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] ZX0 compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_lzfse) {
    start_progress("LZFSE compression", 8);
    update_progress(0, 8);
    if (size_to_compress > (size_t)INT32_MAX - 12u) {
      printf("[%s%s%s] Input too large for LZFSE\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }

    size_t bound = size_to_compress + 12u;
    unsigned char *out = (unsigned char *)malloc(bound ? bound : 1);
    if (!out) {
      printf("[%s%s%s] OOM LZFSE buffer\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }

    void *scratch = malloc(lzfse_encode_scratch_size());
    if (!scratch) {
      free(out);
      printf("[%s%s%s] OOM LZFSE scratch\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }

    size_t sz =
        lzfse_encode_buffer(out, bound, (const uint8_t *)comp_input,
                            size_to_compress, scratch);
    free(scratch);

    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();

    if (sz == 0 || sz > 0x7fffffff) {
      free(out);
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "LZFSE");
      return 1;
    }

    compressed = out;
    comp_size = (int)sz;
    VPRINTF("[%s%s%s] LZFSE compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_csc) {
    start_progress("CSC compression", 8);
    update_progress(0, 8);

    CSCCompressOptions opts;
    opts.level = 2;
    opts.dict_size = 1024u * 1024u;

    size_t bound = size_to_compress + 65536u + (size_to_compress >> 3) + 64u;
    if (bound > (size_t)INT32_MAX) {
      printf("[%s%s%s] Input too large for CSC\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }

    unsigned char *out = (unsigned char *)malloc(bound ? bound : 1);
    if (!out) {
      printf("[%s%s%s] OOM CSC buffer\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }

    size_t sz = CSC_Compress((const void *)comp_input, size_to_compress,
                             (void *)out, bound, &opts);

    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();

    if (sz == 0 || sz > 0x7fffffff) {
      free(out);
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "CSC");
      return 1;
    }

    compressed = out;
    comp_size = (int)sz;
    VPRINTF("[%s%s%s] CSC compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_nz1) {
    start_progress("NZ1 compression", 8);
    update_progress(0, 8);

    if (size_to_compress > (size_t)INT32_MAX / 2u) {
      printf("[%s%s%s] Input too large for NZ1\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }

    size_t bound = (size_to_compress << 1) + 14u;
    if (bound < size_to_compress)
      bound = size_to_compress + 14u;
    if (bound > (size_t)INT32_MAX)
      bound = (size_t)INT32_MAX;

    unsigned char *out = (unsigned char *)malloc(bound ? bound : 1);
    if (!out) {
      printf("[%s%s%s] OOM NZ1 buffer\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }

    size_t sz = nanozip_compress((const uint8_t *)comp_input, size_to_compress,
                                 (uint8_t *)out, bound, 0);

    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();

    if (sz == 0 || sz > 0x7fffffff) {
      free(out);
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "NZ1");
      return 1;
    }

    compressed = out;
    comp_size = (int)sz;
    VPRINTF("[%s%s%s] NZ1 compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_blz) {
    start_progress("BriefLZ compression", 8);
    update_progress(0, 8);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    int rc = blz_compress_c((const unsigned char *)comp_input, size_to_compress,
                            &out, &out_sz);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc != 0 || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "BriefLZ");
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] BriefLZ compression: %zu → %d bytes "
            "(%.1f%%)\n",
            PK_INFO, PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_exo) {
    // EXO: direct exomizer API using buf/crunch

    // Check RAM usage
    // Empirically determined safety factor: ~4800x + 128MB overhead
    // Small files (50KB) use ~250MB (5000x)
    // Medium files (500KB) use ~2GB (4000x)
    // Large files (2MB) use ~4.6GB (2200x)
    // We use a conservative estimate to prevent OOM
    size_t needed_ram = 128 * 1024 * 1024 + size_to_compress * 5000;
    size_t available_ram = get_available_ram();

    if (available_ram != (size_t)-1 && needed_ram > available_ram) {
      printf("\n[%s%s%s] Error: exomizer needs ~ %.2f Gb of RAM "
             "to compress this file, %.2f Gb available, aborted.\n",
             PK_ERR, PK_SYM_ERR, PK_RES,
             (double)needed_ram / (1024.0 * 1024.0 * 1024.0),
             (double)available_ram / (1024.0 * 1024.0 * 1024.0));
      return 1;
    }

    start_progress("EXO compression", 8);
    update_progress(0, 8);
    exo_set_progress_cb(exo_progress_cb);
    struct buf inbuf = STATIC_BUF_INIT;
    struct buf outbuf = STATIC_BUF_INIT;
    struct crunch_options options = CRUNCH_OPTIONS_DEFAULT;
    struct crunch_info info = STATIC_CRUNCH_INFO_INIT;
    buf_init(&inbuf);
    buf_init(&outbuf);
    if (buf_append(&inbuf, (const unsigned char *)comp_input,
                   size_to_compress) == NULL) {
      buf_free(&inbuf);
      buf_free(&outbuf);
      printf("[%s%s%s] EXO: OOM input buf\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    options.direction_forward = 1;
    options.max_passes = 3;
    enum log_level old_level = G_log_level;
    G_log_level = LOG_WARNING; // silence verbose output
    crunch(&inbuf, 0, NULL, &outbuf, &options, &info);
    G_log_level = old_level;
    int out_sz_i = buf_size(&outbuf);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (out_sz_i <= 0 || out_sz_i > 0x7fffffff) {
      buf_free(&inbuf);
      buf_free(&outbuf);
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "EXO");
      return 1;
    }
    unsigned char *out = (unsigned char *)malloc((size_t)out_sz_i);
    if (!out) {
      buf_free(&inbuf);
      buf_free(&outbuf);
      printf("[%s%s%s] EXO: OOM output\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    memcpy(out, buf_data(&outbuf), (size_t)out_sz_i);
    buf_free(&outbuf);
    buf_free(&inbuf);
    compressed = out;
    comp_size = out_sz_i;
    VPRINTF("[%s%s%s] EXO compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_pp) {
    // PowerPacker: vendored CLI compressor, returns allocated buffer
    start_progress("PowerPacker (PP20) compression", 8);
    update_progress(0, 8);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    int rc = pp_compress_c((const unsigned char *)comp_input, size_to_compress,
                           &out, &out_sz);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc != 0 || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "PowerPacker");
      if (out)
        free(out);
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] PowerPacker compression: %zu → %d bytes (%.1f%%)\n",
            PK_INFO, PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_doboz) {
    // DoboZ: allocate output buffer using max size and compress into it
    start_progress("DoboZ compression", 8);
    update_progress(0, 8);
    size_t max = doboz_get_max_compressed_size(size_to_compress);
    unsigned char *out = (unsigned char *)malloc(max);
    if (!out) {
      printf("[%s%s%s] OOM DoboZ\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    int rc = doboz_compress_c((const unsigned char *)comp_input,
                              size_to_compress, out, max);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc < 0 || rc > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "DoboZ");
      free(out);
      return 1;
    }
    compressed = out;
    comp_size = rc;
    VPRINTF("[%s%s%s] DoboZ compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_qlz) {
    // QuickLZ: direct API, allocate destination conservatively (size +
    // QLZ_BUFFER_PADDING)
    start_progress("QuickLZ compression", 8);
    update_progress(0, 8);
    size_t max_out = size_to_compress + QLZ_BUFFER_PADDING;
    unsigned char *out = (unsigned char *)malloc(max_out);
    if (!out) {
      printf("[%s%s%s] OOM QuickLZ buffer\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    qlz_state_compress qstate;
    memset(&qstate, 0, sizeof(qstate));
    size_t written =
        qlz_compress(comp_input, (char *)out, size_to_compress, &qstate);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (written == 0 || written > max_out || written > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "QuickLZ");
      free(out);
      return 1;
    }
    compressed = out;
    comp_size = (int)written;
    VPRINTF("[%s%s%s] QuickLZ compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_snappy) {
    // Snappy: output buffer allocated by wrapper; free() later
    start_progress("Snappy compression", 8);
    update_progress(0, 8);
    snappy_set_progress_cb(snappy_progress_cb);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    int rc = snappy_compress_c((const unsigned char *)comp_input,
                               size_to_compress, &out, &out_sz);
    snappy_set_progress_cb(NULL);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc != 0 || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "Snappy");
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] Snappy compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_lzav) {
    // LZAV: wrapper allocates buffer; free() later
    start_progress("LZAV compression", 8);
    update_progress(0, 8);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    int rc = lzav_compress_c((const unsigned char *)comp_input,
                             size_to_compress, &out, &out_sz);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc != 0 || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "LZAV");
      free(out);
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] LZAV compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_lzma) {
    // LZMA: direct LZMA SDK (LzmaLib) call; free() later
    start_progress("LZMA compression", 8);
    update_progress(0, 8);
    size_t bound = size_to_compress + (size_to_compress / 3) + 256;
    if (bound < 1024)
      bound = 1024;
    unsigned char *out = (unsigned char *)malloc(bound + LZMA_PROPS_SIZE);
    if (!out) {
      printf("[%s%s%s] OOM LZMA buffer\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    unsigned char props[LZMA_PROPS_SIZE];
    size_t propsSize = LZMA_PROPS_SIZE;
    size_t destLen = bound;
    size_t dict = (8u << 20);
    if (size_to_compress < dict)
      dict = size_to_compress ? size_to_compress : 1024;
    int res = LzmaCompress(
        out + LZMA_PROPS_SIZE, &destLen, (const unsigned char *)comp_input,
        (size_t)size_to_compress, props, &propsSize, 5, /* level */
        (unsigned)dict,                                 /* dictSize */
        3, 0, 2,                                        /* lc, lp, pb */
        64,                                             /* fb */
        1);                                             /* numThreads */
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (res != 0 || propsSize != LZMA_PROPS_SIZE || destLen == 0 ||
        destLen > bound) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "LZMA");
      free(out);
      return 1;
    }
    memcpy(out, props, LZMA_PROPS_SIZE);
    compressed = out;
    comp_size = (int)(destLen + LZMA_PROPS_SIZE);
    VPRINTF("[%s%s%s] LZMA compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_zstd) {
    // Zstd: direct streaming compression with progress; free() later
    start_progress("Zstd compression", 8);
    update_progress(0, 8);
    size_t bound = ZSTD_compressBound(size_to_compress);
    unsigned char *out = (unsigned char *)malloc(bound ? bound : 1);
    if (!out) {
      printf("[%s%s%s] OOM Zstd buffer\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    ZSTD_CCtx *cctx = ZSTD_createCCtx();
    if (!cctx) {
      free(out);
      printf("[%s%s%s] Zstd CCtx\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    int level = 22;
    size_t zrc;
    zrc = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);
    if (ZSTD_isError(zrc)) {
      ZSTD_freeCCtx(cctx);
      free(out);
      printf("[%s%s%s] Zstd level\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    zrc = ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 0);
    if (ZSTD_isError(zrc)) {
      ZSTD_freeCCtx(cctx);
      free(out);
      printf("[%s%s%s] Zstd checksum\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    (void)ZSTD_CCtx_setParameter(cctx, ZSTD_c_enableLongDistanceMatching, 0);
    (void)ZSTD_CCtx_setPledgedSrcSize(cctx, size_to_compress);
    ZSTD_outBuffer outb = {out, bound, 0};
    size_t processed = 0;
    const size_t chunk = (size_t)1 << 20;
    while (processed < size_to_compress) {
      size_t remaining = size_to_compress - processed;
      size_t take = remaining < chunk ? remaining : chunk;
      ZSTD_inBuffer inb = {(const unsigned char *)comp_input + processed, take,
                           0};
      while (inb.pos < inb.size) {
        size_t r = ZSTD_compressStream2(cctx, &outb, &inb, ZSTD_e_continue);
        if (ZSTD_isError(r)) {
          ZSTD_freeCCtx(cctx);
          free(out);
          printf("[%s%s%s] Zstd stream\n", PK_ERR, PK_SYM_ERR, PK_RES);
          return 1;
        }
      }
      processed += take;
      zstd_progress_cb(processed, size_to_compress);
    }
    for (;;) {
      ZSTD_inBuffer inb = {NULL, 0, 0};
      size_t r = ZSTD_compressStream2(cctx, &outb, &inb, ZSTD_e_end);
      if (ZSTD_isError(r)) {
        ZSTD_freeCCtx(cctx);
        free(out);
        printf("[%s%s%s] Zstd end\n", PK_ERR, PK_SYM_ERR, PK_RES);
        return 1;
      }
      if (r == 0)
        break;
    }
    ZSTD_freeCCtx(cctx);
    stop_progress(8);
    progress_clear_line();
    if (outb.pos == 0 || outb.pos > 0x7fffffff) {
      free(out);
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "Zstd");
      return 1;
    }
    compressed = out;
    comp_size = (int)outb.pos;
    VPRINTF("[%s%s%s] Zstd compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_shrinkler) {
    // Shrinkler: wrapper allocates buffer; free() later
    start_progress("Shrinkler compression", 8);
    update_progress(0, 8);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    shrinkler_comp_params opt = {0};
    g_shrinkler_last_pct = -1; // reset per-run
    shrinkler_set_progress_cb(shrinkler_progress_cb);
    int ok = shrinkler_compress_memory((const uint8_t *)comp_input,
                                       size_to_compress, &out, &out_sz, &opt);
    shrinkler_set_progress_cb(NULL);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (!ok || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "Shrinkler");
      if (out)
        free(out);
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] Shrinkler compression: %zu → %d bytes (%.1f%%)\n",
            PK_INFO, PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_sc) {
    start_progress("StoneCracker compression", 8);
    update_progress(0, 8);
    size_t max = (size_t)(size_to_compress * 1.2) + 1024;
    unsigned char *out = (unsigned char *)malloc(max);
    if (!out) {
      printf("[%s%s%s] OOM StoneCracker buffer\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    size_t rc =
        sc_compress((const uint8_t *)comp_input, size_to_compress, out, max);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc == 0 || rc > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "StoneCracker");
      free(out);
      return 1;
    }
    compressed = out;
    comp_size = (int)rc;
    VPRINTF("[%s%s%s] StoneCracker compression: %zu → %d bytes (%.1f%%)\n",
            PK_INFO, PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_lzsa) {
    start_progress("LZSA2 compression", 8);
    update_progress(0, 8);
    size_t max = lzsa2_compress_get_max_size(size_to_compress);
    unsigned char *out = (unsigned char *)malloc(max);
    if (!out) {
      printf("[%s%s%s] OOM LZSA2 buffer\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    // Use LZSA_FLAG_FAVOR_RATIO (1<<0) and min match 2
    int rc = lzsa2_compress((const unsigned char *)comp_input, out,
                            (int)size_to_compress, (int)max,
                            LZSA_FLAG_FAVOR_RATIO, 2);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc == 0 || rc > 0x7fffffff) { // lzsa2_compress returns compressed size,
                                      // check for error?
      // It doesn't seem to return negative error codes according to header, but
      // check source or assumption. Header says: return actual compressed size.
      if (rc == 0) { // Assuming 0 is failure if input > 0
        printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
               "LZSA2");
        free(out);
        return 1;
      }
    }
    compressed = out;
    comp_size = rc;
    VPRINTF("[%s%s%s] LZSA2 compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_density) {
    // Density: wrapper allocates buffer; free() later
    start_progress("Density compression", 8);
    update_progress(0, 8);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    int rc = density_compress_c((const unsigned char *)comp_input,
                                (size_t)size_to_compress, &out, &out_sz);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc != 0 || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP,
             "Density");
      if (out)
        free(out);
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] Density compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_rnc) {
    start_progress("RNC compression", 8);
    update_progress(0, 8);
    unsigned char *out = NULL;
    size_t out_sz = 0;
    int rc = rnc_compress_m1((const unsigned char *)comp_input,
                             size_to_compress, &out, &out_sz);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (rc != 0 || out == NULL || out_sz == 0 || out_sz > 0x7fffffff) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "RNC");
      if (out)
        free(out);
      return 1;
    }
    compressed = out;
    comp_size = (int)out_sz;
    VPRINTF("[%s%s%s] RNC compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else if (use_lzham) {
    start_progress("LZHAM compression", 8);
    update_progress(0, 8);

    // LZHAM with dict_size_log2=29 allocates very large internal tables.
    // Preflight an OOM check to avoid opaque ENOMEM failures.
    const uint32_t dict_log2 = 29;
    const size_t dict_sz = (size_t)1ULL << dict_log2;
    const size_t needed_ram = (size_t)(256ULL * 1024ULL * 1024ULL) + dict_sz * 8;
    const size_t available_ram = get_available_ram();
    const size_t rlimit_as = get_rlimit_as_bytes();
    if ((available_ram != (size_t)-1 && needed_ram > available_ram) ||
        (rlimit_as != (size_t)-1 && needed_ram > rlimit_as)) {
      const double need_gb = (double)needed_ram / (1024.0 * 1024.0 * 1024.0);
      const double avail_gb = (available_ram == (size_t)-1)
                                  ? -1.0
                                  : (double)available_ram /
                                        (1024.0 * 1024.0 * 1024.0);
      const double lim_gb = (rlimit_as == (size_t)-1)
                                ? -1.0
                                : (double)rlimit_as /
                                      (1024.0 * 1024.0 * 1024.0);
      stop_progress(8);
      progress_clear_line();
      if (avail_gb >= 0.0 && lim_gb >= 0.0) {
        printf("\n[%s%s%s] Error: LZHAM needs ~ %.2f Gb of RAM "
               "(dict=2^%u), %.2f Gb available, RLIMIT_AS=%.2f Gb, aborted.\n",
               PK_ERR, PK_SYM_ERR, PK_RES, need_gb, dict_log2, avail_gb, lim_gb);
      } else if (avail_gb >= 0.0) {
        printf("\n[%s%s%s] Error: LZHAM needs ~ %.2f Gb of RAM "
               "(dict=2^%u), %.2f Gb available, aborted.\n",
               PK_ERR, PK_SYM_ERR, PK_RES, need_gb, dict_log2, avail_gb);
      } else if (lim_gb >= 0.0) {
        printf("\n[%s%s%s] Error: LZHAM needs ~ %.2f Gb of RAM "
               "(dict=2^%u), RLIMIT_AS=%.2f Gb, aborted.\n",
               PK_ERR, PK_SYM_ERR, PK_RES, need_gb, dict_log2, lim_gb);
      } else {
        printf("\n[%s%s%s] Error: LZHAM needs ~ %.2f Gb of RAM "
               "(dict=2^%u), aborted.\n",
               PK_ERR, PK_SYM_ERR, PK_RES, need_gb, dict_log2);
      }
      return 1;
    }

    size_t max_out_len = size_to_compress + 4096;
    unsigned char *out = (unsigned char *)malloc(max_out_len);
    if (!out) {
      printf("[%s%s%s] OOM LZHAM\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }
    size_t out_len = max_out_len;
    uint32_t adler = 0;
    // Use dict_size_log2=29 (512MB)
    lzham_compress_status_t status = lzhamc_compress_memory(
        dict_log2, comp_input, size_to_compress, out, &out_len, &adler);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (status != LZHAM_COMP_STATUS_SUCCESS) {
      printf("[%s%s%s] %s %s (status=%d)\n", PK_ERR, PK_SYM_ERR, PK_RES,
             ERR_COMP, "LZHAM", status);
      free(out);
      return 1;
    }
    compressed = out;
    comp_size = (int)out_len;

    VPRINTF("[%s%s%s] LZHAM compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  } else {
    int max = LZ4_compressBound((int)size_to_compress);
    compressed = malloc(max);
    if (!compressed)
      fail("malloc compressed");
    start_progress("LZ4 (HC max) compression", 8);
    update_progress(0, 8);
    comp_size =
        LZ4_compress_HC((const char *)comp_input, (char *)compressed,
                        (int)size_to_compress, (int)max, LZ4HC_CLEVEL_MAX);
    update_progress(100, 8);
    stop_progress(8);
    progress_clear_line();
    if (comp_size <= 0) {
      printf("[%s%s%s] %s %s\n", PK_ERR, PK_SYM_ERR, PK_RES, ERR_COMP, "LZ4");
      return 1;
    }
    VPRINTF("[%s%s%s] LZ4 compression: %zu → %d bytes (%.1f%%)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, size_to_compress, comp_size,
            (float)comp_size / size_to_compress * 100);
  }

  *output = compressed;
  *output_size = comp_size;
  return 0;
}
