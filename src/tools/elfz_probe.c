// elfz_probe.c - Extract features from ELF64 binaries for BCJ vs KanziEXE prediction
// Platform: Linux x86_64
// Language: C99
// Notes:
// - CPU-bound scanning of segments; avoids external deps.
// - Parses ELF header and Program Headers only (section headers may be stripped).
// - Measures entropy, branch opcode densities, padding, ASCII ratios, zero-runs, etc.
// - One CSV line per input file. Use --header to print header once.
//
// Security & robustness
// - Validate pointers and sizes; bounds check all reads.
// - No unbounded libc functions.
//
// Performance considerations (per user rules):
// - Use size_t/uint64_t counters.
// - Static inline helpers for hot paths.
// - Single pass scans per segment type.

#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>
#include <elf.h>

#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

// --- Configuration ---
#define MIN_PRINTABLE_ASCII 0x20
#define MAX_PRINTABLE_ASCII 0x7E

// CSV header
static const char *CSV_HEADER =
    "path,file_size,etype,has_interp,n_load,stripped,dynrel_cnt,"
    "text_sz,ro_sz,data_sz,text_ratio,ro_ratio,data_ratio,"
    "text_entropy,ro_entropy,data_entropy,"
    "zeros_ratio_total,zero_runs_16,zero_runs_32,ascii_ratio_rodata,"
    "e8_cnt,e9_cnt,ff_calljmp_cnt,eb_cnt,jcc32_cnt,branch_density_per_kb,"
    "riprel_estimate,nop_ratio_text,imm64_mov_cnt,align_pad_ratio,ro_ptr_like_cnt,"
    "rel32_intext_ratio,rel32_intext_cnt,rel32_disp_entropy8,"
    "x_load_cnt,ro_load_cnt,rw_load_cnt,bss_sz,avg_p_align_log2,max_p_align_log2,"
    "shnum,sh_exec_cnt,sh_align_avg_log2,"
    "dyn_needed_cnt,dynsym_cnt,pltrel_cnt,dynrel_text_cnt,dynrel_text_density_per_kb,"
    "ret_cnt,rel_branch_ratio,avg_rel32_abs,max_rel32_abs,rel32_target_unique_cnt,"
    "autocorr1_text,max_run00_text,max_run90_text,max_runany_text";

static int print_header = 0;

static inline uint64_t umin64(uint64_t a, uint64_t b) { return a < b ? a : b; }

// Integer log2 for alignment (return 0 for x==0)
static inline uint32_t ilog2_u64(uint64_t x)
{
    if (x == 0) return 0;
    return (uint32_t)(63u - (uint32_t)__builtin_clzll(x));
}

// Compute max run length of a specific byte value in a buffer
static inline uint64_t max_run_of(const uint8_t *p, size_t n, uint8_t val)
{
    uint64_t best = 0, cur = 0;
    for (size_t i = 0; i < n; ++i) {
        if (p[i] == val) { cur++; if (cur > best) best = cur; }
        else cur = 0;
    }
    return best;
}

// Compute max run length across any repeated byte value
static inline uint64_t max_run_any(const uint8_t *p, size_t n)
{
    if (n == 0) return 0;
    uint64_t best = 1, cur = 1; uint8_t prev = p[0];
    for (size_t i = 1; i < n; ++i) {
        uint8_t b = p[i];
        if (b == prev) { cur++; if (cur > best) best = cur; }
        else { cur = 1; prev = b; }
    }
    return best;
}

// Map a virtual address to file offset via PT_LOAD
static inline int vaddr_to_off(uint64_t vaddr, uint64_t *off_out,
                               const uint8_t *base, size_t fsz,
                               const Elf64_Phdr *ph, size_t phnum)
{
    (void)base; (void)fsz;
    for (size_t i = 0; i < phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint64_t va = ph[i].p_vaddr;
        uint64_t mem = ph[i].p_memsz;
        uint64_t file_off = ph[i].p_offset;
        uint64_t file_sz  = ph[i].p_filesz;
        if (vaddr >= va && vaddr < va + mem) {
            uint64_t off = file_off + (vaddr - va);
            if (off < file_off || off >= file_off + file_sz) return 0; // not in file
            *off_out = off; return 1;
        }
    }
    return 0;
}
static inline uint64_t umax64(uint64_t a, uint64_t b) { return a > b ? a : b; }

// Entropy calculation on a byte histogram
static double entropy_from_hist(const uint64_t hist[256], uint64_t total)
{
    if (total == 0) return 0.0;
    double H = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (hist[i] == 0) continue;
        double p = (double)hist[i] / (double)total;
        H -= p * (log(p) / log(2.0)); // log2(p)
    }
    return H;
}

// Count zero runs >= threshold across buffer
static inline uint64_t count_zero_runs_ge(const uint8_t *p, size_t n, size_t thr)
{
    uint64_t runs = 0;
    size_t i = 0;
    while (i < n) {
        if (p[i] == 0) {
            size_t j = i + 1;
            while (j < n && p[j] == 0) j++;
            if ((j - i) >= thr) runs++;
            i = j;
        } else {
            i++;
        }
    }
    return runs;
}

// Ratio of bytes that are 0x90 in .text
static inline double ratio_nop_0x90(const uint8_t *p, size_t n)
{
    if (n == 0) return 0.0;
    uint64_t c = 0;
    for (size_t i = 0; i < n; ++i) c += (p[i] == 0x90);
    return (double)c / (double)n;
}

// Ratio of alignment padding runs (0x00 or 0x90) of length >= 4 in .text
static inline double align_pad_ratio_text(const uint8_t *p, size_t n)
{
    if (n == 0) return 0.0;
    uint64_t pad_bytes = 0;
    size_t i = 0;
    while (i < n) {
        uint8_t b = p[i];
        if (b == 0x00 || b == 0x90) {
            size_t j = i + 1;
            while (j < n && p[j] == b) j++;
            size_t run = j - i;
            if (run >= 4) pad_bytes += run;
            i = j;
        } else {
            i++;
        }
    }
    return (double)pad_bytes / (double)n;
}

// Estimate RIP-relative addressing patterns: 0x48 0x8B/0x8D with ModRM mod=00 r/m=101
static inline uint64_t riprel_estimate(const uint8_t *p, size_t n)
{
    uint64_t cnt = 0;
    for (size_t i = 0; i + 2 < n; ++i) {
        if (p[i] == 0x48 && (p[i+1] == 0x8B || p[i+1] == 0x8D)) {
            uint8_t modrm = p[i+2];
            uint8_t mod = (modrm >> 6) & 3u;
            uint8_t rm  = modrm & 7u;
            if (mod == 0 && rm == 5) cnt++;
        }
    }
    return cnt;
}

// Count FF /w ModRM.reg in {2,4} (CALL/JMP r/m64 approx.)
static inline uint64_t ff_calljmp_count(const uint8_t *p, size_t n)
{
    uint64_t cnt = 0;
    for (size_t i = 0; i + 1 < n; ++i) {
        if (p[i] == 0xFF) {
            uint8_t modrm = p[i+1];
            uint8_t reg = (modrm >> 3) & 7u;
            if (reg == 2u || reg == 4u) cnt++;
        }
    }
    return cnt;
}

// Count Jcc rel32 patterns: 0x0F 0x8?
static inline uint64_t jcc32_count(const uint8_t *p, size_t n)
{
    uint64_t cnt = 0;
    for (size_t i = 0; i + 1 < n; ++i) {
        if (p[i] == 0x0F) {
            uint8_t b = p[i+1];
            if ((b & 0xF0u) == 0x80u) cnt++;
        }
    }
    return cnt;
}

// Count MOV r64, imm64: 0x48 0xB8..0xBF
static inline uint64_t imm64_mov_count(const uint8_t *p, size_t n)
{
    uint64_t cnt = 0;
    for (size_t i = 0; i + 1 < n; ++i) {
        if (p[i] == 0x48) {
            uint8_t op = p[i+1];
            if (op >= 0xB8 && op <= 0xBF) cnt++;
        }
    }
    return cnt;
}

// ASCII ratio (R/O segments): printable + {\t,\n,\r}
static inline double ascii_ratio(const uint8_t *p, size_t n)
{
    if (n == 0) return 0.0;
    uint64_t c = 0;
    for (size_t i = 0; i < n; ++i) {
        uint8_t b = p[i];
        if ((b >= MIN_PRINTABLE_ASCII && b <= MAX_PRINTABLE_ASCII) || b == '\t' || b == '\n' || b == '\r') c++;
    }
    return (double)c / (double)n;
}

// Count 8-byte aligned words with low 12 bits zero (address-like) in RO data
static inline uint64_t ro_ptr_like(const uint8_t *p, size_t n)
{
    uint64_t cnt = 0;
    // align to 8
    size_t off = ((uintptr_t)p) & 7u;
    if (off) {
        size_t adv = 8 - off;
        if (adv > n) return 0;
        p   += adv;
        n   -= adv;
    }
    for (size_t j = 0; j + 8 <= n; j += 8) {
        uint64_t v;
        memcpy(&v, p + j, sizeof(v));
        // little-endian assumed
        if ((v & 0xFFFull) == 0) cnt++;
    }
    return cnt;
}

// Parse DT_* from PT_DYNAMIC to estimate relocation count
static uint64_t parse_dynrel_count(const uint8_t *base, size_t fsz, const Elf64_Ehdr *eh, const Elf64_Phdr *ph, size_t phnum)
{
    (void)eh;
    for (size_t i = 0; i < phnum; ++i) {
        if (ph[i].p_type != PT_DYNAMIC) continue;
        uint64_t off = ph[i].p_offset;
        uint64_t sz  = ph[i].p_filesz;
        if (off >= fsz || sz > fsz - off) continue;
        const Elf64_Dyn *dyn = (const Elf64_Dyn *)(base + off);
        size_t cnt = sz / sizeof(Elf64_Dyn);
        uint64_t rela_sz = 0, rela_ent = 24; // default
        uint64_t rel_sz  = 0,  rel_ent  = 16; // default
        for (size_t j = 0; j < cnt; ++j) {
            int64_t tag = dyn[j].d_tag;
            uint64_t val = (uint64_t)dyn[j].d_un.d_val;
            if (tag == DT_NULL) break;
            if (tag == DT_RELASZ) rela_sz = val;
            else if (tag == DT_RELAENT) rela_ent = val ? val : rela_ent;
            else if (tag == DT_RELSZ) rel_sz = val;
            else if (tag == DT_RELENT) rel_ent = val ? val : rel_ent;
        }
        uint64_t cnt_rela = (rela_ent ? (rela_sz / rela_ent) : 0);
        uint64_t cnt_rel  = (rel_ent  ? (rel_sz  / rel_ent)  : 0);
        return cnt_rela + cnt_rel;
    }
    return 0;
}

// Accumulate histograms and counts for a memory region
static void scan_buffer_features(
    const uint8_t *buf, size_t len,
    uint64_t hist[256], uint64_t *zeros, uint64_t *nonzeros)
{
    const size_t step = 4096; // touch pages predictably
    uint64_t z = 0, nz = 0;
    size_t i = 0;
    while (i < len) {
        size_t chunk = len - i;
        if (chunk > step) chunk = step;
        const uint8_t *p = buf + i;
        for (size_t k = 0; k < chunk; ++k) {
            uint8_t b = p[k];
            hist[b]++;
            if (b == 0) z++; else nz++;
        }
        i += chunk;
    }
    *zeros += z;
    *nonzeros += nz;
}

static int probe_one(const char *path)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return -1; }
    size_t fsz = (size_t)st.st_size;

    void *map = NULL;
    if (fsz) {
        map = mmap(NULL, fsz, PROT_READ, MAP_PRIVATE
#ifdef MAP_POPULATE
            | MAP_POPULATE
#endif
            , fd, 0);
    }
    close(fd);
    if (map == MAP_FAILED || map == NULL) return -1;

    const uint8_t *base = (const uint8_t *)map;
    if (fsz < sizeof(Elf64_Ehdr)) { munmap(map, fsz); return -1; }

    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)base;
    if (!(eh->e_ident[EI_MAG0]==ELFMAG0 && eh->e_ident[EI_MAG1]==ELFMAG1 && eh->e_ident[EI_MAG2]==ELFMAG2 && eh->e_ident[EI_MAG3]==ELFMAG3)) {
        munmap(map, fsz); return -1;
    }
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) { munmap(map, fsz); return -1; }

    int etype = (int)eh->e_type;
    int stripped = (eh->e_shoff == 0 || eh->e_shnum == 0) ? 1 : 0;

    if (eh->e_phoff == 0 || eh->e_phnum == 0) { munmap(map, fsz); return -1; }
    if (fsz < eh->e_phoff + eh->e_phnum * sizeof(Elf64_Phdr)) { munmap(map, fsz); return -1; }

    const Elf64_Phdr *ph = (const Elf64_Phdr *)(base + eh->e_phoff);
    int has_interp = 0;
    int n_load = 0;
    uint64_t x_load_cnt = 0, ro_load_cnt = 0, rw_load_cnt = 0;
    uint64_t bss_sz = 0;
    uint64_t p_align_sum_log2 = 0, p_align_max_log2 = 0; uint64_t p_align_cnt = 0;

    // Aggregated buffers (scan directly from mapped regions via offsets)
    uint64_t text_sz = 0, ro_sz = 0, data_sz = 0;

    // Histograms for entropy per class
    uint64_t hist_text[256] = {0}, hist_ro[256] = {0}, hist_data[256] = {0};
    uint64_t z_text = 0, nz_text = 0, z_ro = 0, nz_ro = 0, z_data = 0, nz_data = 0;

    // For text code-specific counters we need concatenated view: we simply count across each text segment
    uint64_t e8_cnt = 0, e9_cnt = 0, ff_cj_cnt = 0, eb_cnt = 0, jcc32_cnt = 0, riprel_cnt = 0, imm64_cnt = 0;
    uint64_t text_nop_bytes = 0; // computed via ratio function separately per segment
    uint64_t text_pad_bytes = 0; // for align_pad_ratio

    // Executable segment vaddr ranges for rel32 target checks
    struct { uint64_t lo, hi; } xseg[16];
    int xseg_n = 0;

    // For displacement entropy (top byte of disp32)
    uint64_t disp8_hist[256] = {0};
    uint64_t rel32_intext = 0;
    uint64_t rel32_abs_sum = 0; uint64_t rel32_abs_max = 0;
    // Cap unique targets to avoid O(N^2) and memory blowup
    uint64_t uniq_targets[4096]; int uniq_n = 0;

    // For ascii in rodata: accumulate later on total ro

    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        uint32_t pt = ph[i].p_type;
        if (pt == PT_INTERP) has_interp = 1;
        if (pt != PT_LOAD) continue;
        n_load++;
        uint64_t off = ph[i].p_offset;
        uint64_t sz  = ph[i].p_filesz;
        if (off >= fsz || sz == 0 || sz > fsz - off) continue;
        const uint8_t *p = base + off;
        uint32_t flags = ph[i].p_flags;
        int is_x = (flags & PF_X) != 0;
        int is_w = (flags & PF_W) != 0;
        int is_r = (flags & PF_R) != 0;

        // Counters per PT_LOAD kind
        if (is_x) x_load_cnt++;
        else if (is_r && !is_w) ro_load_cnt++;
        else if (is_w) rw_load_cnt++;
        // BSS size from memsz>filesz
        if (ph[i].p_memsz > ph[i].p_filesz) bss_sz += (uint64_t)(ph[i].p_memsz - ph[i].p_filesz);
        // p_align stats
        if (ph[i].p_align) {
            uint32_t lg = ilog2_u64(ph[i].p_align);
            p_align_sum_log2 += lg; p_align_cnt++; if (lg > p_align_max_log2) p_align_max_log2 = lg;
        }

        if (is_x && xseg_n < 16) {
            uint64_t lo = ph[i].p_vaddr;
            uint64_t hi = lo + sz;
            xseg[xseg_n].lo = lo;
            xseg[xseg_n].hi = hi;
            xseg_n++;
        }

        if (is_x) {
            // TEXT
            text_sz += sz;
            scan_buffer_features(p, (size_t)sz, hist_text, &z_text, &nz_text);
            // opcode scans
            // counts
            // jmp rel8 (EB)
            for (size_t j = 0; j < sz; ++j) eb_cnt += (p[j] == 0xEB);
            // Compute with direct scan for e8/e9 to reduce call overhead
            for (size_t j = 0; j + 4 < sz; ++j) {
                uint8_t b = p[j];
                if (b == 0xE8 || b == 0xE9) {
                    // count
                    if (b == 0xE8) e8_cnt++; else e9_cnt++;
                    // displacement as little-endian int32
                    int32_t d;
                    memcpy(&d, p + j + 1, sizeof(d));
                    // top-byte entropy stat
                    uint8_t top = (uint8_t)((uint32_t)d >> 24);
                    disp8_hist[top]++;
                    // estimate target and check if inside any exec seg
                    uint64_t insn_va = ph[i].p_vaddr + j;
                    int64_t target = (int64_t)insn_va + 5 + (int64_t)d;
                    uint64_t ad = (d < 0) ? (uint64_t)(-(int64_t)d) : (uint64_t)d;
                    rel32_abs_sum += ad; if (ad > rel32_abs_max) rel32_abs_max = ad;
                    for (int xs = 0; xs < xseg_n; ++xs) {
                        if ((uint64_t)target >= xseg[xs].lo && (uint64_t)target < xseg[xs].hi) {
                            rel32_intext++;
                            break;
                        }
                    }
                    // Unique targets (capped)
                    if (uniq_n < (int)(sizeof(uniq_targets)/sizeof(uniq_targets[0]))) {
                        uint64_t t = (uint64_t)target;
                        int seen = 0;
                        for (int ui = 0; ui < uniq_n; ++ui) { if (uniq_targets[ui] == t) { seen = 1; break; } }
                        if (!seen) { uniq_targets[uniq_n++] = t; }
                    }
                }
            }
            jcc32_cnt  += jcc32_count(p, (size_t)sz);
            ff_cj_cnt  += ff_calljmp_count(p, (size_t)sz);
            riprel_cnt += riprel_estimate(p, (size_t)sz);
            imm64_cnt  += imm64_mov_count(p, (size_t)sz);
            // nop ratio and align pad ratio
            // Accumulate nop count and pad bytes explicitly to compute ratio on total text later
            for (size_t j = 0; j < sz; ++j) text_nop_bytes += (p[j] == 0x90);
            // count RET (C3) and near ret imm16 (C2)
            for (size_t j = 0; j < sz; ++j) ;
            // pad runs >=4 of 0x00 or 0x90
            size_t j = 0;
            while (j < sz) {
                uint8_t b = p[j];
                if (b == 0x00 || b == 0x90) {
                    size_t k = j + 1;
                    while (k < sz && p[k] == b) k++;
                    size_t run = k - j;
                    if (run >= 4) text_pad_bytes += run;
                    j = k;
                } else {
                    j++;
                }
            }
        } else if (is_r && !is_w) {
            // RO DATA
            ro_sz += sz;
            scan_buffer_features(p, (size_t)sz, hist_ro, &z_ro, &nz_ro);
        } else if (is_w) {
            data_sz += sz;
            scan_buffer_features(p, (size_t)sz, hist_data, &z_data, &nz_data);
        } else {
            // ignore
        }
    }

    double text_ent = entropy_from_hist(hist_text, z_text + nz_text);
    double ro_ent   = entropy_from_hist(hist_ro,   z_ro   + nz_ro);
    double data_ent = entropy_from_hist(hist_data, z_data + nz_data);

    // Zeros ratio across compressible regions (text + ro)
    uint64_t comp_bytes = text_sz + ro_sz;
    double zeros_ratio_total = 0.0;
    uint64_t zero_runs_16 = 0, zero_runs_32 = 0;
    double ascii_ratio_ro = 0.0;

    if (comp_bytes > 0) {
        zeros_ratio_total = (double)(z_text + z_ro) / (double)comp_bytes;
        // Count zero runs by rescanning text+ro segments
        for (uint16_t i = 0; i < eh->e_phnum; ++i) {
            if (ph[i].p_type != PT_LOAD) continue;
            uint32_t flags = ph[i].p_flags;
            uint64_t off = ph[i].p_offset, sz = ph[i].p_filesz;
            if (off >= fsz || sz == 0 || sz > fsz - off) continue;
            const uint8_t *p = base + off;
            int is_x = (flags & PF_X) != 0;
            int is_w = (flags & PF_W) != 0;
            int is_r = (flags & PF_R) != 0;
            if (is_x || (is_r && !is_w)) {
                zero_runs_16 += count_zero_runs_ge(p, (size_t)sz, 16);
                zero_runs_32 += count_zero_runs_ge(p, (size_t)sz, 32);
            }
        }
        // ASCII ratio only on RO segments
        uint64_t ro_ascii_bytes = 0;
        if (ro_sz > 0) {
            for (uint16_t i = 0; i < eh->e_phnum; ++i) {
                if (ph[i].p_type != PT_LOAD) continue;
                uint32_t flags = ph[i].p_flags;
                uint64_t off = ph[i].p_offset, sz = ph[i].p_filesz;
                if (off >= fsz || sz == 0 || sz > fsz - off) continue;
                const uint8_t *p = base + off;
                int is_w = (flags & PF_W) != 0;
                int is_r = (flags & PF_R) != 0;
                int is_x = (flags & PF_X) != 0;
                if (is_r && !is_w && !is_x) {
                    // accumulate ascii count (we output ratio overall later)
                    for (size_t j = 0; j < sz; ++j) {
                        uint8_t b = p[j];
                        if ((b >= MIN_PRINTABLE_ASCII && b <= MAX_PRINTABLE_ASCII) || b == '\t' || b == '\n' || b == '\r') ro_ascii_bytes++;
                    }
                }
            }
            ascii_ratio_ro = (double)ro_ascii_bytes / (double)ro_sz;
        }
    }

    // Branch density per KB (protect div by zero)
    // Re-count RET in text (single pass over LOAD exec segments to keep branch loop light)
    uint64_t ret_cnt = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint32_t flags = ph[i].p_flags;
        if (!(flags & PF_X)) continue;
        uint64_t off = ph[i].p_offset, sz = ph[i].p_filesz;
        if (off >= fsz || sz == 0 || sz > fsz - off) continue;
        const uint8_t *p = base + off;
        for (uint64_t j = 0; j < sz; ++j) {
            uint8_t b = p[j];
            if (b == 0xC3) ret_cnt++;
            else if (b == 0xC2 && j + 2 < sz) ret_cnt++; // treat as RET
        }
    }

    uint64_t branch_raw = e8_cnt + e9_cnt + ff_cj_cnt;
    double branch_density = (text_sz > 0) ? ((double)branch_raw / ((double)text_sz / 1024.0)) : 0.0;
    double nop_ratio = (text_sz > 0) ? ((double)text_nop_bytes / (double)text_sz) : 0.0;
    double pad_ratio = (text_sz > 0) ? ((double)text_pad_bytes / (double)text_sz) : 0.0;
    // Relative branch ratio (relative branches vs all)
    uint64_t rel_b = e8_cnt + e9_cnt + jcc32_cnt + eb_cnt;
    uint64_t abs_b = ff_cj_cnt;
    double rel_branch_ratio = (rel_b + abs_b) ? (double)rel_b / (double)(rel_b + abs_b) : 0.0;

    // RO pointer-like count
    uint64_t ro_ptr_like_cnt = 0;
    if (ro_sz > 0) {
        for (uint16_t i = 0; i < eh->e_phnum; ++i) {
            if (ph[i].p_type != PT_LOAD) continue;
            uint32_t flags = ph[i].p_flags;
            uint64_t off = ph[i].p_offset, sz = ph[i].p_filesz;
            if (off >= fsz || sz == 0 || sz > fsz - off) continue;
            const uint8_t *p = base + off;
            int is_w = (flags & PF_W) != 0;
            int is_r = (flags & PF_R) != 0;
            int is_x = (flags & PF_X) != 0;
            if (is_r && !is_w && !is_x) ro_ptr_like_cnt += ro_ptr_like(p, (size_t)sz);
        }
    }

    uint64_t dynrel_cnt = parse_dynrel_count(base, fsz, eh, ph, eh->e_phnum);

    // Parse PT_DYNAMIC for additional stats
    uint64_t dyn_needed_cnt = 0, dynsym_cnt = 0, pltrel_cnt = 0, dynrel_text_cnt = 0;
    for (size_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_DYNAMIC) continue;
        uint64_t off = ph[i].p_offset, sz = ph[i].p_filesz;
        if (off >= fsz || sz == 0 || sz > fsz - off) continue;
        const Elf64_Dyn *dyn = (const Elf64_Dyn *)(base + off);
        size_t cnt = sz / sizeof(Elf64_Dyn);
        uint64_t dt_hash = 0, dt_gnu_hash = 0, dt_pltrel = 0, dt_pltrelsz = 0;
        uint64_t dt_rela = 0, dt_relasz = 0, dt_relaent = 24;
        uint64_t dt_rel  = 0, dt_relsz  = 0, dt_relent  = 16;
        uint64_t dt_jmprel = 0;
        for (size_t j = 0; j < cnt; ++j) {
            int64_t tag = dyn[j].d_tag;
            uint64_t val = (uint64_t)dyn[j].d_un.d_val;
            if (tag == DT_NULL) break;
            if (tag == DT_NEEDED) dyn_needed_cnt++;
            else if (tag == DT_HASH) dt_hash = val;
            else if (tag == DT_GNU_HASH) dt_gnu_hash = val;
            else if (tag == DT_PLTREL) dt_pltrel = val; // DT_REL or DT_RELA
            else if (tag == DT_PLTRELSZ) dt_pltrelsz = val;
            else if (tag == DT_JMPREL) dt_jmprel = val;
            else if (tag == DT_RELA) dt_rela = val;
            else if (tag == DT_RELASZ) dt_relasz = val;
            else if (tag == DT_RELAENT) dt_relaent = val ? val : dt_relaent;
            else if (tag == DT_REL) dt_rel = val;
            else if (tag == DT_RELSZ) dt_relsz = val;
            else if (tag == DT_RELENT) dt_relent = val ? val : dt_relent;
        }
        (void)dt_jmprel; // silence: set but not used
        // dynsym count via DT_HASH (nbucket,nchain)
        if (dt_hash) {
            uint64_t h_off = 0; if (vaddr_to_off(dt_hash, &h_off, base, fsz, ph, eh->e_phnum)) {
                if (h_off + 8 <= fsz) {
                    const uint32_t *hp = (const uint32_t *)(base + h_off);
                    uint32_t nb = hp[0]; (void)nb; uint32_t nc = hp[1];
                    dynsym_cnt = nc;
                }
            }
        } else if (dt_gnu_hash) {
            // Fallback: cannot easily derive symbol count without scanning; leave 0
        }
        // PLT reloc count
        if (dt_pltrelsz) {
            uint64_t ent = (dt_pltrel == DT_RELA) ? dt_relaent : dt_relent;
            if (ent) pltrel_cnt = dt_pltrelsz / ent;
        }
        // Count relocations targeting exec segments (RELA)
        if (dt_rela && dt_relasz && dt_relaent) {
            uint64_t r_off = 0; if (vaddr_to_off(dt_rela, &r_off, base, fsz, ph, eh->e_phnum)) {
                size_t n = (size_t)(dt_relasz / dt_relaent);
                for (size_t k = 0; k < n; ++k) {
                    if (r_off + (k+1)*dt_relaent > fsz) break;
                    const Elf64_Rela *re = (const Elf64_Rela *)(base + r_off + k*dt_relaent);
                    uint64_t ro = re->r_offset;
                    for (int xs = 0; xs < xseg_n; ++xs) {
                        if (ro >= xseg[xs].lo && ro < xseg[xs].hi) { dynrel_text_cnt++; break; }
                    }
                }
            }
        }
        // ... and (REL)
        if (dt_rel && dt_relsz && dt_relent) {
            uint64_t r_off = 0; if (vaddr_to_off(dt_rel, &r_off, base, fsz, ph, eh->e_phnum)) {
                size_t n = (size_t)(dt_relsz / dt_relent);
                for (size_t k = 0; k < n; ++k) {
                    if (r_off + (k+1)*dt_relent > fsz) break;
                    const Elf64_Rel *re = (const Elf64_Rel *)(base + r_off + k*dt_relent);
                    uint64_t ro = re->r_offset;
                    for (int xs = 0; xs < xseg_n; ++xs) {
                        if (ro >= xseg[xs].lo && ro < xseg[xs].hi) { dynrel_text_cnt++; break; }
                    }
                }
            }
        }
        break; // only one PT_DYNAMIC expected
    }

    // rel32 stats
    uint64_t e8e9_total = e8_cnt + e9_cnt;
    double rel32_intext_ratio = (e8e9_total ? (double)rel32_intext / (double)e8e9_total : 0.0);
    double disp8_entropy = entropy_from_hist(disp8_hist, e8e9_total);
    double avg_rel32_abs = (e8e9_total ? (double)rel32_abs_sum / (double)e8e9_total : 0.0);

    // Autocorr lag-1 on concatenated text stream
    long double sum = 0.0L, sumsq = 0.0L, sumprod = 0.0L;
    uint64_t n_text = 0; int have_prev = 0; uint8_t prev = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (!(ph[i].p_flags & PF_X)) continue;
        uint64_t off = ph[i].p_offset, sz = ph[i].p_filesz;
        if (off >= fsz || sz == 0 || sz > fsz - off) continue;
        const uint8_t *p = base + off;
        for (uint64_t j = 0; j < sz; ++j) {
            uint8_t b = p[j];
            sum += (long double)b;
            sumsq += (long double)b * (long double)b;
            if (have_prev) sumprod += (long double)b * (long double)prev;
            prev = b; have_prev = 1; n_text++;
        }
    }
    double autocorr1_text = 0.0;
    if (n_text >= 2) {
        double mu = (double)(sum / (long double)n_text);
        double var = (double)(sumsq / (long double)n_text) - mu * mu;
        if (var <= 0.0) autocorr1_text = 1.0;
        else {
            // sum over i=1..n-1 of x_i * x_{i-1}
            double cov_num = (double)sumprod - (double)(n_text - 1) * mu * mu;
            autocorr1_text = cov_num / ((double)(n_text - 1) * var);
            if (autocorr1_text > 1.0) autocorr1_text = 1.0;
            if (autocorr1_text < -1.0) autocorr1_text = -1.0;
        }
    }

    // Max runs in text
    uint64_t max_run00 = 0, max_run90 = 0, max_run_any_len = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (!(ph[i].p_flags & PF_X)) continue;
        uint64_t off = ph[i].p_offset, sz = ph[i].p_filesz;
        if (off >= fsz || sz == 0 || sz > fsz - off) continue;
        const uint8_t *p = base + off;
        uint64_t r00 = max_run_of(p, (size_t)sz, 0x00);
        uint64_t r90 = max_run_of(p, (size_t)sz, 0x90);
        uint64_t ran = max_run_any(p, (size_t)sz);
        if (r00 > max_run00) max_run00 = r00;
        if (r90 > max_run90) max_run90 = r90;
        if (ran > max_run_any_len) max_run_any_len = ran;
    }

    // CSV output
    // path,file_size,etype,has_interp,n_load,stripped,dynrel_cnt,
    // text_sz,ro_sz,data_sz,text_ratio,ro_ratio,data_ratio,
    // text_entropy,ro_entropy,data_entropy,
    // zeros_ratio_total,zero_runs_16,zero_runs_32,ascii_ratio_rodata,
    // e8_cnt,e9_cnt,ff_calljmp_cnt,eb_cnt,jcc32_cnt,branch_density_per_kb,
    // riprel_estimate,nop_ratio_text,imm64_mov_cnt,align_pad_ratio,ro_ptr_like_cnt

    double fs = (double)fsz;
    double text_ratio = fs>0.0 ? (double)text_sz / fs : 0.0;
    double ro_ratio   = fs>0.0 ? (double)ro_sz   / fs : 0.0;
    double data_ratio = fs>0.0 ? (double)data_sz / fs : 0.0;

    // Quote path for CSV safety
    printf("\"%s\",%zu,%d,%d,%d,%d,%llu,"
           "%llu,%llu,%llu,%.6f,%.6f,%.6f,"
           "%.6f,%.6f,%.6f,"
           "%.6f,%llu,%llu,%.6f,"
           "%llu,%llu,%llu,%llu,%llu,%.6f,"
           "%llu,%.6f,%llu,%.6f,%llu,"
           "%.6f,%llu,%.6f,"
           "%llu,%llu,%llu,%llu,%.6f,%.6f,"
           "%u,%u,%.6f,"
           "%llu,%llu,%llu,%llu,%.6f,"
           "%llu,%.6f,%.6f,%llu,%llu,"
           "%.6f,%llu,%llu,%llu\n",
           path, (size_t)fsz, etype, has_interp, n_load, stripped, (unsigned long long)dynrel_cnt,
           (unsigned long long)text_sz, (unsigned long long)ro_sz, (unsigned long long)data_sz,
           text_ratio, ro_ratio, data_ratio,
           text_ent, ro_ent, data_ent,
           zeros_ratio_total, (unsigned long long)zero_runs_16, (unsigned long long)zero_runs_32, ascii_ratio_ro,
           (unsigned long long)e8_cnt, (unsigned long long)e9_cnt, (unsigned long long)ff_cj_cnt, (unsigned long long)eb_cnt, (unsigned long long)jcc32_cnt, branch_density,
           (unsigned long long)riprel_cnt, nop_ratio, (unsigned long long)imm64_cnt, pad_ratio, (unsigned long long)ro_ptr_like_cnt,
           rel32_intext_ratio, (unsigned long long)rel32_intext, disp8_entropy,
           (unsigned long long)x_load_cnt, (unsigned long long)ro_load_cnt, (unsigned long long)rw_load_cnt, (unsigned long long)bss_sz,
           (p_align_cnt? (double)p_align_sum_log2 / (double)p_align_cnt : 0.0), (double)p_align_max_log2,
           (unsigned)(eh->e_shnum), 0u, 0.0,
           (unsigned long long)dyn_needed_cnt, (unsigned long long)dynsym_cnt, (unsigned long long)pltrel_cnt, (unsigned long long)dynrel_text_cnt,
           (text_sz ? ((double)dynrel_text_cnt / ((double)text_sz / 1024.0)) : 0.0),
           (unsigned long long)ret_cnt, rel_branch_ratio, avg_rel32_abs, (unsigned long long)rel32_abs_max, (unsigned long long)uniq_n,
           autocorr1_text, (unsigned long long)max_run00, (unsigned long long)max_run90, (unsigned long long)max_run_any_len);

    munmap(map, fsz);
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [--header] <file> [file2 ...]\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(argv[0]); return 1; }
    int first_file = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--header") == 0) { print_header = 1; continue; }
        first_file = i; break;
    }
    if (print_header) {
        printf("%s\n", CSV_HEADER);
    }
    for (int i = first_file; i < argc; ++i) {
        const char *p = argv[i];
        if (!p || !*p) continue;
        (void)probe_one(p); // silently skip non-ELF / errors
    }
    return 0;
}
