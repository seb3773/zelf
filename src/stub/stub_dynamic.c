#include "codec_marker.h"
#include "codec_select.h"
#include "stub_vars.h"
#include <stddef.h>
#include <stdint.h>
// Optional executable filters (selected at build time)
#include "bcj_x86_min.h"
#include "kanzi_exe_decode_x86_tiny.h"
#ifdef CODEC_ZSTD
#include "zstd_minidec.h"
#endif

// Includes modulaires
#include "stub_defs.h"
#include "stub_elf.h"
#include "stub_loader.h"
#include "stub_reloc.h"
#include "stub_syscalls.h"
#include "stub_utils.h"

// Minimal params block for password-enabled dynamic stub (v2)
#if defined(ELFZ_STUB_PASSWORD)
__attribute__((used, section(".rodata.elfz_params"))) static const unsigned char
    elfz_params_block[] = {
        '+', 'z', 'E', 'L', 'F', '-', 'P', 'R', 2, 0, 0, 0, 0, 0, 0,
        0,                      // version = 2
        0, 0, 0, 0, 0, 0, 0, 0, // virtual_start (unused for
                                // dynamic)
        0, 0, 0, 0, 0, 0, 0, 0, // packed_data_vaddr (unused for
                                // dynamic)
        0, 0, 0, 0, 0, 0, 0, 0, // salt (to be patched)
        0, 0, 0, 0, 0, 0, 0, 0  // pwd_obfhash (to be patched)
};
#endif

// Capture the original stack pointer
static inline void *get_original_stack_pointer(void) {
  unsigned long sp;
  asm volatile("mov %%rsp, %0" : "=r"(sp));
  return (void *)sp;
}

// Check whether an address falls inside the unmapped region
static inline int is_in_unmap_region(uint64_t addr, uint64_t unmap_start,
                                     uint64_t unmap_size) {
  return (addr >= unmap_start && addr < unmap_start + unmap_size);
}

// Validate that the stack is safe
static inline int check_stack_safety(unsigned long *stack_ptr,
                                     uint64_t unmap_start,
                                     uint64_t unmap_size) {
  // Check the stack pointer itself
  if (is_in_unmap_region((uint64_t)stack_ptr, unmap_start, unmap_size)) {
    return 0;
  }
  // Check a few critical stack elements
  if (is_in_unmap_region(stack_ptr[0], unmap_start, unmap_size)) { // argc
    return 0;
  }
  return 1;
}

// Corrected verification helper
static inline int verify_entry_point_with_real_mapping(uint64_t entry_point) {
  // 1. Ensure entry point is reachable
  if (z_syscall_mprotect((void *)(entry_point & ~0xFFF), 4096,
                         PROT_READ | PROT_EXEC) != 0) {
    return 0;
  }
  // 2. Lire le code
  unsigned char *entry_code = (unsigned char *)entry_point;
  // 3. Check the prologue signature
  if (entry_code[0] == 0x55 && entry_code[1] == 0x48 && entry_code[2] == 0x89 &&
      entry_code[3] == 0xE5) {
    return 1;
  } else if (entry_code[0] == 0x48 && entry_code[1] == 0x83 &&
             entry_code[2] == 0xEC) {
    return 1;
  } else if (entry_code[0] == 0x31 && entry_code[1] == 0xED) {
    return 1;
  } else {
    return 1; // Au moins c'est accessible
  }
}

// Called from ASM to prepare the final sequence.
// Sets up AUXV and returns the address of the AT_NULL value field.
// Not static because it is referenced from assembly.
uint64_t __attribute__((noinline))
elfz_fold_exact_setup(uint64_t entry_point, unsigned long *original_sp,
                      uint64_t ADRU, uint64_t LENU, void *elf_base,
                      void *interp_base, void *hatch_ptr, uint64_t saved_rdx) {

  // Use the explicit parameters
  void *hatch = hatch_ptr;
  (void)saved_rdx;   // unused here
  (void)entry_point; // unused here
  (void)ADRU;        // unused here
  (void)LENU;        // unused here

  // 1. Trouver AUXV dans la stack originale
  unsigned long *auxv_ptr = original_sp;
  auxv_ptr++; // Sauter argc
  while (*auxv_ptr != 0)
    auxv_ptr++; // Sauter argv
  auxv_ptr++;   // Sauter NULL
  while (*auxv_ptr != 0)
    auxv_ptr++; // Sauter envp
  auxv_ptr++;   // Sauter NULL

  Elf64_auxv_t *auxv = (Elf64_auxv_t *)auxv_ptr;

  // 2. Configurer AUXV
  Elf64_Ehdr *main_ehdr = (Elf64_Ehdr *)elf_base;
  Elf64_Phdr *main_ph = (Elf64_Phdr *)((char *)elf_base + main_ehdr->e_phoff);
  uint64_t main_phdr = (uint64_t)elf_base + main_ehdr->e_phoff;
  // Prefer PT_PHDR's p_vaddr when present (more accurate for some large
  // binaries)
  for (int i = 0; i < main_ehdr->e_phnum; i++) {
    if (main_ph[i].p_type == PT_PHDR) {
      main_phdr = (uint64_t)elf_base + main_ph[i].p_vaddr;
      break;
    }
  }
  uint64_t main_entry = (uint64_t)elf_base + main_ehdr->e_entry;

  auxv_up(auxv, AT_PHDR, main_phdr);
  auxv_up(auxv, AT_PHNUM, main_ehdr->e_phnum);
  auxv_up(auxv, AT_PHENT, main_ehdr->e_phentsize);
  auxv_up(auxv, AT_ENTRY, main_entry);
  auxv_up(auxv, AT_BASE, (uint64_t)interp_base);

  // 3. Stocker hatch dans AT_NULL (utiliser l'adresse CODE du hatch)
  void *hatch_code = hatch;
  if (hatch_code) {
    volatile uint8_t *hp = (volatile uint8_t *)hatch_code;
    uint32_t sig = ((uint32_t)hp[0]) | (((uint32_t)hp[1]) << 8) |
                   (((uint32_t)hp[2]) << 16) | (((uint32_t)hp[3]) << 24);
    if (sig != 0xC35A050F) {
      uint64_t maybe = *(volatile uint64_t *)hatch_code;
      volatile uint8_t *hp2 = (volatile uint8_t *)maybe;
      uint32_t sig2 = ((uint32_t)hp2[0]) | (((uint32_t)hp2[1]) << 8) |
                      (((uint32_t)hp2[2]) << 16) | (((uint32_t)hp2[3]) << 24);
      if (sig2 == 0xC35A050F) {
        hatch_code = (void *)maybe;
      }
    }
  }
  auxv_up(auxv, AT_NULL, (uint64_t)hatch_code);

  // 4. Find the ADDRESS of the AT_NULL entry (not its value)
  Elf64_auxv_t *at_null = auxv;
  while (at_null->a_type != AT_NULL)
    at_null++;

  // RETURN: address of the AT_NULL entry
  return (uint64_t)&at_null->a_un.a_val;
}

extern void _start(void); // Defined in start.S
extern char _end[];       // Defined by the linker

#if defined(__GNUC__)
#define ELFZ_ALIGN_STACK __attribute__((force_align_arg_pointer))
#else
#define ELFZ_ALIGN_STACK
#endif
ELFZ_ALIGN_STACK void elfz_main_wrapper(unsigned long *stack_frame);
ELFZ_ALIGN_STACK int stub_dynamic_entry(unsigned long *original_sp);

// Shared implementation
static void stub_main_common(unsigned long *original_sp,
                             uint64_t saved_rdx_kernel) {
#if defined(ELFZ_STUB_PASSWORD)
  const unsigned char *pb;
  {
    uint64_t pb_addr;
    __asm__ volatile("lea elfz_params_block(%%rip), %0" : "=r"(pb_addr));
    pb = (const unsigned char *)pb_addr;
  }
  if (!(pb[0] == '+' && pb[1] == 'z' && pb[2] == 'E' && pb[3] == 'L' &&
        pb[4] == 'F' && pb[5] == '-' && pb[6] == 'P' && pb[7] == 'R')) {
    z_syscall_exit(12);
  }
  uint64_t salt = *(const uint64_t *)(pb + 32);
  uint64_t obf = *(const uint64_t *)(pb + 40);
  uint64_t mask = (salt * 11400714819323198485ull) ^ (salt >> 13);
  uint64_t expected = obf ^ mask;
  static const char pw_prompt[] = "Password:";
  static const char pw_wrong[] = "Wrong password\n";
  (void)z_syscall_write(1, pw_prompt, (int)sizeof(pw_prompt) - 1);
  int fd = (int)z_syscall_open("/dev/tty", 0, 0);
  if (fd < 0)
    fd = 0; // fallback to stdin
  // Disable echo if tty supports it
  int echo_changed = 0;
  struct z_termios {
    unsigned int c_iflag, c_oflag, c_cflag, c_lflag;
    unsigned char c_line;
    unsigned char c_cc[32];
  } tio_old, tio_new;
  if (z_syscall_ioctl(fd, TCGETS, &tio_old) == 0) {
    tio_new = tio_old;
    tio_new.c_lflag &= ~Z_ECHO;
    if (z_syscall_ioctl(fd, TCSETS, &tio_new) == 0) {
      echo_changed = 1;
    }
  }
  char pwbuf[64];
  long n = z_syscall_read(fd, pwbuf, (int)sizeof(pwbuf));
  // Restore echo and move to next line if we disabled it
  if (echo_changed) {
    (void)z_syscall_ioctl(fd, TCSETS, &tio_old);
    (void)z_syscall_write(1, "\n", 1);
  }
  if (fd > 0)
    z_syscall_close(fd);
  if (n <= 0)
    z_syscall_exit(1);
  int len = (int)n;
  while (len > 0 && (pwbuf[len - 1] == '\n' || pwbuf[len - 1] == '\r'))
    len--;
  uint64_t h = 1469598103934665603ull;
  for (int i = 0; i < 8; i++) {
    unsigned char b = (unsigned char)((salt >> (8 * i)) & 0xff);
    h ^= b;
    h *= 1099511628211ull;
  }
  for (int i = 0; i < len; i++) {
    h ^= (unsigned char)pwbuf[i];
    h *= 1099511628211ull;
  }
  if (h != expected) {
    (void)z_syscall_write(1, pw_wrong, (int)sizeof(pw_wrong) - 1);
    z_syscall_exit(1);
  }
#endif

  uint64_t entry_point = 0;
  void *coherent_base = NULL;
  uint64_t elf_header_offset = 0;

  int argc = *(original_sp);
  char **argv = (char **)(original_sp + 1);
  char **envp = argv + argc + 1;

  uint64_t current_start_addr;
  __asm__ volatile("lea _start(%%rip), %0\n" : "=r"(current_start_addr));
  uint64_t virtual_start = 0;
  uint64_t aslr_offset = current_start_addr - virtual_start;

  const char *packed_data = NULL;
  if (virtual_start < 0x10000) {
    uint64_t next_addr = current_start_addr;
    int vma_scans = 0;
    const unsigned char *best_p = NULL;
    while (vma_scans < 16 && best_p == NULL) {
      uint64_t vma_lo = 0, vma_hi = 0;
      while (find_vma_for_addr(next_addr, &vma_lo, &vma_hi) != 0 ||
             vma_hi <= next_addr) {
        next_addr += 0x1000;
        uint64_t cap = current_start_addr + (512ULL << 20);
        if (next_addr >= cap)
          break;
      }
      if (vma_hi <= next_addr)
        break;

      unsigned char *search_start =
          (unsigned char *)((next_addr == current_start_addr)
                                ? current_start_addr
                                : vma_lo);
      unsigned char *search_end = (unsigned char *)vma_hi;
      uint64_t cap = current_start_addr + (512ULL << 20);
      if ((uint64_t)(uintptr_t)search_end > cap)
        search_end = (unsigned char *)(uintptr_t)cap;

      for (unsigned char *p = search_start; p < search_end - 32; p++) {
        if (p[0] == COMP_MARKER[0] && p[1] == COMP_MARKER[1] &&
            p[2] == COMP_MARKER[2] && p[3] == COMP_MARKER[3] &&
            p[4] == COMP_MARKER[4] && p[5] == COMP_MARKER[5]) {
          uint64_t actual_size = *(uint64_t *)(p + COMP_MARKER_LEN);
          // uint64_t entry_off = *(uint64_t *)(p + COMP_MARKER_LEN +
          // sizeof(size_t));
          uint32_t comp_size = *(uint32_t *)(p + COMP_MARKER_LEN +
                                             sizeof(size_t) + sizeof(uint64_t));

          if (actual_size > 1000 && actual_size < 0x20000000 &&
              comp_size > 100 && comp_size < 0x20000000 &&
              (uint64_t)comp_size <
                  ((uint64_t)actual_size * 4ull + 1048576ull)) {
            unsigned char *cand_stream = p + COMP_MARKER_LEN + sizeof(size_t) +
                                         sizeof(uint64_t) + sizeof(uint32_t) +
                                         sizeof(uint32_t);
            if (cand_stream >= search_end)
              continue;

#ifdef CODEC_SHRINKLER
            if (cand_stream + 16 > search_end)
              continue;
            if (!(cand_stream[0] == 'S' && cand_stream[1] == 'h' &&
                  cand_stream[2] == 'r' && cand_stream[3] == 'i'))
              continue;
            unsigned hdr_rem =
                ((unsigned)cand_stream[6] << 8) | (unsigned)cand_stream[7];
            unsigned hdr_sz = 8u + hdr_rem;
            if (hdr_sz < 24u || cand_stream + hdr_sz > search_end)
              continue;
            unsigned payload_sz = ((unsigned)cand_stream[8] << 24) |
                                  ((unsigned)cand_stream[9] << 16) |
                                  ((unsigned)cand_stream[10] << 8) |
                                  (unsigned)cand_stream[11];
            if (payload_sz == 0 || (unsigned)comp_size < hdr_sz + payload_sz)
              continue;
#endif

#ifdef CODEC_LZAV
            if (((cand_stream[0] >> 4) & 0xF) != 2)
              continue;
#endif

#ifdef CODEC_ZSTD
            {
              long long fcs_cand = zstd_peek_content_size(
                  (const unsigned char *)cand_stream, (size_t)comp_size);
              size_t cap_relax =
                  (size_t)actual_size + ((size_t)actual_size >> 3) + 65536u;
              if (fcs_cand > 0 && (size_t)fcs_cand > cap_relax)
                continue;
            }
#endif

#ifdef CODEC_SNAPPY
            const unsigned char *cptr =
                (const unsigned char *)(p + COMP_MARKER_LEN + sizeof(size_t) +
                                        sizeof(uint64_t) + sizeof(uint32_t) +
                                        sizeof(uint32_t));
            size_t ulen_hdr =
                snappy_get_uncompressed_length(cptr, (size_t)comp_size);
            size_t cap_relax =
                (size_t)actual_size + ((size_t)actual_size >> 3) + 65536u;
            if (ulen_hdr == 0 || ulen_hdr > cap_relax)
              continue;
#endif

#ifdef CODEC_LZMA
            if ((size_t)comp_size >= 5) {
              const unsigned char *props = (const unsigned char *)cand_stream;
              unsigned p0 = props[0];
              unsigned lc = p0 % 9;
              p0 /= 9;
              unsigned lp = p0 % 5;
              unsigned pb = p0 / 5;
              unsigned dict = (unsigned)props[1] | ((unsigned)props[2] << 8) |
                              ((unsigned)props[3] << 16) |
                              ((unsigned)props[4] << 24);
              if (!(lc <= 8 && lp <= 4 && pb <= 4))
                continue;
              if (dict < 4096 || dict > (256u << 20))
                continue;
            } else {
              continue;
            }
#endif

            best_p = p;
          }
        }
      }
      if (best_p)
        break;
      next_addr = vma_hi + 1;
      vma_scans++;
    }
    if (best_p) {
      packed_data = (const char *)best_p;
    } else {
      z_syscall_exit(4);
    }
  }

  size_t original_size = *(size_t *)(packed_data + COMP_MARKER_LEN);
  // uint64_t entry_offset_val = *(uint64_t *)(packed_data + COMP_MARKER_LEN +
  // sizeof(size_t));
  int compressed_size = *(int *)(packed_data + COMP_MARKER_LEN +
                                 sizeof(size_t) + sizeof(uint64_t));

#ifdef CODEC_ZSTD
  int filtered_size = 0;
  const char *compressed_data = packed_data + COMP_MARKER_LEN + sizeof(size_t) +
                                sizeof(uint64_t) + sizeof(int);
#else
  int filtered_size = *(int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) +
                               sizeof(uint64_t) + sizeof(int));
  const char *compressed_data = packed_data + COMP_MARKER_LEN + sizeof(size_t) +
                                sizeof(uint64_t) + sizeof(int) + sizeof(int);
#endif

#ifdef CODEC_ZSTD
  if (original_size == 0 || original_size > (256u * 1024u * 1024u))
    z_syscall_exit(90);
  if (compressed_size <= 20 || (size_t)compressed_size >= original_size ||
      (size_t)compressed_size > (128u * 1024u * 1024u))
    z_syscall_exit(90);
  const unsigned char *m = (const unsigned char *)compressed_data;
  if (!(m[0] == 0x28 && m[1] == 0xB5 && m[2] == 0x2F && m[3] == 0xFD))
    z_syscall_exit(4);
  long long cs = zstd_peek_content_size((const unsigned char *)compressed_data,
                                        (size_t)compressed_size);
  size_t cap_relax = (size_t)original_size + (64u << 20) + (128u << 10);
  if (cs > 0 && (size_t)cs > cap_relax)
    z_syscall_exit(4);
#endif

#ifdef CODEC_ZSTD
  size_t alloc_size = original_size + (64u << 20) + (128u << 10);
#elif defined(CODEC_SHRINKLER)
  size_t alloc_size = original_size + (32u << 20);
#else
  size_t alloc_size = original_size + (original_size >> 3) + 65536;
#endif
#ifdef CODEC_SHRINKLER
  if (alloc_size < original_size + (32u << 20))
    alloc_size = original_size + (32u << 20);
#endif
  void *combined_data =
      (void *)z_syscall_mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)combined_data < 0)
    z_syscall_exit(3);

  const char *src_ptr = compressed_data;
  int src_len = compressed_size;
  int decomp_result = -1;

#if defined(CODEC_ZSTD)
  const size_t align = 32;
  const size_t slop = 64;
  size_t src_buf_sz = (size_t)src_len + align + slop;
  void *src_map =
      (void *)z_syscall_mmap(NULL, src_buf_sz, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)src_map < 0)
    z_syscall_exit(3);
  char *src_aligned = (char *)(((uint64_t)(uintptr_t)src_map + (align - 1)) &
                               ~(uint64_t)(align - 1));
  simple_memcpy(src_aligned, src_ptr, (int)src_len);
  decomp_result =
      zstd_decompress_c((const unsigned char *)src_aligned, (size_t)src_len,
                        (unsigned char *)combined_data, (size_t)alloc_size);
  (void)z_syscall_munmap(src_map, src_buf_sz);
#else
// Generic decompression path
// For Shrinkler, BriefLZ, Snappy: use padding
#if defined(CODEC_SHRINKLER) || defined(CODEC_BRIEFLZ) || defined(CODEC_SNAPPY)
  {
    size_t sentinel = 4096;
    void *cbuf_ptr = (void *)z_syscall_mmap(NULL, (size_t)src_len + sentinel,
                                            PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)cbuf_ptr < 0)
      z_syscall_exit(3);
    char *cbuf = (char *)cbuf_ptr;
    for (int i = 0; i < src_len; i++)
      cbuf[i] = src_ptr[i];
    for (size_t i = 0; i < sentinel; i++)
      cbuf[src_len + i] = 0;
    int dst_cap = (int)alloc_size;
#ifdef CODEC_BRIEFLZ
// dst_cap = (int)filtered_size; // BriefLZ uses filtered size
#endif
    decomp_result =
        lz4_decompress(cbuf, (char *)combined_data, src_len, dst_cap);
    (void)z_syscall_munmap(cbuf_ptr, (size_t)src_len + sentinel);
  }
#else
  {
    int dst_cap = (int)alloc_size;
#if defined(CODEC_LZMA) || defined(CODEC_BRIEFLZ)
    dst_cap = (int)filtered_size;
#endif
#if defined(CODEC_PP)
    /* PowerPacker needs exact decompressed size for >16MB file support */
#if defined(ELFZ_FILTER_BCJ)
    dst_cap = (int)original_size;
#else
    dst_cap = (int)filtered_size;
#endif
#endif
    decomp_result =
        lz4_decompress(src_ptr, (char *)combined_data, src_len, dst_cap);
  }
#endif
#endif

  if (decomp_result <= 0)
    z_syscall_exit(5);

#if defined(ELFZ_FILTER_BCJ)
  {
    size_t done =
        bcj_x86_decode((uint8_t *)combined_data, (size_t)decomp_result, 0);
    if (done == 0)
      z_syscall_exit(5);
  }
#else
  {
    void *unfiltered = (void *)z_syscall_mmap(
        NULL, (size_t)original_size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)unfiltered < 0)
      z_syscall_exit(3);
    int processed_f = 0;
    int uf = kanzi_exe_unfilter_x86((const uint8_t *)combined_data,
                                    decomp_result, (uint8_t *)unfiltered,
                                    (int)original_size, &processed_f);
    if (uf != (int)original_size || processed_f != decomp_result)
      z_syscall_exit(5);
    (void)z_syscall_munmap(combined_data, alloc_size);
    combined_data = unfiltered;
  }
#endif

  char *elf_data = (char *)combined_data;
  if (elf_data[0] != 0x7f || elf_data[1] != 'E' || elf_data[2] != 'L' ||
      elf_data[3] != 'F')
    z_syscall_exit(6);

  Elf64_Ehdr *ehdr_tmp = (Elf64_Ehdr *)combined_data;
  int is_pie = (ehdr_tmp->e_type == 3);

  uint64_t total_size = 0;
  uint64_t max_align = 0x1000;
  Elf64_Ehdr *elf_hdr = (Elf64_Ehdr *)combined_data;
  Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)combined_data + elf_hdr->e_phoff);
  for (int i = 0; i < elf_hdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
      uint64_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
      if (seg_end > total_size)
        total_size = seg_end;
      if (phdr[i].p_align > max_align)
        max_align = phdr[i].p_align;
    }
  }

  total_size = (total_size + 0xFFF) & ~0xFFF;
  if (max_align < 0x1000)
    max_align = 0x1000;
  if ((max_align & (max_align - 1)) != 0) {
    uint64_t p = 0x1000;
    while (p < max_align)
      p <<= 1;
    max_align = p;
  }

  uint64_t reserve_len = total_size + max_align;
  void *reserve = (void *)z_syscall_mmap(NULL, reserve_len, PROT_NONE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)reserve < 0)
    z_syscall_exit(6);

  uint64_t base_raw = (uint64_t)reserve;
  uint64_t aligned_addr = (base_raw + (max_align - 1)) & ~(max_align - 1);
  coherent_base = (void *)aligned_addr;
  uint64_t reserve_end = base_raw + reserve_len;
  size_t head = (size_t)(aligned_addr - base_raw);
  if (head)
    z_syscall_munmap((void *)base_raw, head);
  size_t tail = (size_t)(reserve_end - (aligned_addr + total_size));
  if (tail)
    z_syscall_munmap((void *)(aligned_addr + total_size), tail);

  Elf64_Ehdr *elf_for_loop = (Elf64_Ehdr *)combined_data;
  Elf64_Phdr *phdr_for_loop =
      (Elf64_Phdr *)((char *)combined_data + elf_for_loop->e_phoff);
  int loop_count = elf_for_loop->e_phnum;
  void *hatch = NULL;

  for (int i = 0; i < loop_count; i++) {
    if (phdr_for_loop[i].p_type != PT_LOAD)
      continue;

    uint64_t seg_align = phdr_for_loop[i].p_align;
    if (seg_align < 0x1000)
      seg_align = 0x1000;
    if ((seg_align & (seg_align - 1)) != 0) {
      uint64_t p = 0x1000;
      while (p < seg_align)
        p <<= 1;
      seg_align = p;
    }
    uint64_t page_base = phdr_for_loop[i].p_vaddr & ~(seg_align - 1);
    uint64_t map_size = ((phdr_for_loop[i].p_vaddr + phdr_for_loop[i].p_memsz +
                          (seg_align - 1)) &
                         ~(seg_align - 1)) -
                        page_base;
    int prot = 0;
    if (phdr_for_loop[i].p_flags & 4)
      prot |= PROT_READ;
    if (phdr_for_loop[i].p_flags & 2)
      prot |= PROT_WRITE;
    if (phdr_for_loop[i].p_flags & 1)
      prot |= PROT_EXEC;

    uint64_t filesz = phdr_for_loop[i].p_filesz;
    uint64_t memsz = phdr_for_loop[i].p_memsz;

    void *target_addr;
    if (is_pie)
      target_addr = (char *)coherent_base + page_base;
    else
      z_syscall_exit(1);

    void *mapped =
        (void *)z_syscall_mmap(target_addr, map_size, prot | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if ((long)mapped < 0 || mapped != target_addr)
      z_syscall_exit(6);

    uint64_t page_offset = (phdr_for_loop[i].p_vaddr - page_base);
    void *dest = (char *)mapped + page_offset;

    if (filesz > 0) {
      uint64_t file_offset = phdr_for_loop[i].p_offset;
      void *src = (char *)combined_data + file_offset;
      simple_memcpy(dest, src, filesz);
    }

    if (memsz > filesz) {
      size_t bss_size = memsz - filesz;
      void *bss_start = (char *)dest + filesz;
      simple_memset(bss_start, 0, bss_size);
    }

    if (hatch == NULL) {
      uint64_t reloc = is_pie ? (uint64_t)coherent_base : 0;
      void *new_hatch = make_hatch_x86_64(&phdr_for_loop[i], reloc, ~0xFFFUL);
      if (new_hatch != NULL)
        hatch = new_hatch;
    }

    if (!(prot & PROT_WRITE)) {
      z_syscall_mprotect(mapped, map_size, prot);
    }
  }

  if (is_pie)
    elf_header_offset = 0;
  else
    z_syscall_exit(1);

  unsigned char *elf_header_location =
      (unsigned char *)coherent_base + elf_header_offset;
  if (!coherent_base)
    z_syscall_exit(1);

  Elf64_Ehdr *ehdr_final;
  Elf64_Phdr *phdr_final;
  if (is_pie) {
    ehdr_final = (Elf64_Ehdr *)((char *)coherent_base + elf_header_offset);
    phdr_final = (Elf64_Phdr *)((char *)coherent_base + elf_header_offset +
                                ehdr_final->e_phoff);
  } else {
    z_syscall_exit(1);
  }

  entry_point = ((uint64_t)coherent_base + ehdr_final->e_entry);

  int has_dynamic = 0;
  for (int i = 0; i < ehdr_final->e_phnum; i++) {
    if (phdr_final[i].p_type == PT_DYNAMIC) {
      has_dynamic = 1;
      break;
    }
  }

  if (has_dynamic) {
    if (hatch == NULL)
      z_syscall_exit(88);
    loader_info_t loader_info =
        handle_dynamic_interpreter_complete(coherent_base, original_sp, hatch);
    if (!loader_info.at_null_entry)
      z_syscall_exit(1);

    uint64_t loader_entry = loader_info.loader_entry;
    uint64_t ADRU = 0;
    uint64_t LENU = 0;

    elfz_fold_final_sequence(loader_entry, original_sp, ADRU, LENU,
                             coherent_base, loader_info.interp_base,
                             loader_info.hatch_ptr, saved_rdx_kernel);
  }

  unsigned long *sp2 = original_sp;
  sp2++;
  sp2 += argc + 1;
  int envc = 0;
  while (envp[envc])
    envc++;
  sp2 += envc + 1;
  unsigned long *auxv = sp2;

  int j = 0;
  while (auxv[j * 2] != 0) {
    unsigned long tag = auxv[j * 2];
    if (tag == AT_PHNUM)
      auxv[j * 2 + 1] = ((Elf64_Ehdr *)coherent_base)->e_phnum;
    else if (tag == AT_PHENT)
      auxv[j * 2 + 1] = ((Elf64_Ehdr *)coherent_base)->e_phentsize;
    else if (tag == AT_BASE)
      auxv[j * 2 + 1] = 0;
    j++;
  }

  unsigned long *argc_ptr = original_sp;

  __asm__ volatile("mov %0, %%rsp\n"
                   "xor %%rdx, %%rdx\n"
                   "xor %%rbx, %%rbx\n"
                   "xor %%rcx, %%rcx\n"
                   "xor %%rbp, %%rbp\n"
                   "xor %%r8, %%r8\n"
                   "xor %%r9, %%r9\n"
                   "xor %%r10, %%r10\n"
                   "xor %%r11, %%r11\n"
                   "xor %%r12, %%r12\n"
                   "xor %%r13, %%r13\n"
                   "xor %%r15, %%r15\n"
                   "jmp *%1\n"
                   :
                   : "r"(argc_ptr), "r"(entry_point)
                   : "memory");
  z_syscall_exit(1);
}

ELFZ_ALIGN_STACK void elfz_main_wrapper(unsigned long *stack_frame) {
  unsigned long *original_sp = (unsigned long *)stack_frame[2];
  uint64_t saved_rdx = stack_frame[4];
  stub_main_common(original_sp, saved_rdx);
}

int stub_dynamic_entry(unsigned long *original_sp) {
  stub_main_common(original_sp, 0);
  return 0;
}
