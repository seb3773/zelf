#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

#include "archive_mode.h"
#include "bcj_x86_enc.h"
#include "compression.h"
#include "depacker.h"
#include "elf_builder.h"
#include "elf_utils.h"
#define CONSOLE_USE_PROGRESS_8
#define CONSOLE_USE_RAW_CLONE
// Declare progress bar symbols as *_real (implementation in compression.c)
#define start_progress start_progress_real
#define update_progress update_progress_real
#define stop_progress stop_progress_real
#include "../others/console_progress.h"
#undef start_progress
#undef update_progress
#undef stop_progress
#include "filter_selection.h"
#include "kanzi_exe_encode_c.h"
#include "packer_defs.h"
#include "stub_selection.h"

// New terminal help rendering library
#include "help_display.h"
// Version and color centralization (also provides help_colors.h)
#include "zelf_packer.h"

static int create_backup(const char *filename) {
  char backup_name[4096];
  snprintf(backup_name, sizeof(backup_name), "%s.bak", filename);

  int src_fd = open(filename, O_RDONLY);
  if (src_fd < 0)
    return -1;

  struct stat st;
  if (fstat(src_fd, &st) != 0) {
    close(src_fd);
    return -1;
  }

  int dst_fd = open(backup_name, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
  if (dst_fd < 0) {
    close(src_fd);
    return -1;
  }

  char buf[8192];
  ssize_t n;
  while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
    if (write(dst_fd, buf, n) != n) {
      close(src_fd);
      close(dst_fd);
      return -1;
    }
  }

  close(src_fd);
  close(dst_fd);
  return 0;
}

// Global filter selection defined in packer_defs.h, defined here
exe_filter_t g_exe_filter = EXE_FILTER_AUTO;
int g_verbose = 0;
int g_no_progress = 0;
int g_no_colors = 0; // Global monochrome flag (affects help and all packer outputs)

#define KB (1024ull)
#define MB (1024ull * 1024ull)

typedef struct {
  size_t min_size;
  size_t max_size;
  const char *codecs[3];
} size_codec_rule_t;

// Table derived from stats/results/rankings_by_size.csv (top 3 per range)
static const size_codec_rule_t k_size_codec_table[] = {
    {0, 15 * KB - 1, {"zx0", "zx7b", "shrinkler"}},
    {15 * KB, 50 * KB - 1, {"shrinkler", "zx0", "zx7b"}},
    {50 * KB, 100 * KB - 1, {"shrinkler", "lzma", "zx0"}},
    {100 * KB, 200 * KB - 1, {"lzma", "shrinkler", "zx0"}},
    {200 * KB, 300 * KB - 1, {"lzma", "shrinkler", "apultra"}},
    {300 * KB, 400 * KB - 1, {"lzma", "shrinkler", "apultra"}},
    {400 * KB, 450 * KB - 1, {"lzma", "shrinkler", "exo"}},
    {450 * KB, 500 * KB - 1, {"lzma", "shrinkler", "apultra"}},
    {500 * KB, 600 * KB - 1, {"lzma", "shrinkler", "apultra"}},
    {600 * KB, 800 * KB - 1, {"lzma", "shrinkler", "apultra"}},
    {800 * KB, 1 * MB - 1, {"lzma", "shrinkler", "apultra"}},
    {1 * MB, ((size_t)3 * MB) / 2 - 1, {"lzma", "shrinkler", "zstd"}},
    {((size_t)3 * MB) / 2, 4 * MB - 1, {"lzma", "shrinkler", "zstd"}},
    {4 * MB, ((size_t)15 * MB) / 2 - 1, {"lzma", "zstd", "apultra"}},
    {((size_t)15 * MB) / 2, 12 * MB - 1, {"lzma", "zstd", "apultra"}},
    {12 * MB, 15 * MB - 1, {"lzma", "zstd", "shrinkler"}},
    {15 * MB, 20 * MB - 1, {"lzma", "shrinkler", "zstd"}},
    {20 * MB, 40 * MB - 1, {"lzma", "zstd", "apultra"}},
    {40 * MB, 50 * MB - 1, {"lzma", "zstd", "apultra"}},
    {50 * MB, (size_t)-1, {"lzma", "zstd", "apultra"}},
};

static const char *filter_name(exe_filter_t f) {
  switch (f) {
  case EXE_FILTER_AUTO:
    return "auto";
  case EXE_FILTER_KANZIEXE:
    return "kanzi";
  case EXE_FILTER_BCJ:
    return "bcj";
  case EXE_FILTER_NONE:
    return "none";
  default:
    return "unknown";
  }
}

static int select_codecs_for_size(size_t size, const char *out_codecs[3]) {
  for (size_t i = 0; i < sizeof(k_size_codec_table) / sizeof(k_size_codec_table[0]);
       ++i) {
    if (size >= k_size_codec_table[i].min_size &&
        size <= k_size_codec_table[i].max_size) {
      out_codecs[0] = k_size_codec_table[i].codecs[0];
      out_codecs[1] = k_size_codec_table[i].codecs[1];
      out_codecs[2] = k_size_codec_table[i].codecs[2];
      return 0;
    }
  }
  return -1;
}

// Detailed help/usage
static void print_help(const char *prog) {
  (void)prog; // new help UI does not require program name
  help_display_show_overview(g_no_colors ? GFALSE : GTRUE);
}

static void print_main_header(void) {
  printf("\n");
  pk_emit_raw(ZELF_H2);
  printf("%s\n", PK_RES);
  printf("         %sv" ZELF_VERSION "%s -- By Seb3773%s\n\n", PKC(FYW),
         PKC(GREY17), PK_RES);
}

static int compute_effective_size(const char *input_path, int no_strip,
                                  size_t *actual_size_out) {
  int rc = 1;
  int fd = -1;
  unsigned char *data = NULL;
  size_t size = 0;

  fd = open(input_path, O_RDONLY);
  if (fd < 0) {
    perror("open");
    return rc;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    perror("fstat");
    goto cleanup;
  }
  size = st.st_size;

  data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED) {
    perror("mmap");
    data = NULL;
    goto cleanup;
  }

  if (size < sizeof(Elf64_Ehdr)) {
    fprintf(stderr, "[%s%s%s] Error: Input too small for ELF header\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    fprintf(stderr, "[%s%s%s] Error: Not an ELF (bad magic)\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
    fprintf(stderr, "[%s%s%s] Error: Only 64-bit ELF supported\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }
  if (ehdr->e_machine != EM_X86_64) {
    fprintf(stderr, "[%s%s%s] Error: Only x86_64 ELF supported\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }
  if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
    fprintf(stderr, "[%s%s%s] Error: Input must be ET_EXEC or ET_DYN\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0 ||
      ehdr->e_phoff + (size_t)ehdr->e_phnum * ehdr->e_phentsize > size) {
    fprintf(stderr, "[%s%s%s] Error: Invalid program header table\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  if (no_strip) {
    *actual_size_out = size;
    rc = 0;
    goto cleanup;
  }

  Elf64_Phdr *phdr = (Elf64_Phdr *)(data + ehdr->e_phoff);
  size_t ph_table_end =
      ehdr->e_phoff + (size_t)ehdr->e_phnum * (size_t)ehdr->e_phentsize;
  size_t newsize = ehdr->e_ehsize;
  if (ph_table_end > newsize)
    newsize = ph_table_end;
  for (int i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_NULL) {
      size_t n = (size_t)phdr[i].p_offset + (size_t)phdr[i].p_filesz;
      if (n > newsize)
        newsize = n;
    }
  }
  if (newsize > size)
    newsize = size;

  size_t zsize = newsize;
  while (zsize > 0 && data[zsize - 1] == 0)
    --zsize;
  size_t ph_min = (size_t)ehdr->e_phoff +
                  (size_t)ehdr->e_phnum * (size_t)ehdr->e_phentsize;
  if (zsize < ph_min)
    zsize = ph_min;
  if (zsize < (size_t)ehdr->e_ehsize)
    zsize = (size_t)ehdr->e_ehsize;
  if (zsize == 0)
    zsize = newsize;
  newsize = zsize;

  *actual_size_out = newsize;
  rc = 0;

cleanup:
  if (data)
    munmap(data, size);
  if (fd >= 0)
    close(fd);
  return rc;
}

static int pack_elf_once(const char *input_path, const char *output_path,
                         const char *codec, int no_strip, int use_password,
                         uint64_t pwd_salt, uint64_t pwd_obf, int print_header,
                         int fatal_on_error, int has_forced_filter,
                         exe_filter_t forced_filter, int quiet_output) {
  int rc = 1;
  int fd = -1;
  unsigned char *data = NULL;
  unsigned char *slim = NULL;
  unsigned char *filtered_buf = NULL;
  unsigned char *compressed = NULL;
  size_t size = 0;
  exe_filter_t saved_filter = g_exe_filter;

  if (has_forced_filter)
    g_exe_filter = forced_filter;

  const char *marker = "zELFl4";
  if (strcmp(codec, "apultra") == 0)
    marker = "zELFap";
  else if (strcmp(codec, "zx7b") == 0)
    marker = "zELFzx";
  else if (strcmp(codec, "zx0") == 0)
    marker = "zELFz0";
  else if (strcmp(codec, "exo") == 0)
    marker = "zELFex";
  else if (strcmp(codec, "blz") == 0)
    marker = "zELFbz";
  else if (strcmp(codec, "pp") == 0)
    marker = "zELFpp";
  else if (strcmp(codec, "snappy") == 0)
    marker = "zELFsn";
  else if (strcmp(codec, "doboz") == 0)
    marker = "zELFdz";
  else if (strcmp(codec, "qlz") == 0)
    marker = "zELFqz";
  else if (strcmp(codec, "lzav") == 0)
    marker = "zELFlv";
  else if (strcmp(codec, "lzma") == 0)
    marker = "zELFla";
  else if (strcmp(codec, "zstd") == 0)
    marker = "zELFzd";
  else if (strcmp(codec, "shrinkler") == 0)
    marker = "zELFsh";
  else if (strcmp(codec, "sc") == 0)
    marker = "zELFsc";
  else if (strcmp(codec, "lzsa") == 0)
    marker = "zELFls";

  if (print_header) {
  printf("\n");
  pk_emit_raw(ZELF_H2);
  printf("%s\n", PK_RES);
    printf("         %sv" ZELF_VERSION "%s -- By Seb3773%s\n\n", PKC(FYW),
           PKC(GREY17), PK_RES);
    printf("[%s%s%s] Packing ELF: %s\n", PK_ARROW, PK_SYM_ARROW, PK_RES,
           input_path);
  }

  fd = open(input_path, O_RDONLY);
  if (fd < 0) {
    perror("open ELF");
    if (fatal_on_error)
    fail("open ELF");
    goto cleanup;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    perror("fstat");
    if (fatal_on_error)
      fail("fstat");
    goto cleanup;
  }
  size = st.st_size;

  data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED) {
    perror("mmap ELF");
    if (fatal_on_error)
    fail("mmap ELF");
    data = NULL;
    goto cleanup;
  }

  // Validate ELF file before processing
  if (size < sizeof(Elf64_Ehdr)) {
    fprintf(stderr,
            "[%s%s%s] Error: Input file is too small to be a valid ELF binary\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;

  // Check ELF magic
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    fprintf(stderr, "[%s%s%s] Error: Input file is not a valid ELF binary (bad magic)\n",
                    PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  // Check ELF class (must be 64-bit)
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
    fprintf(stderr, "[%s%s%s] Error: Only 64-bit ELF binaries are supported\n",
                    PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  // Check architecture (must be x86_64)
  if (ehdr->e_machine != EM_X86_64) {
    fprintf(stderr, "[%s%s%s] Error: Only x86_64 ELF binaries are supported\n",
                    PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  // Check ELF type (must be executable or shared object)
  if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
    fprintf(stderr, "[%s%s%s] Error: Input must be an executable or shared object ELF\n",
                    PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  // Validate program header table bounds
  if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0 ||
      ehdr->e_phoff + (size_t)ehdr->e_phnum * ehdr->e_phentsize > size) {
    fprintf(
        stderr,
        "[%s%s%s] Error: Invalid ELF program header table\n", PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }

  Elf64_Phdr *phdr = (Elf64_Phdr *)(data + ehdr->e_phoff);

  VPRINTF("[%s%s%s] Computing a safe address to avoid collisions...\n", PK_ARROW, PK_SYM_ARROW, PK_RES);
  Elf64_Addr safe_stub_vaddr = find_safe_stub_vaddr(phdr, ehdr->e_phnum);

  SegmentInfo segments[16];
  size_t segment_count = 0;
  Elf64_Addr base_vaddr = (Elf64_Addr)-1;
  Elf64_Xword total_size = 0;

  int has_pt_interp = 0;
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_INTERP) {
      has_pt_interp = 1;
      break;
    }
  }

  if (has_pt_interp) {
    VPRINTF("[%s%s%s] DYNAMIC ELF detected (PT_INTERP present)\n", PK_INFO, PK_SYM_INFO, PK_RES);
  } else {
    VPRINTF("[%s%s%s] STATIC ELF detected (no PT_INTERP)\n", PK_INFO, PK_SYM_INFO, PK_RES);
  }

  const unsigned char *stub_start = NULL, *stub_end = NULL;
  exe_filter_t saved_filter_sel = g_exe_filter;
  if (g_exe_filter == EXE_FILTER_NONE)
    g_exe_filter = EXE_FILTER_BCJ;
  select_embedded_stub(codec, has_pt_interp, use_password, &stub_start,
                       &stub_end);
  g_exe_filter = saved_filter_sel;

  if (!stub_start || !stub_end || stub_end <= stub_start) {
    if (fatal_on_error)
    fail("embedded stub missing");
    fprintf(stderr, "[%s%s%s] Error: embedded stub missing\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    goto cleanup;
  }
  VPRINTF("[%s%s%s] Using embedded stub: %s (%s)\n", PK_INFO, PK_SYM_INFO, PK_RES,
          codec, has_pt_interp ? "dynamic" : "static");

  size_t stub_size = (size_t)(stub_end - stub_start);
  VPRINTF("[%s%s%s] Stub size: %zu bytes\n", PK_INFO, PK_SYM_INFO, PK_RES, stub_size);

  // Analyze segments
  VPRINTF("[%s%s%s] Analyzing PT_LOAD segments:\n", PK_ARROW, PK_SYM_ARROW, PK_RES);
  for (int i = 0; i < ehdr->e_phnum; i++) {
    Elf64_Phdr *p = &phdr[i];
    if (p->p_type == PT_LOAD) {
      segments[segment_count].vaddr = p->p_vaddr;
      segments[segment_count].offset = p->p_offset;
      segments[segment_count].filesz = p->p_filesz;
      segments[segment_count].memsz = p->p_memsz;
      segments[segment_count].flags = p->p_flags;
      const char *type = "unknown";
      if (p->p_flags & PF_X)
        type = ".text";
      else if (p->p_flags & PF_W)
        type = ".data";
      else if (p->p_flags & PF_R)
        type = ".rodata";
      segments[segment_count].name = type;

      int is_readonly = !(p->p_flags & PF_W);
      VPRINTF("[%s%s%s] Segment %zu [%c%c%c]: %s vaddr=0x%lx "
              "offset=0x%lx size=%zu → %s\n",
              PK_INFO, PK_SYM_INFO, PK_RES,
              segment_count, (p->p_flags & PF_R) ? 'R' : '-',
              (p->p_flags & PF_W) ? 'W' : '-', (p->p_flags & PF_X) ? 'X' : '-',
              type, p->p_vaddr, p->p_offset, p->p_filesz,
              is_readonly ? "COMPRESSIBLE" : "LEAVE UNTOUCHED");

      segment_count++;

      if (p->p_vaddr < base_vaddr)
        base_vaddr = p->p_vaddr;
      Elf64_Addr end = p->p_vaddr + p->p_filesz;
      if (end - base_vaddr > total_size)
        total_size = end - base_vaddr;
    }
  }

  VPRINTF("[%s%s%s] Virtual base: 0x%lx, Total size: %zu\n", PK_INFO, PK_SYM_INFO, PK_RES,
          base_vaddr, total_size);
  VPRINTF("[%s%s%s] Original file size: %zu bytes\n", PK_INFO, PK_SYM_INFO, PK_RES, size);

  size_t text_off = (size_t)-1;
  size_t text_end = 0;
  size_t max_text_sz = 0;
  for (size_t i = 0; i < segment_count; ++i) {
    if (segments[i].flags & PF_X) {
      size_t off = (size_t)segments[i].offset;
      size_t end = (size_t)segments[i].offset + (size_t)segments[i].filesz;
      size_t sz = (size_t)segments[i].filesz;
      if (sz > max_text_sz) {
        max_text_sz = sz;
        text_off = off;
        text_end = end;
      }
    }
  }
  if (max_text_sz == 0 || text_off == (size_t)-1 || text_end <= text_off) {
    text_off = 0;
    text_end = 9;
    max_text_sz = 9;
  }

  unsigned char *combined = NULL;
  size_t actual_size = 0;

  if (!no_strip) {
    VPRINTF("[%s%s%s] Stripping binary...\n", PK_ARROW, PK_SYM_ARROW, PK_RES);
    size_t ph_table_end =
        ehdr->e_phoff + (size_t)ehdr->e_phnum * (size_t)ehdr->e_phentsize;
    size_t newsize = ehdr->e_ehsize;
    if (ph_table_end > newsize)
      newsize = ph_table_end;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
      if (phdr[i].p_type != PT_NULL) {
        size_t n = (size_t)phdr[i].p_offset + (size_t)phdr[i].p_filesz;
        if (n > newsize)
          newsize = n;
      }
    }
    if (newsize > size)
      newsize = size;

    slim = (unsigned char *)malloc(newsize);
    if (!slim) {
      if (fatal_on_error)
      fail("malloc super-strip buffer");
      fprintf(stderr, "[%s%s%s] Error: malloc super-strip buffer\n",
              PK_ERR, PK_SYM_ERR, PK_RES);
      goto cleanup;
    }
    memcpy(slim, data, newsize);

    Elf64_Ehdr *nehdr = (Elf64_Ehdr *)slim;
    size_t zsize = newsize;
    while (zsize > 0 && slim[zsize - 1] == 0)
      --zsize;
    size_t ph_min = (size_t)nehdr->e_phoff +
                    (size_t)nehdr->e_phnum * (size_t)nehdr->e_phentsize;
    if (zsize < ph_min)
      zsize = ph_min;
    if (zsize < (size_t)nehdr->e_ehsize)
      zsize = (size_t)nehdr->e_ehsize;
    if (zsize == 0)
      zsize = newsize;
    newsize = zsize;

    if ((size_t)nehdr->e_shoff >= newsize) {
      nehdr->e_shoff = 0;
      nehdr->e_shnum = 0;
      nehdr->e_shstrndx = 0;
    }
    Elf64_Phdr *nphdr = (Elf64_Phdr *)(slim + nehdr->e_phoff);
    for (int i = 0; i < nehdr->e_phnum; ++i) {
      if ((size_t)nphdr[i].p_offset >= newsize) {
        nphdr[i].p_offset = (Elf64_Off)newsize;
        nphdr[i].p_filesz = 0;
      } else if ((size_t)nphdr[i].p_offset + (size_t)nphdr[i].p_filesz >
                 newsize) {
        nphdr[i].p_filesz = (Elf64_Xword)(newsize - (size_t)nphdr[i].p_offset);
      }
    }

    VPRINTF(
        "[%s%s%s] Super-strip: %zu → %zu bytes (saved %zu)\n",
        PK_INFO, PK_SYM_INFO, PK_RES, size, newsize, size - newsize);
    combined = slim;
    actual_size = newsize;
  } else {
    combined = (unsigned char *)data;
    actual_size = size;
  }

  if (g_exe_filter == EXE_FILTER_AUTO) {
    exe_filter_t auto_sel;
    if (strcmp(codec, "blz") == 0)
      auto_sel = decide_exe_filter_auto_blz_model(combined, actual_size,
                                                  segments, segment_count,
                                                  text_off, text_end, size);
    else if (strcmp(codec, "zx7b") == 0)
      auto_sel = decide_exe_filter_auto_zx7b_model(combined, actual_size,
                                                   segments, segment_count,
                                                   text_off, text_end, size);
    else if (strcmp(codec, "zx0") == 0) {
      auto_sel = decide_exe_filter_auto_zx0_model(combined, actual_size,
                                                  segments, segment_count,
                                                  text_off, text_end, size);
      VPRINTF(
          "[%s%s%s] Heuristic ZX0 filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (auto_sel == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    } else if (strcmp(codec, "qlz") == 0)
      auto_sel = decide_exe_filter_auto_qlz_model(combined, actual_size,
                                                  segments, segment_count,
                                                  text_off, text_end, size);
    else if (strcmp(codec, "pp") == 0)
      auto_sel = decide_exe_filter_auto_pp_rule(combined, actual_size, segments,
                                                segment_count, text_off,
                                                text_end, size);
    else if (strcmp(codec, "lz4") == 0)
      auto_sel = decide_exe_filter_auto_lz4_model(combined, actual_size,
                                                  segments, segment_count,
                                                  text_off, text_end, size);
    else if (strcmp(codec, "lzav") == 0)
      auto_sel = decide_exe_filter_auto_lzav_model(combined, actual_size,
                                                   segments, segment_count,
                                                   text_off, text_end, size);
    else if (strcmp(codec, "lzma") == 0)
      auto_sel = decide_exe_filter_auto_lzma_model(combined, actual_size,
                                                   segments, segment_count,
                                                   text_off, text_end, size);
    else if (strcmp(codec, "exo") == 0)
      auto_sel = decide_exe_filter_auto_exo_model(combined, actual_size,
                                                  segments, segment_count,
                                                  text_off, text_end, size);
    else if (strcmp(codec, "zstd") == 0)
      auto_sel = decide_exe_filter_auto_zstd_model(combined, actual_size,
                                                   segments, segment_count,
                                                   text_off, text_end, size);
    else if (strcmp(codec, "doboz") == 0)
      auto_sel = decide_exe_filter_auto_doboz_model(combined, actual_size,
                                                    segments, segment_count,
                                                    text_off, text_end, size);
    else if (strcmp(codec, "snappy") == 0)
      auto_sel = decide_exe_filter_auto_snappy_model(combined, actual_size,
                                                     segments, segment_count,
                                                     text_off, text_end, size);
    else if (strcmp(codec, "apultra") == 0)
      auto_sel = decide_exe_filter_auto_apultra_model(combined, actual_size,
                                                      segments, segment_count,
                                                      text_off, text_end, size);
    else if (strcmp(codec, "shrinkler") == 0)
      auto_sel = decide_exe_filter_auto_shrinkler_model(
          combined, actual_size, segments, segment_count, text_off, text_end,
          size);
    else if (strcmp(codec, "sc") == 0)
      auto_sel = decide_exe_filter_auto_sc_model(combined, actual_size,
                                                 segments, segment_count,
                                                 text_off, text_end, size);
    else if (strcmp(codec, "lzsa") == 0)
      auto_sel = decide_exe_filter_auto_lzsa_model(combined, actual_size,
                                                   segments, segment_count,
                                                   text_off, text_end, size);
    else
      auto_sel =
          decide_exe_filter_auto_v21(combined, actual_size, text_off, text_end);

    g_exe_filter = auto_sel;
    if (strcmp(codec, "lz4") == 0) {
      VPRINTF(
          "[%s%s%s] Heuristic LZ4 filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    } else if (strcmp(codec, "apultra") == 0) {
      VPRINTF("[%s%s%s] Heuristic APULTRA filter choice: %s chosen\n",
              PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    }
    if (strcmp(codec, "blz") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic BLZ filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "pp") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic PP filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "lzav") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic LZAV filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "lzma") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic LZMA filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "exo") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic EXO filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "zstd") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic ZSTD filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "qlz") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic QLZ filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "snappy") == 0)
      VPRINTF("[%s%s%s] Heuristic SNAPPY filter choice: %s chosen\n",
              PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "doboz") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic DoboZ filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "zx7b") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic ZX7B filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "shrinkler") == 0)
      VPRINTF("[%s%s%s] Heuristic SHRINKLER filter choice: %s chosen\n",
              PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "sc") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic SC filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");
    else if (strcmp(codec, "lzsa") == 0)
      VPRINTF(
          "[%s%s%s] Heuristic LZSA filter choice: %s chosen\n",
          PK_INFO, PK_SYM_INFO, PK_RES, (g_exe_filter == EXE_FILTER_BCJ) ? "BCJ" : "KanziExe");

    // Reselect stub
    exe_filter_t saved_filter_sel2 = g_exe_filter;
    if (g_exe_filter == EXE_FILTER_NONE)
      g_exe_filter = EXE_FILTER_BCJ;
    select_embedded_stub(codec, has_pt_interp, use_password, &stub_start,
                         &stub_end);
    g_exe_filter = saved_filter_sel2;
    stub_size = (size_t)(stub_end - stub_start);
    VPRINTF("[%s%s%s] Stub re-selected (AUTO): %zu bytes\n", PK_INFO, PK_SYM_INFO, PK_RES,
            stub_size);
  }

  // Apply filter if selected
  unsigned char *data_to_compress = combined;
  size_t size_to_compress = actual_size;
  size_t filtered_size = actual_size;

  if (g_exe_filter == EXE_FILTER_KANZIEXE) {
    int max_sz = kanzi_exe_get_max_encoded_length((int)actual_size);
    filtered_buf = (unsigned char *)malloc(max_sz);
    if (!filtered_buf) {
      if (fatal_on_error)
      fail("malloc kanzi");
      fprintf(stderr, "[%s%s%s] Error: malloc kanzi\n",
              PK_ERR, PK_SYM_ERR, PK_RES);
      goto cleanup;
    }
    int processed = 0;
    int fsz;
    if (text_off != (size_t)-1 && text_end > text_off) {
      fsz = kanzi_exe_filter_encode_force_x86_range(
          combined, (int)actual_size, filtered_buf, max_sz, &processed,
          (int)text_off, (int)text_end);
    } else {
      fsz = kanzi_exe_filter_encode(combined, (int)actual_size, filtered_buf,
                                    max_sz, &processed);
    }

    if (fsz > 0) {
      data_to_compress = filtered_buf;
      size_to_compress = (size_t)fsz;
      filtered_size = (size_t)fsz;
      VPRINTF("[%s%s%s] Kanzi EXE filter applied: %zu bytes\n", PK_INFO, PK_SYM_INFO, PK_RES,
              filtered_size);
    } else {
      VPRINTF("[%s%s%s] KanziExe failed, using raw data\n", PK_WARN, PK_SYM_WARN, PK_RES);
      free(filtered_buf);
      filtered_buf = NULL;
    }
  } else if (g_exe_filter == EXE_FILTER_BCJ) {
    filtered_buf = (unsigned char *)malloc(actual_size);
    if (!filtered_buf) {
      if (fatal_on_error)
      fail("malloc bcj");
      fprintf(stderr, "[%s%s%s] Error: malloc bcj\n",
              PK_ERR, PK_SYM_ERR, PK_RES);
      goto cleanup;
    }
    memcpy(filtered_buf, combined, actual_size);
    size_t done = bcj_x86_encode(filtered_buf, actual_size, 0);
    if (done > 0) {
      data_to_compress = filtered_buf;
      size_to_compress = actual_size;
      filtered_size = actual_size;
      VPRINTF("[%s%s%s] BCJ filter applied: %zu bytes\n", PK_INFO, PK_SYM_INFO, PK_RES,
              actual_size);
    } else {
      VPRINTF("[%s%s%s] BCJ failed/no-op\n", PK_WARN, PK_SYM_WARN, PK_RES);
      free(filtered_buf);
      filtered_buf = NULL;
    }
  }

  // Compression
  int comp_size = 0;
  if (compress_data_with_codec(codec, data_to_compress, size_to_compress,
                               &compressed, &comp_size) != 0) {
    if (fatal_on_error)
    fail("Compression failed");
    fprintf(stderr, "[%s%s%s] Error: compression failed for codec %s\n",
            PK_ERR, PK_SYM_ERR, PK_RES, codec);
    goto cleanup;
  }

  VPRINTF("[%s%s%s] Stub size: %zu, Compressed data: %d, Estimated total: %zu\n",
          PK_INFO, PK_SYM_INFO, PK_RES, stub_size, comp_size, stub_size + (size_t)comp_size);
  VPRINTF("[%s%s%s] Estimated ratio (vs original): %.1f%%\n", PK_INFO, PK_SYM_INFO, PK_RES,
          100.0f * (float)(stub_size + (size_t)comp_size) / (float)size);

  // Debug: display the first compressed bytes
  if (g_verbose) {
    printf("[%s%s%s] First compressed bytes: ", PK_INFO, PK_SYM_INFO, PK_RES);
    for (int i = 0; i < 16 && i < comp_size; i++) {
      printf("%02x ", compressed[i]);
    }
    printf("\n");
  }

  // Offset calculations
  Elf64_Off stub_off = 0x1000;
  size_t estimated_stub_sz = 15000;
  if (strcmp(codec, "zstd") == 0)
    estimated_stub_sz = 65536;
  Elf64_Off packed_off = stub_off + align_up(estimated_stub_sz, ALIGN);
  Elf64_Addr stub_vaddr = has_pt_interp ? 0x0 : safe_stub_vaddr;
  Elf64_Addr packed_vaddr = stub_vaddr + packed_off - stub_off;

  VPRINTF(
      "[%s%s%s] ELF entry point: 0x%lx (stored directly)\n",
      PK_INFO, PK_SYM_INFO, PK_RES, ehdr->e_entry);
  VPRINTF("[%s%s%s] After computing entry_offset...\n", PK_INFO, PK_SYM_INFO, PK_RES);

  if (has_pt_interp) {
    VPRINTF("[%s%s%s] DYNAMIC ELF: RELATIVE addresses (vaddr=0x0)\n", PK_INFO, PK_SYM_INFO, PK_RES);
  } else {
    VPRINTF("[%s%s%s] STATIC ELF: ABSOLUTE addresses (vaddr=0x%lx)\n",
            PK_INFO, PK_SYM_INFO, PK_RES, stub_vaddr);
  }

  VPRINTF("[%s%s%s] stub_off=0x%lx, estimated_stub_sz=%zu, ALIGN=%d\n",
          PK_INFO, PK_SYM_INFO, PK_RES, stub_off, estimated_stub_sz, ALIGN);
  VPRINTF("[%s%s%s] align_up(%zu, %d) = %zu\n",
          PK_INFO, PK_SYM_INFO, PK_RES, estimated_stub_sz, ALIGN, align_up(estimated_stub_sz, ALIGN));
  VPRINTF("[%s%s%s] packed_off computed = 0x%lx\n", PK_INFO, PK_SYM_INFO, PK_RES, packed_off);

  VPRINTF("[%s%s%s] Rebuilding ELF binary (%s)...\n",
          PK_ARROW, PK_SYM_ARROW, PK_RES, has_pt_interp ? "DYNAMIC" : "STATIC");
  VPRINTF("[%s%s%s] stub_vaddr=0x%lx, packed_vaddr=0x%lx\n",
          PK_INFO, PK_SYM_INFO, PK_RES, stub_vaddr, packed_vaddr);

  VPRINTF("[%s%s%s] Offsets: stub=0x%lx, packed=0x%lx, vaddr=0x%lx\n",
          PK_INFO, PK_SYM_INFO, PK_RES, stub_off, packed_off, packed_vaddr);

  uint8_t elfz_flags = 0;
  if (g_exe_filter == EXE_FILTER_BCJ) {
    elfz_flags |= 1;
  }

  uint64_t entry_point = ehdr->e_entry;
  munmap(data, size);
  data = NULL;
  close(fd);
  fd = -1;

  int saved_stdout = -1;
  int devnull_fd = -1;
  if (quiet_output) {
    fflush(stdout);
    saved_stdout = dup(STDOUT_FILENO);
    devnull_fd = open("/dev/null", O_WRONLY);
    if (devnull_fd >= 0) {
      dup2(devnull_fd, STDOUT_FILENO);
    }
  } else {
  VPRINTF("[%s%s%s] Writing packed binary to: %s\n",
            PK_INFO, PK_SYM_INFO, PK_RES, output_path);
  }

  write_packed_binary(output_path, has_pt_interp, stub_start, stub_size,
                      compressed, comp_size, actual_size, entry_point,
                      filtered_size, codec, g_exe_filter, stub_off, stub_vaddr,
                      packed_vaddr, packed_off, marker, elfz_flags,
                      use_password, pwd_salt, pwd_obf);

  if (quiet_output) {
    fflush(stdout);
    if (saved_stdout >= 0)
      dup2(saved_stdout, STDOUT_FILENO);
    if (devnull_fd >= 0)
      close(devnull_fd);
    if (saved_stdout >= 0)
      close(saved_stdout);
  }

  rc = 0;

cleanup:
  if (data)
    munmap(data, size);
  if (fd >= 0)
    close(fd);
  if (slim)
    free(slim);
  if (compressed)
  free(compressed);
  if (filtered_buf)
    free(filtered_buf);
  g_exe_filter = saved_filter;
  return rc;
}

static int copy_file(const char *src, const char *dst) {
  int in_fd = open(src, O_RDONLY);
  if (in_fd < 0)
    return -1;
  struct stat st;
  if (fstat(in_fd, &st) != 0) {
    close(in_fd);
    return -1;
  }
  int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
  if (out_fd < 0) {
    close(in_fd);
    return -1;
  }
  char buf[8192];
  ssize_t n;
  int rc = 0;
  while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
    ssize_t w = write(out_fd, buf, n);
    if (w != n) {
      rc = -1;
      break;
    }
  }
  if (n < 0)
    rc = -1;
  close(in_fd);
  close(out_fd);
  return rc;
}

static int run_best_mode(const char *input_path, const char *final_output_path,
                         int no_strip, int use_password, uint64_t pwd_salt,
                         uint64_t pwd_obf) {
  struct stat st_input;
  if (stat(input_path, &st_input) != 0) {
    perror("stat input");
    return 1;
  }
  size_t original_size = (size_t)st_input.st_size;

  size_t effective_size = 0;
  if (compute_effective_size(input_path, no_strip, &effective_size) != 0) {
    fprintf(stderr, "[%s%s%s] Error: unable to compute effective size for -best\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    return 1;
  }

  const char *best_codecs[3] = {0};
  if (select_codecs_for_size(effective_size, best_codecs) != 0) {
    fprintf(stderr, "[%s%s%s] Error: no codec mapping for size=%zu (should not happen)\n",
            PK_ERR, PK_SYM_ERR, PK_RES, effective_size);
    return 1;
  }

  const char *tmp_dir = "/tmp";
  if (access(tmp_dir, W_OK) != 0) {
    tmp_dir = ".";
    if (g_verbose) {
      VPRINTF("[%s%s%s] /tmp not writable, using current directory for temps\n",
              PK_WARN, PK_SYM_WARN, PK_RES);
    }
  }

  char *base_dup = strdup(input_path);
  if (!base_dup) {
    perror("strdup");
    return 1;
  }
  char *base = basename(base_dup);
  pid_t pid = getpid();

  char temp_a[4096], temp_b[4096];
  snprintf(temp_a, sizeof(temp_a), "%s/%s_zelf0_%d", tmp_dir, base, (int)pid);
  snprintf(temp_b, sizeof(temp_b), "%s/%s_zelf1_%d", tmp_dir, base, (int)pid);

  char best_path[4096] = {0};
  size_t best_size = 0;
  const char *best_codec = NULL;
  exe_filter_t best_filter = EXE_FILTER_AUTO;
  int have_best = 0;

  int saved_no_progress = g_no_progress;
  int show_global_progress = (!g_verbose && !g_no_progress);
  int attempts_done = 0;
  const int attempts_total = 9;

  printf("[%s%s%s] -best mode: trying best predicted compressors (%s / %s / %s)\n",
         PK_INFO, PK_SYM_INFO, PK_RES, best_codecs[2], best_codecs[1],
         best_codecs[0]);
  if (show_global_progress) {
    start_progress_real("[best] overall", 8);
    update_progress_real(0, 8);
  }

  exe_filter_t filters_to_try[3] = {EXE_FILTER_BCJ, EXE_FILTER_KANZIEXE,
                                    EXE_FILTER_NONE};

  for (int ci = 2; ci >= 0; --ci) {
    const char *codec = best_codecs[ci];
    for (int fi = 0; fi < 3; ++fi) {
      exe_filter_t f = filters_to_try[fi];
      const char *target;
      if (!have_best) {
        target = temp_a;
      } else if (strcmp(best_path, temp_a) == 0) {
        target = temp_b;
      } else {
        target = temp_a;
      }

      unlink(target);
      if (g_verbose) {
        printf("\n");
        printf("[%s%s%s] -best try: codec=%s, filter=%s → %s\n",
               PK_INFO, PK_SYM_INFO, PK_RES, codec, filter_name(f), target);
      }

      // Silence inner codec progress bars when showing global progress
      if (show_global_progress)
        g_no_progress = 1;
      int rc = pack_elf_once(input_path, target, codec, no_strip, use_password,
                             pwd_salt, pwd_obf, /*print_header=*/0,
                             /*fatal_on_error=*/0,
                             /*has_forced_filter=*/1, f,
                             /*quiet_output=*/!g_verbose);
      if (show_global_progress)
        g_no_progress = saved_no_progress;
      if (rc != 0) {
        if (g_verbose) {
          fprintf(stderr, "[%s%s%s] Skipping combo %s/%s (error)\n",
                  PK_WARN, PK_SYM_WARN, PK_RES, codec, filter_name(f));
        }
        unlink(target);
        attempts_done++;
        if (show_global_progress) {
          int pct = (attempts_done * 100) / attempts_total;
          if (pct > 100)
            pct = 100;
          update_progress_real(pct, 8);
        }
        continue;
      }

      struct stat st;
      if (stat(target, &st) != 0) {
        perror("stat temp");
        unlink(target);
        attempts_done++;
        if (show_global_progress) {
          int pct = (attempts_done * 100) / attempts_total;
          if (pct > 100)
            pct = 100;
          update_progress_real(pct, 8);
        }
        continue;
      }

      if (!have_best || (size_t)st.st_size < best_size) {
        if (have_best)
          unlink(best_path);
        strncpy(best_path, target, sizeof(best_path) - 1);
        best_size = (size_t)st.st_size;
        best_codec = codec;
        best_filter = f;
        have_best = 1;
      } else {
        unlink(target);
      }

      attempts_done++;
      if (show_global_progress) {
        int pct = (attempts_done * 100) / attempts_total;
        if (pct > 100)
          pct = 100;
        update_progress_real(pct, 8);
      }
    }
  }

  if (!have_best) {
    fprintf(stderr, "[%s%s%s] Error: no successful combination for -best\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    if (show_global_progress) {
      stop_progress_real(8);
      g_no_progress = saved_no_progress;
    }
    free(base_dup);
    return 1;
  }

  // Move best to final output (rename then copy fallback)
  if (rename(best_path, final_output_path) != 0) {
    if (errno == EXDEV) {
      if (copy_file(best_path, final_output_path) != 0) {
        perror("copy temp");
        free(base_dup);
        return 1;
      }
      unlink(best_path);
    } else {
      perror("rename temp");
      if (show_global_progress) {
        stop_progress_real(8);
        g_no_progress = saved_no_progress;
      }
      free(base_dup);
      return 1;
    }
  }

  // Cleanup remaining temp (the other slot)
  if (strcmp(best_path, temp_a) != 0)
    unlink(temp_a);
  if (strcmp(best_path, temp_b) != 0)
    unlink(temp_b);

  double ratio = original_size ? ((double)best_size / (double)original_size) * 100.0 : 0.0;
  if (show_global_progress) {
    update_progress_real(100, 8);
    stop_progress_real(8);
    g_no_progress = saved_no_progress;
  }

  if (g_verbose) {
    printf("[%s%s%s] -best result: codec=%s filter=%s size=%zu bytes (ratio=%.1f%% vs original %zu)\n",
           PK_OK, PK_SYM_OK, PK_RES, best_codec ? best_codec : "?",
           filter_name(best_filter), best_size, ratio, original_size);
    printf("[%s%s%s] Output written to: %s\n", PK_INFO, PK_SYM_INFO, PK_RES,
           final_output_path);
  } else {
    printf("[%s%s%s] -best result: codec=%s filter=%s size=%zu bytes (ratio=%.1f%% vs original %zu)\n",
           PK_OK, PK_SYM_OK, PK_RES, best_codec ? best_codec : "?",
           filter_name(best_filter), best_size, ratio, original_size);
    printf("[%s%s%s] Output written to: %s\n", PK_INFO, PK_SYM_INFO, PK_RES,
           final_output_path);
  }

  free(base_dup);
  return 0;
}


int main(int argc, char **argv) {
  const char *codec = "lz4";
  int codec_explicit = 0;
  int mode_best = 0;
  const char *input_path = NULL;
  const char *output_arg = NULL;
  int no_strip = 0;
  int use_password = 0;
  int no_backup = 0;
  int mode_archive = 0;
  int mode_unpack = 0;
  int header_printed = 0;

  // Pre-scan for --no-colors so it affects help regardless of argument order
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--no-colors") == 0) {
      g_no_colors = 1;
      break;
    }
  }

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-best") == 0) {
      mode_best = 1;
    } else if (strcmp(argv[i], "-lz4") == 0) {
      codec = "lz4";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-apultra") == 0) {
      codec = "apultra";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-zx7b") == 0) {
      codec = "zx7b";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-zx0") == 0) {
      codec = "zx0";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-blz") == 0) {
      codec = "blz";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-exo") == 0) {
      codec = "exo";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-pp") == 0) {
      codec = "pp";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-snappy") == 0) {
      codec = "snappy";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-doboz") == 0) {
      codec = "doboz";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-qlz") == 0) {
      codec = "qlz";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-lzav") == 0) {
      codec = "lzav";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-lzma") == 0) {
      codec = "lzma";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-zstd") == 0) {
      codec = "zstd";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-shr") == 0) {
      codec = "shrinkler";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-stcr") == 0) {
      codec = "sc";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-lzsa") == 0 || strcmp(argv[i], "-lzsa2") == 0) {
      codec = "lzsa";
      codec_explicit = 1;
    } else if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0) {
      help_display_show_overview(g_no_colors ? GFALSE : GTRUE);
      return 0;
    } else if (strcmp(argv[i], "-help-full") == 0) {
      help_display_show_full(g_no_colors ? GFALSE : GTRUE);
      return 0;
    } else if (strcmp(argv[i], "-help-algos") == 0) {
      help_display_show_algorithms(g_no_colors ? GFALSE : GTRUE);
      return 0;
    } else if (strcmp(argv[i], "-help-faq") == 0) {
      help_display_show_faq(g_no_colors ? GFALSE : GTRUE);
      return 0;
    } else if (strcmp(argv[i], "-help-credits") == 0) {
      help_display_show_credits(g_no_colors ? GFALSE : GTRUE);
      return 0;
    } else if (strcmp(argv[i], "--no-colors") == 0) {
      g_no_colors = 1; // global monochrome
    } else if (strcmp(argv[i], "-verbose") == 0 || strcmp(argv[i], "--verbose") == 0) {
      g_verbose = 1;
    } else if (strcmp(argv[i], "--no-progress") == 0) {
      g_no_progress = 1;
    } else if (strcmp(argv[i], "--no-strip") == 0) {
      no_strip = 1;
    } else if (strcmp(argv[i], "--password") == 0) {
      use_password = 1;
    } else if (strncmp(argv[i], "--exe-filter=", 13) == 0) {
      const char *val = argv[i] + 13;
      if (strcmp(val, "auto") == 0)
        g_exe_filter = EXE_FILTER_AUTO;
      else if (strcmp(val, "kanziexe") == 0)
        g_exe_filter = EXE_FILTER_KANZIEXE;
      else if (strcmp(val, "bcj") == 0)
        g_exe_filter = EXE_FILTER_BCJ;
      else if (strcmp(val, "none") == 0)
        g_exe_filter = EXE_FILTER_NONE;
      else {
        print_help(argv[0]);
        return 1;
      }
    } else if (strcmp(argv[i], "--exe-filter") == 0) {
      if (i + 1 >= argc) {
        print_help(argv[0]);
        return 1;
      }
      const char *val = argv[++i];
      if (strcmp(val, "auto") == 0)
        g_exe_filter = EXE_FILTER_AUTO;
      else if (strcmp(val, "kanziexe") == 0)
        g_exe_filter = EXE_FILTER_KANZIEXE;
      else if (strcmp(val, "bcj") == 0)
        g_exe_filter = EXE_FILTER_BCJ;
      else if (strcmp(val, "none") == 0)
        g_exe_filter = EXE_FILTER_NONE;
      else {
        print_help(argv[0]);
        return 1;
      }
    } else if (strncmp(argv[i], "--output=", 9) == 0) {
      output_arg = argv[i] + 9;
    } else if (strncmp(argv[i], "--output", 8) == 0 && argv[i][8] != '\0') {
      output_arg = argv[i] + 8;
    } else if (strcmp(argv[i], "--output") == 0) {
      if (i + 1 >= argc) {
        print_help(argv[0]);
        return 1;
      }
      output_arg = argv[++i];
    } else if (strcmp(argv[i], "--nobackup") == 0) {
      no_backup = 1;
    } else if (strcmp(argv[i], "--archive") == 0) {
      mode_archive = 1;
    } else if (strcmp(argv[i], "--unpack") == 0) {
      mode_unpack = 1;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "[%s%s%s] Error: Unknown option: %s\n", PK_ERR, PK_SYM_ERR, PK_RES, argv[i]);
      print_help(argv[0]);
      return 1;
    } else if (!input_path) {
      input_path = argv[i];
    } else {
      fprintf(stderr,
              "[%s%s%s] Error: Multiple input files specified (or unknown option treated as file): %s\n",
              PK_ERR, PK_SYM_ERR, PK_RES, argv[i]);
      print_help(argv[0]);
      return 1;
    }
  }

  if (!input_path) {
    if (!header_printed) {
      print_main_header();
      header_printed = 1;
    }
    print_help(argv[0]);
    return 1;
  }

  if (mode_best && codec_explicit) {
    if (!header_printed) {
      print_main_header();
      header_printed = 1;
    }
    fprintf(stderr, "[%s%s%s] Error: -best cannot be combined with explicit codec selection.\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    return 1;
  }
  if (mode_best && (mode_archive || mode_unpack)) {
    if (!header_printed) {
      print_main_header();
      header_printed = 1;
    }
    fprintf(stderr, "[%s%s%s] Error: -best is only for binary packing (not archive/unpack).\n",
            PK_ERR, PK_SYM_ERR, PK_RES);
    return 1;
  }

  // Determine final output path and validate (shared logic for pack/unpack/archive)
  char final_output_path[4096];
  int should_backup = 0;

  if (output_arg) {
    struct stat st;
    if (stat(output_arg, &st) == 0 && S_ISDIR(st.st_mode)) {
      char *base_dup = strdup(input_path);
      char *base = basename(base_dup);
      snprintf(final_output_path, sizeof(final_output_path), "%s/%s",
               output_arg, base);
      free(base_dup);
    } else {
      char *dir_dup = strdup(output_arg);
      char *dname = dirname(dir_dup);
      struct stat st_dir;
      if (stat(dname, &st_dir) != 0 || !S_ISDIR(st_dir.st_mode)) {
        fprintf(stderr, "[%s%s%s] Error: Invalid output path/file specified.\n", PK_ERR, PK_SYM_ERR, PK_RES);
        free(dir_dup);
        return 1;
      }
      free(dir_dup);
      strncpy(final_output_path, output_arg, sizeof(final_output_path) - 1);
      final_output_path[sizeof(final_output_path) - 1] = '\0';
    }
  } else {
    strncpy(final_output_path, input_path, sizeof(final_output_path) - 1);
    final_output_path[sizeof(final_output_path) - 1] = '\0';
    if (!no_backup) {
      should_backup = 1;
    }
  }

  if (!header_printed) {
    print_main_header();
    header_printed = 1;
  }

  // Handle Archive/Unpack modes (not allowed with -best)
  if (mode_archive) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int arc_rc = archive_file(input_path, final_output_path, codec);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (double)(t1.tv_sec - t0.tv_sec) +
                     (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    if (g_verbose) {
      printf("[%s%s%s] Archive time: %.3f s\n", PK_INFO, PK_SYM_INFO, PK_RES,
             elapsed);
    }
    return arc_rc;
  }
  if (mode_unpack) {
    int fd_chk = open(input_path, O_RDONLY);
    if (fd_chk < 0) {
      fprintf(stderr, "[%s%s%s] Error opening input file: %s\n", PK_ERR, PK_SYM_ERR, PK_RES, strerror(errno));
      return 1;
    }
    unsigned char magic[6];
    ssize_t n = read(fd_chk, magic, sizeof(magic));
    close(fd_chk);

    if (n < 4) {
      fprintf(stderr, "[%s%s%s] Error: Input file too small\n", PK_ERR, PK_SYM_ERR, PK_RES);
      return 1;
    }

    if (magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' &&
        magic[3] == 'F') {
      return elfz_depack(input_path, final_output_path);
    } else if (n >= 4 && memcmp(magic, "zELF", 4) == 0) {
      return unpack_file(input_path, final_output_path);
    } else {
      fprintf(
          stderr,
          "[%s%s%s] Error: %s is neither a zelf binary packed, nor a zelf archive.\n",
          PK_ERR, PK_SYM_ERR, PK_RES, input_path);
      return 1;
    }
  }

  uint64_t pwd_salt = 0, pwd_obf = 0;
  if (use_password) {
    char pwbuf[128] = {0};
    fprintf(stdout, "Password: ");
    fflush(stdout);
    if (!fgets(pwbuf, sizeof(pwbuf), stdin)) {
      fprintf(stderr, "\nError: failed to read password\n");
      return 1;
    }
    size_t pwlen = strlen(pwbuf);
    while (pwlen > 0 &&
           (pwbuf[pwlen - 1] == '\n' || pwbuf[pwlen - 1] == '\r')) {
      pwbuf[--pwlen] = '\0';
    }
    int ur = open("/dev/urandom", O_RDONLY);
    if (ur >= 0) {
      if (read(ur, &pwd_salt, sizeof(pwd_salt)) != (ssize_t)sizeof(pwd_salt))
        pwd_salt = 0;
      close(ur);
    }
    if (pwd_salt == 0) {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      pwd_salt = ((uint64_t)ts.tv_sec << 32) ^ (uint64_t)ts.tv_nsec ^
                 (uint64_t)getpid();
    }
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < 8; i++) {
      unsigned char b = (unsigned char)((pwd_salt >> (8 * i)) & 0xff);
      h ^= b;
      h *= 1099511628211ull;
    }
    for (size_t i = 0; i < pwlen; i++) {
      h ^= (unsigned char)pwbuf[i];
      h *= 1099511628211ull;
    }
    uint64_t mask = (pwd_salt * 11400714819323198485ull) ^ (pwd_salt >> 13);
    pwd_obf = h ^ mask;
    VPRINTF("[%s%s%s] Password mode: salt=0x%lx\n", PK_INFO, PK_SYM_INFO, PK_RES,
            (unsigned long)pwd_salt);
  }

  if (should_backup) {
    VPRINTF("[%s%s%s] Creating backup: %s.bak\n", PK_INFO, PK_SYM_INFO, PK_RES, input_path);
    if (create_backup(input_path) != 0) {
      fprintf(stderr, "[%s%s%s] Warning: Failed to create backup. Aborting.\n", PK_WARN, PK_SYM_WARN, PK_RES);
      return 1;
    }
  }

  if (mode_best) {
    if (g_exe_filter != EXE_FILTER_AUTO) {
      fprintf(stderr,
              "[%s%s%s] Notice: --exe-filter ignored in -best (bcj/kanzi/none tried)\n",
              PK_WARN, PK_SYM_WARN, PK_RES);
    }
    g_exe_filter = EXE_FILTER_AUTO;
    struct timespec t0b, t1b;
    clock_gettime(CLOCK_MONOTONIC, &t0b);
    int rc_best = run_best_mode(input_path, final_output_path, no_strip, use_password,
                                pwd_salt, pwd_obf);
    clock_gettime(CLOCK_MONOTONIC, &t1b);
    double elapsed_best = (double)(t1b.tv_sec - t0b.tv_sec) +
                          (double)(t1b.tv_nsec - t0b.tv_nsec) / 1e9;
    if (g_verbose) {
      printf("[%s%s%s] Compression time: %.3f s\n", PK_INFO, PK_SYM_INFO, PK_RES,
             elapsed_best);
    }
    return rc_best;
  }

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  int rc_pack = pack_elf_once(input_path, final_output_path, codec, no_strip,
                              use_password, pwd_salt, pwd_obf,
                              /*print_header=*/1, /*fatal_on_error=*/1,
                              /*has_forced_filter=*/0, EXE_FILTER_AUTO,
                              /*quiet_output=*/0);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double elapsed = (double)(t1.tv_sec - t0.tv_sec) +
                   (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
  if (g_verbose && rc_pack == 0) {
    printf("[%s%s%s] Compression time: %.3f s\n", PK_INFO, PK_SYM_INFO, PK_RES,
           elapsed);
  }
  return rc_pack;
}
