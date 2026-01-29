#include "codec_marker.h"
#include "codec_select.h"
#include <stddef.h>
#include <stdint.h>

#include "stub_defs.h"
#include "stub_elf.h"
#include "stub_loader.h"
#include "stub_reloc.h"
#include "stub_syscalls.h"
#include "stub_utils.h"

#if defined(ELFZ_FILTER_BCJ)
#include "bcj_arm64_min_legacy.h"
#endif

#if defined(ELFZ_FILTER_KANZIEXE)
#include "kanzi_exe_decode_arm64_tiny.h"
#endif

#if defined(ELFZ_STUB_PASSWORD)
__attribute__((used, section(".rodata.elfz_params"))) static const unsigned char
    elfz_params_block[] = {
        '+', 'z', 'E', 'L', 'F', '-', 'P', 'R', 2, 0, 0, 0, 0, 0, 0,
        0,                      // version
        0, 0, 0, 0, 0, 0, 0, 0, // virtual_start
        0, 0, 0, 0, 0, 0, 0, 0, // packed_data_vaddr
        0, 0, 0, 0, 0, 0, 0, 0, // salt
        0, 0, 0, 0, 0, 0, 0, 0  // pwd_obfhash
};
#else
__attribute__((used, section(".rodata.elfz_params"))) static const unsigned char
    elfz_params_block[] = {
        '+', 'z', 'E', 'L', 'F', '-', 'P', 'R', 1, 0, 0, 0, 0, 0, 0,
        0,                      // version
        0, 0, 0, 0, 0, 0, 0, 0, // virtual_start
        0, 0, 0, 0, 0, 0, 0, 0, // packed_data_vaddr
        0, 0, 0, 0, 0, 0, 0, 0  // reserved
};
#endif

extern void _start(void);

static inline uint64_t sym_addr_elfz_params(void) {
  uint64_t v;
  __asm__ volatile("adrp %0, elfz_params_block\n"
                   "add  %0, %0, :lo12:elfz_params_block\n"
                   : "=r"(v));
  return v;
}

static inline uint64_t sym_addr_start(void) {
  uint64_t v;
  __asm__ volatile("adrp %0, _start\n"
                   "add  %0, %0, :lo12:_start\n"
                   : "=r"(v));
  return v;
}

static inline void password_gate(const unsigned char *pb) {
#if defined(ELFZ_STUB_PASSWORD)
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
#else
  (void)pb;
#endif
}

uint64_t __attribute__((noinline))
elfz_fold_exact_setup(uint64_t entry_point, unsigned long *original_sp,
                      uint64_t ADRU, uint64_t LENU, void *elf_base,
                      void *interp_base, void *hatch_ptr, uint64_t saved_x0) {
  (void)saved_x0;

  Elf64_auxv_t *auxv = get_auxv_from_original_stack(original_sp);

  Elf64_Ehdr *main_ehdr = (Elf64_Ehdr *)elf_base;
  Elf64_Phdr *main_ph = (Elf64_Phdr *)((char *)elf_base + main_ehdr->e_phoff);

  uint64_t main_phdr = (uint64_t)elf_base + main_ehdr->e_phoff;
  for (int i = 0; i < main_ehdr->e_phnum; i++) {
    if (main_ph[i].p_type == PT_PHDR) {
      main_phdr = (main_ehdr->e_type == 3) ? ((uint64_t)elf_base + main_ph[i].p_vaddr)
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

  if (LENU) {
    if (entry_point >= ADRU && entry_point < ADRU + LENU)
      z_syscall_exit(99);
    uint64_t hatch_addr = (uint64_t)hatch_ptr;
    if (hatch_addr >= ADRU && hatch_addr < ADRU + LENU)
      z_syscall_exit(98);
  }

  return (uint64_t)&at_null->a_un.a_val;
}

static inline const unsigned char *find_packed_data(uint64_t start_addr) {
  const unsigned char *p = (const unsigned char *)start_addr;
  const unsigned char *scan_lo = p;
  const unsigned char *scan_hi = p + 0x28000u;
  {
    uint64_t vma_lo = 0, vma_hi = 0;
    if (find_vma_for_addr((uint64_t)p, &vma_lo, &vma_hi) == 0 && vma_hi > vma_lo) {
      scan_lo = (const unsigned char *)(uintptr_t)vma_lo;
      scan_hi = (const unsigned char *)(uintptr_t)vma_hi;
#if defined(CODEC_ZSTD) || defined(CODEC_DENSITY)
      {
        uint64_t prev_lo = 0, prev_hi = 0;
        if (find_vma_ending_at(vma_lo, &prev_lo, &prev_hi) == 0 &&
            prev_hi == vma_lo && prev_lo < vma_lo) {
          scan_lo = (const unsigned char *)(uintptr_t)prev_lo;
        }
      }
#endif
      uint64_t cap = (uint64_t)p + (64u << 20);
      if ((uint64_t)(uintptr_t)scan_hi > cap)
        scan_hi = (const unsigned char *)(uintptr_t)cap;
    }
  }

#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
  {
    const unsigned char *q = p;
    while (q > scan_lo) {
      --q;
      if (q + 32 >= scan_hi)
        continue;
      if (q[0] != COMP_MARKER[0] || q[1] != COMP_MARKER[1] || q[2] != COMP_MARKER[2] ||
          q[3] != COMP_MARKER[3] || q[4] != COMP_MARKER[4] || q[5] != COMP_MARKER[5])
        continue;

      const unsigned char *hdr = q + COMP_MARKER_LEN;
      size_t os = *(const size_t *)(hdr);
      int cs = *(const int *)(hdr + sizeof(size_t) + sizeof(uint64_t));
      int fs = *(const int *)(hdr + sizeof(size_t) + sizeof(uint64_t) + sizeof(int));

      if ((uint64_t)os <= 1000ull || os > 0x20000000u)
        continue;
      if (cs <= 100 || cs > 0x20000000)
        continue;
      if (fs <= 0 || fs > 0x20000000)
        continue;

      {
        uint64_t ufs = (uint64_t)(uint32_t)fs;
        uint64_t uos = (uint64_t)os;
        if (ufs < uos)
          continue;
        if ((ufs - uos) > 256u)
          continue;
      }

      {
        uint64_t max_os = (uint64_t)os;
        if ((uint64_t)(uint32_t)fs > max_os)
          max_os = (uint64_t)(uint32_t)fs;
        if ((uint64_t)(uint32_t)cs > (max_os * 4ull + 1048576ull))
          continue;
      }

      size_t hdr_len = COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
                       sizeof(int) + sizeof(int);
      {
        uint32_t tag = 0;
        const unsigned char *tagp = q + hdr_len;
        if ((size_t)(scan_hi - tagp) >= 4u) {
          simple_memcpy(&tag, tagp, sizeof(tag));
          if (tag == 0x21303454u)
            hdr_len += (sizeof(uint32_t) + 40u);
        }
      }

      if ((size_t)(scan_hi - q) < hdr_len + (size_t)(uint32_t)cs)
        continue;

      const unsigned char *cand_end = q + hdr_len + (size_t)(uint32_t)cs;
      if (cand_end > scan_hi)
        continue;
      if (cand_end > p)
        continue;

      if ((size_t)(p - cand_end) < 16384u)
        continue;

      return q;
    }
  }
#endif

  const unsigned char *best = NULL;
  size_t best_tail = (size_t)-1;
#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
  size_t best_gap = (size_t)-1;
#endif

  for (const unsigned char *q = scan_lo; q + 32 < scan_hi; q++) {
    if (q[0] == COMP_MARKER[0] && q[1] == COMP_MARKER[1] && q[2] == COMP_MARKER[2] &&
        q[3] == COMP_MARKER[3] && q[4] == COMP_MARKER[4] && q[5] == COMP_MARKER[5]) {
#if defined(CODEC_ZSTD)
      if (q >= p)
        continue;
#elif defined(CODEC_DENSITY)
      if (q >= p)
        continue;
#endif
      const unsigned char *hdr = q + COMP_MARKER_LEN;
      size_t os = *(const size_t *)(hdr);
      int cs = *(const int *)(hdr + sizeof(size_t) + sizeof(uint64_t));

      if (os == 0 || os > 0x20000000u)
        continue;
      if (cs <= 0 || cs > 0x20000000)
        continue;

      {
        uint64_t max_os = (uint64_t)os;
#if defined(ELFZ_FILTER_KANZIEXE)
        int fs = *(const int *)(hdr + sizeof(size_t) + sizeof(uint64_t) + sizeof(int));
        if (fs <= 0 || fs > 0x20000000)
          continue;
#if defined(CODEC_DENSITY)
        {
          uint64_t ufs = (uint64_t)(uint32_t)fs;
          uint64_t uos = (uint64_t)os;
          if (uos <= 1000ull)
            continue;
          if ((uint64_t)(uint32_t)cs <= 100ull)
            continue;
          if (ufs < uos)
            continue;
          if ((ufs - uos) > 256u)
            continue;
        }
#endif
        if ((uint64_t)(uint32_t)fs > max_os)
          max_os = (uint64_t)(uint32_t)fs;
#endif
        if ((uint64_t)(uint32_t)cs > (max_os * 4ull + 1048576ull))
          continue;
      }

#if defined(ELFZ_FILTER_BCJ)
      const size_t hdr_len = COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
                             sizeof(int);
#elif defined(ELFZ_FILTER_KANZIEXE)
      size_t hdr_len = COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
                       sizeof(int) + sizeof(int);
#if defined(CODEC_DENSITY)
      {
        uint32_t tag = 0;
        const unsigned char *tagp = q + hdr_len;
        if ((size_t)(scan_hi - tagp) >= 4u) {
          simple_memcpy(&tag, tagp, sizeof(tag));
          if (tag == 0x21303454u)
            hdr_len += (sizeof(uint32_t) + 40u);
        }
      }
#endif
#else
      const size_t hdr_len = COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
                             sizeof(int);
#endif

      if ((size_t)(scan_hi - q) < hdr_len + (size_t)(uint32_t)cs)
        continue;

#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
      {
        const unsigned char *cand_end = q + hdr_len + (size_t)(uint32_t)cs;
        if (cand_end > p)
          continue;
        if ((size_t)(scan_hi - cand_end) >= 32u) {
          uint64_t acc = 0;
          for (int zi = 0; zi < 32; zi++)
            acc |= (uint64_t)cand_end[zi];
          if (acc != 0)
            continue;
        }

        size_t gap = (size_t)(p - cand_end);
        if (gap < best_gap) {
          best_gap = gap;
          best = q;
        }
      }
#endif

#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
      if (best == q)
        continue;
#endif
      size_t tail = (size_t)(scan_hi - (q + hdr_len + (size_t)(uint32_t)cs));
      if (tail < best_tail) {
        best_tail = tail;
        best = q;
      }
    }
  }

#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
  return best;
#endif

#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
  {
    const unsigned char *mv_best = NULL;
    size_t mv_best_gap = (size_t)-1;

    uint64_t cap_hi = (uint64_t)p + (512ULL << 20);
    uint64_t cap_lo = ((uint64_t)p > (512ULL << 20)) ? ((uint64_t)p - (512ULL << 20)) : 0;

    {
      uint64_t next_addr = (uint64_t)p;
      int vma_scans = 0;
      while (vma_scans < 16) {
        uint64_t lo = 0, hi = 0;
        while (find_vma_for_addr(next_addr, &lo, &hi) != 0 || hi <= next_addr) {
          next_addr += 0x1000;
          if (next_addr >= cap_hi)
            break;
        }
        if (next_addr >= cap_hi)
          break;
        if (hi <= next_addr)
          break;

        const unsigned char *search_start =
            (const unsigned char *)((next_addr == (uint64_t)p) ? (uint64_t)p : lo);
        const unsigned char *search_end = (const unsigned char *)(uintptr_t)hi;
        if ((uint64_t)(uintptr_t)search_end > cap_hi)
          search_end = (const unsigned char *)(uintptr_t)cap_hi;

        for (const unsigned char *r = search_start; r + 32 < search_end; ++r) {
          if (r[0] != COMP_MARKER[0] || r[1] != COMP_MARKER[1] || r[2] != COMP_MARKER[2] ||
              r[3] != COMP_MARKER[3] || r[4] != COMP_MARKER[4] || r[5] != COMP_MARKER[5])
            continue;

          const unsigned char *hdr = r + COMP_MARKER_LEN;
          size_t os = *(const size_t *)(hdr);
          int cs = *(const int *)(hdr + sizeof(size_t) + sizeof(uint64_t));
          int fs = *(const int *)(hdr + sizeof(size_t) + sizeof(uint64_t) + sizeof(int));

          if ((uint64_t)os <= 1000ull || os > 0x20000000u)
            continue;
          if (cs <= 100 || cs > 0x20000000)
            continue;
          if (fs <= 0 || fs > 0x20000000)
            continue;

          {
            uint64_t ufs = (uint64_t)(uint32_t)fs;
            uint64_t uos = (uint64_t)os;
            if (ufs < uos)
              continue;
            if ((ufs - uos) > 256u)
              continue;
          }

          {
            uint64_t max_os = (uint64_t)os;
            if ((uint64_t)(uint32_t)fs > max_os)
              max_os = (uint64_t)(uint32_t)fs;
            if ((uint64_t)(uint32_t)cs > (max_os * 4ull + 1048576ull))
              continue;
          }

          const size_t hdr_len = COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
                                 sizeof(int) + sizeof(int);
          if ((size_t)(search_end - r) < hdr_len + (size_t)(uint32_t)cs)
            continue;

          const unsigned char *cand_end = r + hdr_len + (size_t)(uint32_t)cs;
          if (cand_end > search_end)
            continue;

          size_t gap;
          if (cand_end <= p)
            gap = (size_t)(p - cand_end);
          else
            gap = (size_t)(cand_end - p) + (size_t)(512u << 20);

          if (gap < 16384u)
            continue;

          if (gap < mv_best_gap) {
            mv_best_gap = gap;
            mv_best = r;
          }
        }

        next_addr = hi;
        vma_scans++;
      }
    }

    {
      uint64_t prev_addr = (uint64_t)p;
      int vma_scans = 0;
      while (vma_scans < 16) {
        uint64_t lo = 0, hi = 0;
        if (find_vma_for_addr(prev_addr, &lo, &hi) != 0 || hi <= lo) {
          if (prev_addr <= cap_lo + 0x1000)
            break;
          prev_addr -= 0x1000;
          continue;
        }

        const unsigned char *search_start = (const unsigned char *)(uintptr_t)lo;
        const unsigned char *search_end = (const unsigned char *)(uintptr_t)hi;
        if ((uint64_t)(uintptr_t)search_start < cap_lo)
          search_start = (const unsigned char *)(uintptr_t)cap_lo;

        for (const unsigned char *r = search_start; r + 32 < search_end; ++r) {
          if (r[0] != COMP_MARKER[0] || r[1] != COMP_MARKER[1] || r[2] != COMP_MARKER[2] ||
              r[3] != COMP_MARKER[3] || r[4] != COMP_MARKER[4] || r[5] != COMP_MARKER[5])
            continue;

          const unsigned char *hdr = r + COMP_MARKER_LEN;
          size_t os = *(const size_t *)(hdr);
          int cs = *(const int *)(hdr + sizeof(size_t) + sizeof(uint64_t));
          int fs = *(const int *)(hdr + sizeof(size_t) + sizeof(uint64_t) + sizeof(int));

          if ((uint64_t)os <= 1000ull || os > 0x20000000u)
            continue;
          if (cs <= 100 || cs > 0x20000000)
            continue;
          if (fs <= 0 || fs > 0x20000000)
            continue;

          {
            uint64_t ufs = (uint64_t)(uint32_t)fs;
            uint64_t uos = (uint64_t)os;
            if (ufs < uos)
              continue;
            if ((ufs - uos) > 256u)
              continue;
          }

          {
            uint64_t max_os = (uint64_t)os;
            if ((uint64_t)(uint32_t)fs > max_os)
              max_os = (uint64_t)(uint32_t)fs;
            if ((uint64_t)(uint32_t)cs > (max_os * 4ull + 1048576ull))
              continue;
          }

          const size_t hdr_len = COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
                                 sizeof(int) + sizeof(int);
          if ((size_t)(search_end - r) < hdr_len + (size_t)(uint32_t)cs)
            continue;

          const unsigned char *cand_end = r + hdr_len + (size_t)(uint32_t)cs;
          if (cand_end > search_end)
            continue;

          size_t gap;
          if (cand_end <= p)
            gap = (size_t)(p - cand_end);
          else
            gap = (size_t)(cand_end - p) + (size_t)(512u << 20);

          if (gap < 16384u)
            continue;

          if (gap < mv_best_gap) {
            mv_best_gap = gap;
            mv_best = r;
          }
        }

        if (lo <= cap_lo)
          break;
        prev_addr = lo - 1;
        vma_scans++;
      }
    }

    if (mv_best)
      return mv_best;
  }
#endif

  return best;
}

static inline void set_stack_return(unsigned long *stack_frame, uint64_t entry,
                                   unsigned long *argc_ptr) {
  stack_frame[0] = (unsigned long)entry;
  stack_frame[1] = (unsigned long)argc_ptr;
}

static inline void map_and_jump(unsigned long *stack_frame,
                               unsigned long *original_sp, uint64_t saved_x0,
                               const unsigned char *packed_data) {
  size_t original_size = *(const size_t *)(packed_data + COMP_MARKER_LEN);
  int compressed_size =
      *(const int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));

#if defined(ELFZ_FILTER_BCJ)
  int filtered_size = (int)original_size;
  const unsigned char *compressed_data =
      packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
  {
    int maybe_filtered = *(const int *)compressed_data;
    if (maybe_filtered == (int)original_size) {
      compressed_data += sizeof(int);
    }
  }
#elif defined(ELFZ_FILTER_KANZIEXE)
  int filtered_size =
      *(const int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
                     sizeof(int));
  const unsigned char *compressed_data = packed_data + COMP_MARKER_LEN + sizeof(size_t) +
                                         sizeof(uint64_t) + sizeof(int) + sizeof(int);
#if defined(CODEC_DENSITY)
  const unsigned char *density_kexe_tail40 = NULL;
  {
    int os_i = (int)original_size;
    if (filtered_size > os_i && (filtered_size - os_i) <= 256) {
      uint32_t tag = 0;
      simple_memcpy(&tag, compressed_data, sizeof(tag));
      if (tag == 0x21303454u) {
        density_kexe_tail40 = compressed_data + 4u;
        compressed_data += (sizeof(uint32_t) + 40u);
      }
    }
  }
#endif
#else
  int filtered_size = (int)original_size;
  const unsigned char *compressed_data =
      packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
#endif

  if (original_size == 0 || original_size > 0x20000000u)
    z_syscall_exit(3);
  if (compressed_size <= 0 || compressed_size > 0x20000000)
    z_syscall_exit(3);

  size_t alloc_size;
#if defined(CODEC_DENSITY)
  alloc_size = original_size + (original_size >> 2) + 131072u;

#if !defined(ELFZ_FILTER_BCJ)
  if ((size_t)filtered_size + 4096u > alloc_size)
    alloc_size = (size_t)filtered_size + 4096u;
#endif
#else
  alloc_size = original_size + (original_size >> 3) + 65536u;

#if !defined(ELFZ_FILTER_BCJ)
  if ((size_t)filtered_size + 4096u > alloc_size)
    alloc_size = (size_t)filtered_size + 4096u;
#endif
#endif

  void *combined_data =
      (void *)z_syscall_mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)combined_data < 0)
    z_syscall_exit(3);

  int dst_cap =
#if defined(ELFZ_FILTER_BCJ)
      (int)original_size;
#else
      (int)filtered_size;
#endif

#if defined(CODEC_DENSITY)
  dst_cap = (int)alloc_size;
#endif

  int decomp_result = -1;
#if defined(CODEC_EXO)
  {
    size_t sentinel = 4096u;
    size_t src_sz = (size_t)(uint32_t)compressed_size;
    void *cbuf_ptr = (void *)z_syscall_mmap(NULL, src_sz + sentinel,
                                            PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)cbuf_ptr < 0)
      z_syscall_exit(3);
    simple_memcpy(cbuf_ptr, compressed_data, src_sz);
    simple_memset((unsigned char *)cbuf_ptr + src_sz, 0, sentinel);
    decomp_result = lz4_decompress((const char *)cbuf_ptr, (char *)combined_data,
                                   compressed_size, dst_cap);
    (void)z_syscall_munmap(cbuf_ptr, src_sz + sentinel);
  }
#elif defined(CODEC_DENSITY)
  {
    size_t sentinel = 4096u;
    size_t src_sz = (size_t)(uint32_t)compressed_size;
    void *cbuf_ptr = (void *)z_syscall_mmap(NULL, src_sz + sentinel,
                                            PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)cbuf_ptr < 0)
      z_syscall_exit(3);
    simple_memcpy(cbuf_ptr, compressed_data, src_sz);
    simple_memset((unsigned char *)cbuf_ptr + src_sz, 0, sentinel);
    decomp_result = lz4_decompress((const char *)cbuf_ptr, (char *)combined_data,
                                   compressed_size, dst_cap);
    (void)z_syscall_munmap(cbuf_ptr, src_sz + sentinel);
  }
#else
  decomp_result =
      lz4_decompress((const char *)compressed_data, (char *)combined_data,
                     compressed_size, dst_cap);
#endif
  if (decomp_result <= 0) {
#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
    z_syscall_exit(51);
#else
    z_syscall_exit(5);
#endif
  }

#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
  if (density_kexe_tail40 && filtered_size > (int)original_size) {
    if ((int)filtered_size == decomp_result + 40) {
      unsigned char *tail = (unsigned char *)combined_data + (size_t)decomp_result;
      uint64_t acc = 0;
      for (int i = 0; i < 40; i++)
        acc |= (uint64_t)tail[i];
      if (acc == 0) {
        simple_memcpy(tail, density_kexe_tail40, 40u);
        decomp_result = (int)filtered_size;
      }
    }
  }
#endif

  {
    int expected = (int)original_size;
#if defined(ELFZ_FILTER_KANZIEXE)
    expected = (int)filtered_size;
#endif
    if (decomp_result != expected) {
#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
      {
        const unsigned char *p0 = (const unsigned char *)combined_data;
        /* Kanzi input starts with 0x20 (ARM64 marker). If we have it, let the
         * unfilter validate sizes instead of requiring decomp_result==filtered_size.
         * If we already have an ELF, assume filter was not actually applied.
         */
        if (p0[0] != 0x20) {
          if (p0[0] != 0x7f || p0[1] != 'E' || p0[2] != 'L' || p0[3] != 'F')
            z_syscall_exit(52);
        }
      }
#else
      z_syscall_exit(5);
#endif
    }
  }

#if defined(ELFZ_FILTER_BCJ)
  {
    int bcj_applied = 1;
    const unsigned char *pb = (const unsigned char *)sym_addr_elfz_params();
    if (pb[0] == '+' && pb[1] == 'z' && pb[2] == 'E' && pb[3] == 'L' &&
        pb[4] == 'F' && pb[5] == '-' && pb[6] == 'P' && pb[7] == 'R') {
      uint64_t params_version = *(const uint64_t *)(pb + 8);
      bcj_applied = ((params_version >> 8) & 1ULL) ? 1 : 0;
    }
    if (bcj_applied) {
      size_t done = bcj_arm64_decode((uint8_t *)combined_data,
                                     (size_t)decomp_result, 0);
      if (done == 0)
        z_syscall_exit(5);
    }
  }
#endif

#if defined(ELFZ_FILTER_KANZIEXE)
  {
    int kexe_applied = 1;
    const unsigned char *pb = (const unsigned char *)sym_addr_elfz_params();
    if (pb[0] == '+' && pb[1] == 'z' && pb[2] == 'E' && pb[3] == 'L' &&
        pb[4] == 'F' && pb[5] == '-' && pb[6] == 'P' && pb[7] == 'R') {
      uint64_t params_version = *(const uint64_t *)(pb + 8);
      kexe_applied = ((params_version >> 9) & 1ULL) ? 1 : 0;
    }
    if (kexe_applied) {
      int kexe_input_sz = decomp_result;
      const uint8_t *kexe_in = (const uint8_t *)combined_data;

#if defined(CODEC_DENSITY)
      if (filtered_size > 0 && filtered_size <= decomp_result)
        kexe_input_sz = filtered_size;

      int kexe_off = 0;
      {
        if (kexe_in[0] == 0x7f && kexe_in[1] == 'E' && kexe_in[2] == 'L' &&
            kexe_in[3] == 'F')
          goto kexe_skip;

        int ok = 1;
        if (kexe_input_sz < 9)
          ok = 0;
        if ((kexe_input_sz - 9) < (int)original_size)
          ok = 0;
        if ((kexe_input_sz - 9) > ((int)original_size + 256))
          ok = 0;
        if (kexe_in[0] != 0x20)
          ok = 0;
        if (ok) {
          int code_start = arm64_read_le32(&kexe_in[1]);
          int code_end = arm64_read_le32(&kexe_in[5]);
          if (code_end > kexe_input_sz || code_start + 9 > kexe_input_sz)
            ok = 0;
          if (code_start < 0 || code_end < 9)
            ok = 0;
          if (code_end < (9 + code_start))
            ok = 0;
        }

        if (!ok) {
          for (int off = 1; off <= 64 && off + 9 < kexe_input_sz; off++) {
            if (kexe_in[off] != 0x20)
              continue;
            const uint8_t *cand = kexe_in + off;
            int cand_sz = kexe_input_sz - off;
            int cs = arm64_read_le32(&cand[1]);
            int ce = arm64_read_le32(&cand[5]);
            int ok2 = 1;
            if (cand_sz < 9)
              ok2 = 0;
            if ((cand_sz - 9) < (int)original_size)
              ok2 = 0;
            if ((cand_sz - 9) > ((int)original_size + 256))
              ok2 = 0;
            if (ce > cand_sz || cs + 9 > cand_sz)
              ok2 = 0;
            if (cs < 0 || ce < 9)
              ok2 = 0;
            if (ce < (9 + cs))
              ok2 = 0;
            if (cs > (int)original_size)
              ok2 = 0;
            if (ok2) {
              kexe_off = off;
              break;
            }
          }

          if (kexe_off == 0)
            z_syscall_exit(54);
        }
      }

      kexe_in += (size_t)kexe_off;
      kexe_input_sz -= kexe_off;
#else
      if (kexe_in[0] != 0x20)
        goto kexe_skip;
#endif
      void *unfiltered =
          (void *)z_syscall_mmap(NULL, (size_t)original_size,
                                 PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if ((long)unfiltered < 0)
        z_syscall_exit(3);

      int processed_f = 0;
      int uf = kanzi_exe_unfilter_arm64((const uint8_t *)kexe_in,
                                       kexe_input_sz, (uint8_t *)unfiltered,
                                       (int)original_size, &processed_f);
      if (uf != (int)original_size || processed_f != kexe_input_sz) {
#if defined(CODEC_DENSITY) && defined(ELFZ_FILTER_KANZIEXE)
        if (uf == 0)
          z_syscall_exit(55);
        if (uf != (int)original_size)
          z_syscall_exit(56);
        if (processed_f != kexe_input_sz)
          z_syscall_exit(57);
        z_syscall_exit(53);
#else
        z_syscall_exit(5);
#endif
      }

      (void)z_syscall_munmap(combined_data, alloc_size);
      combined_data = unfiltered;
      alloc_size = (size_t)original_size;
      decomp_result = uf;
    }
  kexe_skip:
    ;
  }
#endif

  if (((unsigned char *)combined_data)[0] != 0x7f ||
      ((unsigned char *)combined_data)[1] != 'E' ||
      ((unsigned char *)combined_data)[2] != 'L' ||
      ((unsigned char *)combined_data)[3] != 'F')
    z_syscall_exit(6);

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)combined_data;
  if (ehdr->e_type != 2 && ehdr->e_type != 3)
    z_syscall_exit(1);

  Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)combined_data + ehdr->e_phoff);

  uint64_t lo = ~0ull;
  uint64_t hi = 0;
  int has_dynamic = 0;

  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_DYNAMIC)
      has_dynamic = 1;
    if (phdr[i].p_type != PT_LOAD)
      continue;
    if (phdr[i].p_vaddr < lo)
      lo = phdr[i].p_vaddr;
    uint64_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
    if (end > hi)
      hi = end;
  }

  if (lo == ~0ull)
    z_syscall_exit(1);

  void *elf_base = 0;
  uint64_t base_offset = 0;

  if (ehdr->e_type == 3) {
    lo &= ~0xFFFull;
    size_t hull_len = (size_t)((hi - lo + 0xFFFu) & ~0xFFFull);
    void *hull = (void *)z_syscall_mmap(NULL, hull_len, PROT_NONE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)hull < 0)
      z_syscall_exit(3);

    if ((uint64_t)hull < 0x10000u) {
      (void)z_syscall_munmap(hull, hull_len);
      hull = (void *)z_syscall_mmap((void *)0x10000u, hull_len, PROT_NONE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if ((long)hull < 0 || (uint64_t)hull < 0x10000u)
        z_syscall_exit(3);
    }
    elf_base = (char *)hull - lo;
    base_offset = (uint64_t)elf_base;
  } else {
    elf_base = (void *)(uintptr_t)lo;
    base_offset = 0;
  }

  void *hatch = NULL;

  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type != PT_LOAD)
      continue;

    uint64_t page_base = phdr[i].p_vaddr & ~0xFFFull;
    uint64_t map_end = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFFu) & ~0xFFFull;
    uint64_t map_size = map_end - page_base;

    int prot = pf_to_prot(phdr[i].p_flags);

    void *target_addr = 0;
    if (ehdr->e_type == 3) {
      target_addr = (char *)elf_base + page_base;
    } else {
      target_addr = (void *)(uintptr_t)page_base;
    }
    void *mapped =
        (void *)z_syscall_mmap(target_addr, (size_t)map_size, prot | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
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
      void *new_hatch = make_hatch_x86_64(&phdr[i], 0, ~0xFFFull);
      if (new_hatch)
        hatch = new_hatch;
    }

    if (!(prot & PROT_WRITE))
      z_syscall_mprotect(mapped, (size_t)map_size, prot);
  }

  if (ehdr->e_type == 3)
    apply_complete_relocations(elf_base, base_offset);

  (void)z_syscall_munmap(combined_data, alloc_size);

  if (has_dynamic) {
    if (!hatch)
      z_syscall_exit(88);

    loader_info_t li =
        handle_dynamic_interpreter_complete(elf_base, original_sp, hatch);
    if (!li.at_null_entry)
      z_syscall_exit(1);

    elfz_fold_final_sequence(li.loader_entry, original_sp, 0, 0, elf_base,
                             li.interp_base, li.hatch_ptr, saved_x0);
  }

  uint64_t entry = (ehdr->e_type == 3) ? ((uint64_t)elf_base + ehdr->e_entry)
                                      : (uint64_t)ehdr->e_entry;

  set_stack_return(stack_frame, entry, original_sp);
}

void elfz_main_wrapper(unsigned long *stack_frame) {
  unsigned long *original_sp = (unsigned long *)stack_frame[2];
  uint64_t saved_x0 = stack_frame[4];

  const unsigned char *pb = (const unsigned char *)sym_addr_elfz_params();
  password_gate(pb);

  uint64_t start_addr = sym_addr_start();
  const unsigned char *packed_data = find_packed_data(start_addr);
  if (!packed_data)
    z_syscall_exit(4);

  map_and_jump(stack_frame, original_sp, saved_x0, packed_data);
}

int stub_dynexec_entry(unsigned long *original_sp) {
  (void)original_sp;
  return 0;
}
