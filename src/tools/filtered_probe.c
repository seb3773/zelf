// filtered_probe.c - Post-filter metrics for PP predictor (raw vs BCJ vs KanziEXE)
// Platform: Linux x86_64
// Language: C99
// Notes:
// - Mirrors packer behavior: super-strip the file, compute code window from PF_X PT_LOAD segments
// - Apply BCJ on entire stripped stream; apply KanziEXE on [codeStart,codeEnd) within stripped
// - Compute: entropies, autocorr(lag1), per-4K block entropy stdev, diff-adjacent entropy
// - Compute LZ4 and LZ4HC compressed sizes for raw/BCJ/Kanzi buffers as LZ-complexity proxies
// - Output a CSV line per file. Use --header to print header. Use --no-path to omit the path column.

#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>
#include <elf.h>

#include "bcj_x86_enc.h"
#include "kanzi_exe_encode_c.h"
#include "lz4.h"
#include "lz4hc.h"

#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

static int opt_header = 0;
static int opt_no_path = 0;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [--header] [--no-path] <file> [file2 ...]\n", prog);
}

static void *map_file_ro(const char *path, size_t *psize) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return NULL; }
    void *p = NULL;
    if (st.st_size) {
        p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE
#ifdef MAP_POPULATE
            | MAP_POPULATE
#endif
            , fd, 0);
    }
    close(fd);
    if (p == MAP_FAILED) return NULL;
    *psize = (size_t)st.st_size;
    return p ? p : NULL;
}

static int is_elf64(const unsigned char *data, size_t size) {
    if (size < sizeof(Elf64_Ehdr)) return 0;
    if (data[0] != 0x7f || data[1] != 'E' || data[2] != 'L' || data[3] != 'F') return 0;
    if (data[4] != ELFCLASS64) return 0; // 64-bit only
    return 1;
}

// Super-strip: copy only bytes referenced by program headers and trim trailing zeros
static int superstrip(const unsigned char* file, size_t fsize,
                      unsigned char** out_buf, size_t* out_size) {
    if (!is_elf64(file, fsize)) return 0;
    const Elf64_Ehdr* eh = (const Elf64_Ehdr*)file;
    if ((size_t)eh->e_phoff + (size_t)eh->e_phnum * (size_t)eh->e_phentsize > fsize) return 0;
    size_t ph_table_end = eh->e_phoff + (size_t)eh->e_phnum * (size_t)eh->e_phentsize;
    size_t newsize = eh->e_ehsize;
    if (ph_table_end > newsize) newsize = ph_table_end;
    const Elf64_Phdr* ph = (const Elf64_Phdr*)(file + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_NULL) {
            size_t n = (size_t)ph[i].p_offset + (size_t)ph[i].p_filesz;
            if (n > newsize) newsize = n;
        }
    }
    if (newsize > fsize) newsize = fsize;
    unsigned char* slim = (unsigned char*)malloc(newsize);
    if (!slim) return 0;
    memcpy(slim, file, newsize);
    // Trim trailing zeros but not below headers
    Elf64_Ehdr* ne = (Elf64_Ehdr*)slim;
    size_t zsize = newsize;
    while (zsize > 0 && slim[zsize - 1] == 0) zsize--;
    size_t ph_min = (size_t)ne->e_phoff + (size_t)ne->e_phnum * (size_t)ne->e_phentsize;
    if (zsize < ph_min) zsize = ph_min;
    if (zsize < (size_t)ne->e_ehsize) zsize = (size_t)ne->e_ehsize;
    if (zsize == 0) zsize = newsize;
    newsize = zsize;
    // Patch section table if out of range
    if ((size_t)ne->e_shoff >= newsize) {
        ne->e_shoff = 0; ne->e_shnum = 0; ne->e_shstrndx = 0;
    }
    Elf64_Phdr* nph = (Elf64_Phdr*)(slim + ne->e_phoff);
    for (int i = 0; i < ne->e_phnum; ++i) {
        if ((size_t)nph[i].p_offset >= newsize) {
            nph[i].p_offset = (Elf64_Off)newsize; nph[i].p_filesz = 0;
        } else if ((size_t)nph[i].p_offset + (size_t)nph[i].p_filesz > newsize) {
            nph[i].p_filesz = (Elf64_Xword)(newsize - (size_t)nph[i].p_offset);
        }
    }
    *out_buf = slim; *out_size = newsize;
    return 1;
}

// Compute code window [start,end) from PF_X PT_LOAD segments in FILE offsets
static void compute_code_window(const unsigned char* file, size_t fsize,
                                int* p_start, int* p_end) {
    const Elf64_Ehdr* eh = (const Elf64_Ehdr*)file;
    const Elf64_Phdr* ph = (const Elf64_Phdr*)(file + eh->e_phoff);
    size_t off = (size_t)-1, end = 0, max_sz = 0;
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type == PT_LOAD && (ph[i].p_flags & PF_X) && ph[i].p_filesz > 0) {
            size_t so = (size_t)ph[i].p_offset;
            size_t se = so + (size_t)ph[i].p_filesz;
            size_t sz = (size_t)ph[i].p_filesz;
            if (sz > max_sz) { max_sz = sz; off = so; end = se; }
        }
    }
    if (off == (size_t)-1 || end <= off) { *p_start = 0; *p_end = 9; return; }
    if (end > fsize) end = fsize;
    *p_start = (int)off; *p_end = (int)end;
    if (*p_end < *p_start + 9) *p_end = (*p_start + 9 <= (int)fsize) ? *p_start + 9 : (int)fsize;
}

static double entropy_hist(const uint64_t hist[256], uint64_t tot) {
    if (tot == 0) return 0.0;
    double H = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (!hist[i]) continue;
        double p = (double)hist[i] / (double)tot;
        H -= p * (log(p) / log(2.0));
    }
    return H;
}

static double entropy_bytes(const unsigned char *p, size_t n) {
    uint64_t hist[256] = {0};
    for (size_t i = 0; i < n; ++i) hist[p[i]]++;
    return entropy_hist(hist, n);
}

static double entropy_diffadj(const unsigned char *p, size_t n) {
    if (n < 2) return 0.0;
    uint64_t hist[256] = {0};
    uint8_t prev = p[0];
    for (size_t i = 1; i < n; ++i) {
        uint8_t b = p[i];
        uint8_t d = (uint8_t)((int)b - (int)prev);
        hist[d]++;
        prev = b;
    }
    return entropy_hist(hist, n - 1);
}

static double autocorr1(const unsigned char *p, size_t n) {
    if (n < 2) return 0.0;
    double mean = 0.0; for (size_t i = 0; i < n; ++i) mean += (double)p[i]; mean /= (double)n;
    double var = 0.0; for (size_t i = 0; i < n; ++i) { double d = (double)p[i] - mean; var += d*d; }
    if (var == 0.0) return 1.0;
    double cov = 0.0;
    for (size_t i = 1; i < n; ++i) {
        double x = (double)p[i] - mean;
        double y = (double)p[i-1] - mean;
        cov += x * y;
    }
    cov /= (double)(n - 1);
    var /= (double)n;
    double r = cov / var;
    if (r > 1.0) r = 1.0;
    if (r < -1.0) r = -1.0;
    return r;
}

static double entropy_block_stddev(const unsigned char *p, size_t n, size_t block) {
    if (n == 0 || block == 0) return 0.0;
    size_t blocks = n / block;
    if (blocks == 0) return 0.0;
    double *vals = (double*)malloc(sizeof(double) * blocks);
    if (!vals) return 0.0;
    for (size_t b = 0; b < blocks; ++b) {
        const unsigned char *q = p + b * block;
        vals[b] = entropy_bytes(q, block);
    }
    double mean = 0.0; for (size_t b = 0; b < blocks; ++b) mean += vals[b]; mean /= (double)blocks;
    double var = 0.0; for (size_t b = 0; b < blocks; ++b) { double d = vals[b] - mean; var += d*d; }
    free(vals);
    if (blocks > 1) var /= (double)(blocks - 1); else var = 0.0;
    return sqrt(var);
}

static int compress_lz4_size(const unsigned char *src, int size, int hc) {
    int bound = LZ4_compressBound(size);
    char *out = (char*)malloc((size_t)bound);
    if (!out) return -1;
    int rc = hc ? LZ4_compress_HC((const char*)src, out, size, bound, 12)
                : LZ4_compress_default((const char*)src, out, size, bound);
    free(out);
    return (rc > 0) ? rc : -1;
}

static int apply_bcj_copy(const unsigned char *src, int size, unsigned char **out) {
    unsigned char *buf = (unsigned char*)malloc((size_t)size);
    if (!buf) return -1;
    memcpy(buf, src, (size_t)size);
    (void)bcj_x86_encode((uint8_t*)buf, (size_t)size, 0);
    *out = buf; return size;
}

static int apply_kanzi_stream(const unsigned char *slim, int actual, int cs, int ce,
                              unsigned char **out, int *outlen) {
    int cap = kanzi_exe_get_max_encoded_length(actual);
    if (cap < actual + 64) cap = actual + 64;
    unsigned char *kbuf = (unsigned char*)malloc((size_t)cap);
    if (!kbuf) return -1;
    int processed = 0;
    int ksz = kanzi_exe_filter_encode_force_x86_range(slim, actual, kbuf, cap, &processed, cs, ce);
    if (ksz <= 0 || processed != actual) {
        if ((size_t)cap < (size_t)actual + 9) {
            unsigned char *tmp = (unsigned char*)realloc(kbuf, (size_t)actual + 9);
            if (!tmp) { free(kbuf); return -1; }
            kbuf = tmp; cap = actual + 9;
        }
        kbuf[0] = 0x40; memset(kbuf + 1, 0, 8); memcpy(kbuf + 9, slim, (size_t)actual); ksz = actual + 9;
    }
    *out = kbuf; *outlen = ksz; return 0;
}

static int probe_one(const char *path) {
    size_t fsz = 0; unsigned char *file = (unsigned char*)map_file_ro(path, &fsz);
    if (!file) return -1;
    if (!is_elf64(file, fsz)) { munmap(file, fsz); return -1; }
    const Elf64_Ehdr *eh = (const Elf64_Ehdr*)file;
    if ((size_t)eh->e_phoff + (size_t)eh->e_phnum * (size_t)eh->e_phentsize > fsz) { munmap(file, fsz); return -1; }

    unsigned char *slim = NULL; size_t actual = 0;
    if (!superstrip(file, fsz, &slim, &actual)) { munmap(file, fsz); return -1; }
    int cs = 0, ce = 0; compute_code_window(file, fsz, &cs, &ce);
    if (cs > (int)actual) cs = (int)actual;
    if (ce > (int)actual) ce = (int)actual;
    if (ce < cs + 9) ce = (cs + 9 <= (int)actual) ? cs + 9 : (int)actual;

    // Prepare BCJ
    unsigned char *bcj = NULL; int bsz = apply_bcj_copy(slim, (int)actual, &bcj);
    if (bsz < 0) { free(slim); munmap(file, fsz); return -1; }
    // Prepare Kanzi
    unsigned char *kan = NULL; int ksz = -1;
    if (apply_kanzi_stream(slim, (int)actual, cs, ce, &kan, &ksz) != 0) { free(bcj); free(slim); munmap(file, fsz); return -1; }

    // Entropies
    double ent_raw   = entropy_bytes(slim, actual);
    double ent_bcj   = entropy_bytes(bcj,  (size_t)bsz);
    double ent_kanzi = entropy_bytes(kan,  (size_t)ksz);

    // Diff-adjacent entropies
    double dent_raw   = entropy_diffadj(slim, actual);
    double dent_bcj   = entropy_diffadj(bcj,  (size_t)bsz);
    double dent_kanzi = entropy_diffadj(kan,  (size_t)ksz);

    // Autocorrelation lag-1
    double ac1_raw   = autocorr1(slim, actual);
    double ac1_bcj   = autocorr1(bcj,  (size_t)bsz);
    double ac1_kanzi = autocorr1(kan,  (size_t)ksz);

    // Per-4K block entropy stddev (homogeneity proxy)
    double estd_raw   = entropy_block_stddev(slim, actual, 4096);
    double estd_bcj   = entropy_block_stddev(bcj,  (size_t)bsz, 4096);
    double estd_kanzi = entropy_block_stddev(kan,  (size_t)ksz, 4096);

    // LZ4 sizes (raw, BCJ, Kanzi)
    int lz4_raw    = compress_lz4_size(slim, (int)actual, 0);
    int lz4_bcj    = compress_lz4_size(bcj, bsz, 0);
    int lz4_kanzi  = compress_lz4_size(kan, ksz, 0);
    int lz4hc_raw   = compress_lz4_size(slim, (int)actual, 1);
    int lz4hc_bcj   = compress_lz4_size(bcj, bsz, 1);
    int lz4hc_kanzi = compress_lz4_size(kan, ksz, 1);

    // KEXP
    double kexp = ((double)ksz / (double)actual) - 1.0;

    // Derived ratios (entropy-based)
    double ratio_bcj = (ent_raw > 0.0) ? (ent_raw - ent_bcj) / ent_raw : 0.0;
    double ratio_kz  = (ent_raw > 0.0) ? (ent_raw - ent_kanzi) / ent_raw : 0.0;
    double delta_eff = ratio_kz - ratio_bcj;

    // Compression gain (LZ4HC) relative to stripped size
    double cg_k_vs_b_hc = 0.0;
    if (lz4hc_bcj >= 0 && lz4hc_kanzi >= 0 && actual > 0) {
        cg_k_vs_b_hc = ((double)lz4hc_bcj - (double)lz4hc_kanzi) / (double)actual;
    }
    if (lz4_bcj >= 0 && lz4_kanzi >= 0 && actual > 0) {
    }

    if (!opt_no_path) printf("\"%s\",", path);
    printf("%zu,%d,%d,%d,", actual, cs, ce, (ce>=cs? (ce-cs):0));
    printf("%.6f,%.6f,%.6f,", ent_raw, ent_bcj, ent_kanzi);
    printf("%.6f,%.6f,%.6f,", ac1_raw, ac1_bcj, ac1_kanzi);
    printf("%.6f,%.6f,%.6f,", estd_raw, estd_bcj, estd_kanzi);
    printf("%.6f,%.6f,%.6f,", dent_raw, dent_bcj, dent_kanzi);
    printf("%d,%d,%d,%d,%d,%d,", lz4_raw, lz4_bcj, lz4_kanzi, lz4hc_raw, lz4hc_bcj, lz4hc_kanzi);
    printf("%.6f,%.6f,%.6f,%.6f,%.6f\n", kexp, ratio_bcj, ratio_kz, delta_eff, cg_k_vs_b_hc);

    free(bcj); free(kan); free(slim); munmap(file, fsz);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    int first = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--header") == 0) { opt_header = 1; continue; }
        if (strcmp(argv[i], "--no-path") == 0) { opt_no_path = 1; continue; }
        first = i; break;
    }
    if (opt_header) {
        if (!opt_no_path) printf("path,");
        printf("stripped_size,code_start,code_end,code_span,"
               "ent_raw,ent_bcj,ent_kanzi,"
               "autoc1_raw,autoc1_bcj,autoc1_kanzi,"
               "entstd_raw,entstd_bcj,entstd_kanzi,"
               "diffent_raw,diffent_bcj,diffent_kanzi,"
               "lz4_raw,lz4_bcj,lz4_kanzi,lz4hc_raw,lz4hc_bcj,lz4hc_kanzi,"
               "kexp,ratio_bcj,ratio_kanzi,delta_eff,comp_gain_k_vs_b_lz4hc\n");
        if (first >= argc) return 0;
    }
    for (int i = first; i < argc; ++i) {
        const char *p = argv[i]; if (!p || !*p) continue;
        (void)probe_one(p);
    }
    return 0;
}
