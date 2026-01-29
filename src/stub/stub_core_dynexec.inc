#include <stdint.h>
#include <stddef.h>
#include "stub_vars.h"
#include "codec_select.h"
#include "codec_marker.h"
#include "bcj_x86_min.h"
#include "kanzi_exe_decode_x86_tiny.h"
#if defined(CODEC_ZSTD)
#include "zstd_minidec.h"
#endif

#include "stub_defs.h"
#include "stub_utils.h"

#define simple_hex_string(a,b) ((void)0)
#define simple_decimal_string(a,b) ((void)0)

static inline int decompress_payload(const unsigned char *src, int src_len,
                                      unsigned char *dst, int dst_cap) {
  return lz4_decompress((const char *)src, (char *)dst, src_len, dst_cap);
}

#if defined(ELFZ_STUB_PASSWORD)
__attribute__((used, section(".rodata.elfz_params")))
static const unsigned char elfz_params_block[] = {
    '+','z','E','L','F','-','P','R',
    2,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};
#else
__attribute__((used, section(".rodata.elfz_params")))
static const unsigned char elfz_params_block[] = {
    '+','z','E','L','F','-','P','R',
    1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};
#endif

#include "stub_syscalls.h"
#include "stub_elf.h"
#include "stub_reloc.h"
#include "stub_loader.h"

// Bind to tiny ASM implementations in mini_mem.S
extern void *simple_memcpy(void *dst, const void *src, size_t n);
extern void *simple_memset(void *dst, int c, size_t n);

uint64_t __attribute__((noinline))
elfz_fold_exact_setup(uint64_t entry_point, unsigned long *original_sp,
                      uint64_t ADRU, uint64_t LENU, void *elf_base,
                      void *interp_base, void *hatch_ptr, uint64_t saved_rdx) {
  (void)saved_rdx;

  Elf64_auxv_t *auxv = get_auxv_from_original_stack(original_sp);

  Elf64_Ehdr *main_ehdr = (Elf64_Ehdr *)elf_base;
  Elf64_Phdr *main_ph = (Elf64_Phdr *)((char *)elf_base + main_ehdr->e_phoff);

  uint64_t main_phdr = (uint64_t)elf_base + main_ehdr->e_phoff;
  for (int i = 0; i < main_ehdr->e_phnum; i++) {
    if (main_ph[i].p_type == PT_PHDR) {
      main_phdr = (main_ehdr->e_type == 3)
                      ? ((uint64_t)elf_base + main_ph[i].p_vaddr)
                      : main_ph[i].p_vaddr;
      break;
    }
  }

  uint64_t main_entry =
      (main_ehdr->e_type == 3) ? ((uint64_t)elf_base + main_ehdr->e_entry)
                               : main_ehdr->e_entry;

  auxv_up(auxv, AT_PHDR, main_phdr);
  auxv_up(auxv, AT_PHNUM, main_ehdr->e_phnum);
  auxv_up(auxv, AT_PHENT, main_ehdr->e_phentsize);
  auxv_up(auxv, AT_ENTRY, main_entry);
  auxv_up(auxv, AT_BASE, (uint64_t)interp_base);

  auxv_up(auxv, AT_NULL, (uint64_t)hatch_ptr);

  Elf64_auxv_t *at_null = auxv;
  while (at_null->a_type != AT_NULL)
    at_null++;

  if (entry_point >= ADRU && entry_point < ADRU + LENU)
    z_syscall_exit(99);

  uint64_t hatch_addr = (uint64_t)hatch_ptr;
  if (hatch_addr >= ADRU && hatch_addr < ADRU + LENU)
    z_syscall_exit(98);

  return (uint64_t)&at_null->a_un.a_val;
}

extern void _start(void);
extern char _end[];

#if defined(__GNUC__)
#define ELFZ_ALIGN_STACK __attribute__((force_align_arg_pointer))
#else
#define ELFZ_ALIGN_STACK
#endif
ELFZ_ALIGN_STACK void elfz_main_wrapper(unsigned long *stack_frame);
ELFZ_ALIGN_STACK int stub_dynexec_entry(unsigned long *original_sp);

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
    fd = 0;
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

  int argc = *(original_sp);
  char **argv = (char **)(original_sp + 1);
  char **envp = argv + argc + 1;
  (void)envp;

  uint64_t current_start_addr;
  __asm__ volatile("lea _start(%%rip), %0\n" : "=r"(current_start_addr));

  const char *packed_data = (const char *)0;

  {
    const unsigned char *p = (const unsigned char *)current_start_addr;
    const unsigned char *best = 0;

    const unsigned char *scan_lo = p;
    const unsigned char *scan_hi = p + (64u << 20);

#if defined(CODEC_ZSTD) || defined(CODEC_DENSITY)
    {
      uint64_t vma_lo = 0, vma_hi = 0;
      if (find_vma_for_addr((uint64_t)p, &vma_lo, &vma_hi) == 0 &&
          vma_lo < (uint64_t)p && vma_hi > (uint64_t)p) {
        scan_lo = (const unsigned char *)vma_lo;
        scan_hi = (const unsigned char *)vma_hi;

        const unsigned char *q = p;
        while (q > scan_lo) {
          --q;
          if (q + 32 < scan_hi && q[0] == COMP_MARKER[0] && q[1] == COMP_MARKER[1] &&
              q[2] == COMP_MARKER[2] && q[3] == COMP_MARKER[3] &&
              q[4] == COMP_MARKER[4] && q[5] == COMP_MARKER[5]) {
            size_t os = *(size_t *)(q + COMP_MARKER_LEN);
            int cs = *(int *)(q + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));
            const unsigned char *cand_stream =
                q + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
            const unsigned char *cand_end = cand_stream + (size_t)(uint32_t)cs;
            if ((uint64_t)os > 1000ull && (uint64_t)os < 0x20000000ull &&
                (uint64_t)(uint32_t)cs > 100ull &&
                (uint64_t)(uint32_t)cs < 0x20000000ull &&
                (uint64_t)(uint32_t)cs < ((uint64_t)os * 4ull + 1048576ull) &&
                cand_stream < scan_hi && cand_end > cand_stream &&
                cand_end <= scan_hi) {
              best = q;
              break;
            }
          }
        }

        if (!best) {
          for (const unsigned char *r = p; r + 32 < scan_hi; ++r) {
            if (r[0] == COMP_MARKER[0] && r[1] == COMP_MARKER[1] &&
                r[2] == COMP_MARKER[2] && r[3] == COMP_MARKER[3] &&
                r[4] == COMP_MARKER[4] && r[5] == COMP_MARKER[5]) {
              size_t os = *(size_t *)(r + COMP_MARKER_LEN);
              int cs = *(int *)(r + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));
              const unsigned char *cand_stream =
                  r + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
              const unsigned char *cand_end = cand_stream + (size_t)(uint32_t)cs;
              if ((uint64_t)os > 1000ull && (uint64_t)os < 0x20000000ull &&
                  (uint64_t)(uint32_t)cs > 100ull &&
                  (uint64_t)(uint32_t)cs < 0x20000000ull &&
                  (uint64_t)(uint32_t)cs < ((uint64_t)os * 4ull + 1048576ull) &&
                  cand_stream < scan_hi && cand_end > cand_stream &&
                  cand_end <= scan_hi) {
                best = r;
                break;
              }
            }
          }
        }
      }

      if (!best) {
        uint64_t next_addr = (uint64_t)p;
        int vma_scans = 0;
        while (vma_scans < 16 && !best) {
          uint64_t lo = 0, hi = 0;
          while (find_vma_for_addr(next_addr, &lo, &hi) != 0 || hi <= next_addr) {
            next_addr += 0x1000;
            uint64_t cap = (uint64_t)p + (512ULL << 20);
            if (next_addr >= cap)
              break;
          }
          if (hi <= next_addr)
            break;

          const unsigned char *search_start =
              (const unsigned char *)((next_addr == (uint64_t)p) ? (uint64_t)p : lo);
          const unsigned char *search_end = (const unsigned char *)hi;
          {
            uint64_t cap = (uint64_t)p + (512ULL << 20);
            if ((uint64_t)(uintptr_t)search_end > cap)
              search_end = (const unsigned char *)(uintptr_t)cap;
          }

          for (const unsigned char *r = search_start; r + 32 < search_end; ++r) {
            if (r[0] == COMP_MARKER[0] && r[1] == COMP_MARKER[1] &&
                r[2] == COMP_MARKER[2] && r[3] == COMP_MARKER[3] &&
                r[4] == COMP_MARKER[4] && r[5] == COMP_MARKER[5]) {
              size_t os = *(size_t *)(r + COMP_MARKER_LEN);
              int cs = *(int *)(r + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));
              const unsigned char *cand_stream =
                  r + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
              const unsigned char *cand_end = cand_stream + (size_t)(uint32_t)cs;
              if ((uint64_t)os > 1000ull && (uint64_t)os < 0x20000000ull &&
                  (uint64_t)(uint32_t)cs > 100ull &&
                  (uint64_t)(uint32_t)cs < 0x20000000ull &&
                  (uint64_t)(uint32_t)cs < ((uint64_t)os * 4ull + 1048576ull) &&
                  cand_stream < search_end && cand_end > cand_stream &&
                  cand_end <= search_end) {
                best = r;
                break;
              }
            }
          }

          next_addr = hi;
          vma_scans++;
        }
      }
    }
#endif

    if (!best) {
      for (const unsigned char *q = p; q + 32 < scan_hi; q++) {
        if (q[0] == COMP_MARKER[0] && q[1] == COMP_MARKER[1] &&
            q[2] == COMP_MARKER[2] && q[3] == COMP_MARKER[3] &&
            q[4] == COMP_MARKER[4] && q[5] == COMP_MARKER[5]) {
          best = q;
          break;
        }
      }
    }

    if (!best)
      z_syscall_exit(4);
    packed_data = (const char *)best;
  }

  size_t original_size = *(size_t *)(packed_data + COMP_MARKER_LEN);
  int compressed_size =
      *(int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));

  int has_kanzi_filter = 0;
  {
    uint64_t pb_addr;
    __asm__ volatile("lea elfz_params_block(%%rip), %0" : "=r"(pb_addr));
    const unsigned char *pb = (const unsigned char *)pb_addr;
    if (pb[0] == '+' && pb[1] == 'z' && pb[2] == 'E' && pb[3] == 'L' &&
        pb[4] == 'F' && pb[5] == '-' && pb[6] == 'P' && pb[7] == 'R') {
      uint64_t params_version = *(const uint64_t *)(pb + 8);
      has_kanzi_filter = ((params_version >> 9) & 1ULL) ? 1 : 0;
    }
  }

#if defined(CODEC_ZSTD) || defined(CODEC_DENSITY)
  int filtered_size = (int)original_size;
  const char *compressed_data = packed_data + COMP_MARKER_LEN + sizeof(size_t) +
                                sizeof(uint64_t) + sizeof(int);
#else
  int filtered_size = has_kanzi_filter
                          ? *(int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) +
                                    sizeof(uint64_t) + sizeof(int))
                          : (int)original_size;
  const char *compressed_data =
      packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
      sizeof(int) + (has_kanzi_filter ? (int)sizeof(int) : 0);
#endif

#ifdef CODEC_ZSTD
  if (original_size == 0 || original_size > (256u * 1024u * 1024u))
    z_syscall_exit(90);
  if (compressed_size <= 20 || (size_t)compressed_size >= original_size ||
      (size_t)compressed_size > (128u * 1024u * 1024u))
    z_syscall_exit(90);
#endif

  size_t alloc_size;
#if defined(CODEC_ZSTD)
  alloc_size = original_size + (64u << 20) + (128u << 10);
#elif defined(CODEC_SHRINKLER)
  alloc_size = original_size + (32u << 20);
  if (alloc_size < original_size + (32u << 20))
    alloc_size = original_size + (32u << 20);
#elif defined(CODEC_DENSITY)
  alloc_size = original_size + (original_size >> 2) + 131072;
#else
  alloc_size = original_size + (original_size >> 3) + 65536;
#endif

  void *combined_data =
      (void *)z_syscall_mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)combined_data < 0)
    z_syscall_exit(3);

  int dst_cap = (int)filtered_size;
#if defined(CODEC_SHRINKLER)
  dst_cap = (int)alloc_size;
#endif
#if defined(CODEC_DENSITY)
  dst_cap = (int)alloc_size;
#endif
#if defined(CODEC_LZHAM)
#if defined(ELFZ_FILTER_BCJ)
  dst_cap = (int)original_size;
#else
  dst_cap = (int)filtered_size;
#endif
#endif
#if defined(CODEC_PP)
#if defined(ELFZ_FILTER_BCJ)
  dst_cap = (int)original_size;
#else
  dst_cap = (int)filtered_size;
#endif
#endif

  const unsigned char *src_ptr = (const unsigned char *)compressed_data;
  int src_len = compressed_size;
  void *src_tmp_map = 0;
  size_t src_tmp_map_sz = 0;

#if defined(CODEC_ZSTD)
  {
    const size_t align = 32;
    const size_t slop = 64;
    size_t map_sz = (size_t)src_len + align + slop;
    src_tmp_map = (void *)z_syscall_mmap(NULL, map_sz, PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)src_tmp_map < 0)
      z_syscall_exit(3);
    src_tmp_map_sz = map_sz;
    uintptr_t a = ((uintptr_t)src_tmp_map + (align - 1u)) & ~(uintptr_t)(align - 1u);
    unsigned char *abuf = (unsigned char *)a;
    simple_memcpy(abuf, src_ptr, (size_t)src_len);
    simple_memset(abuf + (size_t)src_len, 0, slop);
    src_ptr = abuf;
  }
#elif defined(CODEC_DENSITY) || defined(CODEC_SHRINKLER) || defined(CODEC_BRIEFLZ) || defined(CODEC_SNAPPY)
  {
    size_t sentinel = 4096;
    size_t map_sz = (size_t)src_len + sentinel;
    src_tmp_map = (void *)z_syscall_mmap(NULL, map_sz, PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)src_tmp_map < 0)
      z_syscall_exit(3);
    src_tmp_map_sz = map_sz;
    simple_memcpy((unsigned char *)src_tmp_map, src_ptr, (size_t)src_len);
    simple_memset((unsigned char *)src_tmp_map + (size_t)src_len, 0, sentinel);
    src_ptr = (const unsigned char *)src_tmp_map;
  }
#endif

  int decomp_result =
      decompress_payload(src_ptr, src_len, (unsigned char *)combined_data, dst_cap);

  if (src_tmp_map_sz)
    z_syscall_munmap(src_tmp_map, src_tmp_map_sz);
  if (decomp_result <= 0)
    z_syscall_exit(5);

#if defined(ELFZ_FILTER_BCJ)
  {
    int bcj_applied = 1;
    {
      uint64_t pb_addr;
      __asm__ volatile("lea elfz_params_block(%%rip), %0" : "=r"(pb_addr));
      const unsigned char *pb = (const unsigned char *)pb_addr;
      if (pb[0] == '+' && pb[1] == 'z' && pb[2] == 'E' && pb[3] == 'L' &&
          pb[4] == 'F' && pb[5] == '-' && pb[6] == 'P' && pb[7] == 'R') {
        uint64_t params_version = *(const uint64_t *)(pb + 8);
        bcj_applied = ((params_version >> 8) & 1ULL) ? 1 : 0;
      }
    }
    if (bcj_applied) {
      size_t done =
          bcj_x86_decode((uint8_t *)combined_data, (size_t)decomp_result, 0);
      if (done == 0)
        z_syscall_exit(5);
    }
  }
#endif

#if !defined(ELFZ_FILTER_BCJ)
  {
    void *unfiltered = (void *)z_syscall_mmap(
        NULL, (size_t)original_size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)unfiltered < 0)
      z_syscall_exit(3);
    int processed_f = 0;
    int uf = kanzi_exe_unfilter_x86((const uint8_t *)combined_data, decomp_result,
                                    (uint8_t *)unfiltered, (int)original_size,
                                    &processed_f);
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
  if (ehdr_tmp->e_type != 2)
    z_syscall_exit(1);

  Elf64_Ehdr *elf_hdr = (Elf64_Ehdr *)combined_data;
  Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)combined_data + elf_hdr->e_phoff);

  uint64_t base_vaddr = ~0ULL;
  for (int i = 0; i < elf_hdr->e_phnum; i++) {
    if (phdr[i].p_type != PT_LOAD)
      continue;
    if (phdr[i].p_vaddr < base_vaddr)
      base_vaddr = phdr[i].p_vaddr;
  }
  if (base_vaddr == ~0ULL)
    z_syscall_exit(1);

  void *hatch = NULL;
  int has_dynamic = 0;

  for (int i = 0; i < elf_hdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_DYNAMIC)
      has_dynamic = 1;

    if (phdr[i].p_type != PT_LOAD)
      continue;

    uint64_t seg_align = phdr[i].p_align;
    if (seg_align < 0x1000)
      seg_align = 0x1000;
    if ((seg_align & (seg_align - 1)) != 0) {
      uint64_t p2 = 0x1000;
      while (p2 < seg_align)
        p2 <<= 1;
      seg_align = p2;
    }

    uint64_t page_base = phdr[i].p_vaddr & ~(seg_align - 1);
    uint64_t map_end = (phdr[i].p_vaddr + phdr[i].p_memsz + (seg_align - 1)) &
                       ~(seg_align - 1);
    uint64_t map_size = map_end - page_base;

    int prot = 0;
    if (phdr[i].p_flags & 4)
      prot |= PROT_READ;
    if (phdr[i].p_flags & 2)
      prot |= PROT_WRITE;
    if (phdr[i].p_flags & 1)
      prot |= PROT_EXEC;

    void *target_addr = (void *)(uintptr_t)page_base;
    void *mapped = (void *)z_syscall_mmap(target_addr, (size_t)map_size,
                                          prot | PROT_WRITE,
                                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                                          -1, 0);
    if ((long)mapped < 0 || mapped != target_addr)
      z_syscall_exit(6);

    uint64_t page_offset = (phdr[i].p_vaddr - page_base);
    void *dest = (char *)mapped + page_offset;

    if (phdr[i].p_filesz > 0) {
      void *src = (char *)combined_data + phdr[i].p_offset;
      simple_memcpy(dest, src, (size_t)phdr[i].p_filesz);
    }

    if (phdr[i].p_memsz > phdr[i].p_filesz) {
      size_t bss_size = (size_t)(phdr[i].p_memsz - phdr[i].p_filesz);
      void *bss_start = (char *)dest + phdr[i].p_filesz;
      simple_memset(bss_start, 0, bss_size);
    }

    if (hatch == NULL) {
      void *new_hatch = make_hatch_x86_64(&phdr[i], 0, ~0xFFFUL);
      if (new_hatch != NULL)
        hatch = new_hatch;
    }

    if (!(prot & PROT_WRITE))
      z_syscall_mprotect(mapped, (size_t)map_size, prot);
  }

  entry_point = (uint64_t)ehdr_tmp->e_entry;

  if (has_dynamic) {
    if (hatch == NULL)
      z_syscall_exit(88);

    loader_info_t loader_info =
        handle_dynamic_interpreter_complete((void *)(uintptr_t)0, original_sp, hatch);
    if (!loader_info.at_null_entry)
      z_syscall_exit(1);

    uint64_t loader_entry = loader_info.loader_entry;
    uint64_t ADRU = 0;
    uint64_t LENU = 0;

    elfz_fold_final_sequence(loader_entry, original_sp, ADRU, LENU,
                             (void *)(uintptr_t)base_vaddr, loader_info.interp_base,
                             loader_info.hatch_ptr, saved_rdx_kernel);
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

int stub_dynexec_entry(unsigned long *original_sp) {
  stub_main_common(original_sp, 0);
  return 0;
}
