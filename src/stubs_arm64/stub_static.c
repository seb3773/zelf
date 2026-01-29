#include "codec_marker.h"
#include "codec_select.h"
#include "stub_vars.h"
#include <stddef.h>
#include <stdint.h>

#include "stub_defs.h"
#include "stub_elf.h"
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
        '+', 'z', 'E', 'L', 'F', '-', 'P', 'R', 2, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#else
__attribute__((used, section(".rodata.elfz_params"))) static const unsigned char
    elfz_params_block[] = {
        '+', 'z', 'E', 'L', 'F', '-', 'P', 'R', 1, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0};
#endif

extern void _start(void);

static inline uint64_t sym_addr_elfz_params(void) {
  uint64_t v;
  __asm__ volatile("adrp %0, elfz_params_block\n"
                   "add  %0, %0, :lo12:elfz_params_block\n"
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
    if (z_syscall_ioctl(fd, TCSETS, &tio_new) == 0)
      echo_changed = 1;
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

static inline void set_stack_return(unsigned long *stack_frame, uint64_t entry,
                                   unsigned long *argc_ptr) {
  stack_frame[0] = (unsigned long)entry;
  stack_frame[1] = (unsigned long)argc_ptr;
}

static inline uint64_t find_elf_header_vaddr(Elf64_Ehdr *ehdr,
                                            Elf64_Phdr *phdr) {
  uint64_t min_base = ~0ull;
  int have_off0 = 0;

  for (int i = 0; i < ehdr->e_phnum; i++) {
    Elf64_Phdr *p = &phdr[i];
    if (p->p_type != PT_LOAD)
      continue;

    uint64_t base = p->p_vaddr - p->p_offset;
    if (base < min_base)
      min_base = base;
    if (p->p_offset == 0)
      have_off0 = 1;
  }

  if (have_off0)
    return min_base;

  if (min_base == ~0ull)
    return 0;

  return min_base;
}

static inline void map_and_prepare(unsigned long *stack_frame,
                                  unsigned long *original_sp,
                                  uint64_t stub_runtime_addr,
                                  const unsigned char *pb) {
  uint64_t params_virtual_start = 0;
  uint64_t params_packed_data_vaddr = 0;

  if (!(pb[0] == '+' && pb[1] == 'z' && pb[2] == 'E' && pb[3] == 'L' &&
        pb[4] == 'F' && pb[5] == '-' && pb[6] == 'P' && pb[7] == 'R')) {
    z_syscall_exit(11);
  }

  params_virtual_start = *(const uint64_t *)(pb + 16);
  params_packed_data_vaddr = *(const uint64_t *)(pb + 24);

  if (params_virtual_start == 0 || params_packed_data_vaddr == 0)
    z_syscall_exit(65);

  uint64_t aslr_offset = stub_runtime_addr - params_virtual_start;
  const unsigned char *packed_data =
      (const unsigned char *)(params_packed_data_vaddr + aslr_offset);

  size_t original_size = *(const size_t *)(packed_data + COMP_MARKER_LEN);
  uint64_t entry_offset =
      *(const uint64_t *)(packed_data + COMP_MARKER_LEN + sizeof(size_t));
  int compressed_size =
      *(const int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t));

#if defined(ELFZ_FILTER_BCJ)
  int filtered_size = 0;
  const unsigned char *compressed_stream =
      packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
  {
    int maybe_filtered = *(const int *)compressed_stream;
    if (maybe_filtered == (int)original_size) {
      compressed_stream += sizeof(int);
    }
  }
#elif defined(ELFZ_FILTER_KANZIEXE)
  int filtered_size =
      *(const int *)(packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) +
                     sizeof(int));
  const unsigned char *compressed_stream = packed_data + COMP_MARKER_LEN + sizeof(size_t) +
                                           sizeof(uint64_t) + sizeof(int) + sizeof(int);
#else
  int filtered_size = (int)original_size;
  const unsigned char *compressed_stream =
      packed_data + COMP_MARKER_LEN + sizeof(size_t) + sizeof(uint64_t) + sizeof(int);
#endif

  (void)entry_offset;

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

  int dst_cap = (int)alloc_size;
#if defined(ELFZ_FILTER_BCJ)
  dst_cap = (int)original_size;
#elif defined(ELFZ_FILTER_KANZIEXE)
  dst_cap = (int)filtered_size;
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
    simple_memcpy(cbuf_ptr, compressed_stream, src_sz);
    simple_memset((unsigned char *)cbuf_ptr + src_sz, 0, sentinel);
    decomp_result = lz4_decompress((const char *)cbuf_ptr, (char *)combined_data,
                                   compressed_size, dst_cap);
    (void)z_syscall_munmap(cbuf_ptr, src_sz + sentinel);
  }
#elif defined(CODEC_BRIEFLZ) || defined(CODEC_SNAPPY) || defined(CODEC_SHRINKLER)
  {
    size_t sentinel = 4096u;
    size_t src_sz = (size_t)(uint32_t)compressed_size;
    void *cbuf_ptr = (void *)z_syscall_mmap(NULL, src_sz + sentinel,
                                            PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)cbuf_ptr < 0)
      z_syscall_exit(3);
    simple_memcpy(cbuf_ptr, compressed_stream, src_sz);
    simple_memset((unsigned char *)cbuf_ptr + src_sz, 0, sentinel);
    decomp_result = lz4_decompress((const char *)cbuf_ptr, (char *)combined_data,
                                   compressed_size, dst_cap);
    (void)z_syscall_munmap(cbuf_ptr, src_sz + sentinel);
  }
#else
  decomp_result =
      lz4_decompress((const char *)compressed_stream, (char *)combined_data,
                     compressed_size, dst_cap);
#endif
  if (decomp_result <= 0)
    z_syscall_exit(5);

  {
    int expected = (int)original_size;
#if defined(ELFZ_FILTER_KANZIEXE)
    expected = (int)filtered_size;
#endif
    if (decomp_result != expected)
      z_syscall_exit(5);
  }

#if defined(ELFZ_FILTER_BCJ)
  {
    int bcj_applied = 1;
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
    if (pb[0] == '+' && pb[1] == 'z' && pb[2] == 'E' && pb[3] == 'L' &&
        pb[4] == 'F' && pb[5] == '-' && pb[6] == 'P' && pb[7] == 'R') {
      uint64_t params_version = *(const uint64_t *)(pb + 8);
      kexe_applied = ((params_version >> 9) & 1ULL) ? 1 : 0;
    }
    if (kexe_applied) {
      void *unfiltered =
          (void *)z_syscall_mmap(NULL, (size_t)original_size,
                                 PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if ((long)unfiltered < 0)
        z_syscall_exit(3);

      int processed_f = 0;
      int uf = kanzi_exe_unfilter_arm64((const uint8_t *)combined_data,
                                       decomp_result, (uint8_t *)unfiltered,
                                       (int)original_size, &processed_f);
      if (uf != (int)original_size || processed_f != decomp_result)
        z_syscall_exit(5);

      (void)z_syscall_munmap(combined_data, alloc_size);
      combined_data = unfiltered;
      alloc_size = (size_t)original_size;
      decomp_result = uf;
    }
  }
#endif

  unsigned char *elf_data = (unsigned char *)combined_data;
  if (elf_data[0] != 0x7f || elf_data[1] != 'E' || elf_data[2] != 'L' ||
      elf_data[3] != 'F') {
    z_syscall_exit(6);
  }

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)combined_data;
  if (ehdr->e_type != 2 && ehdr->e_type != 3)
    z_syscall_exit(1);

  Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)combined_data + ehdr->e_phoff);

  uint64_t lo = ~0ull;
  uint64_t hi = 0;
  for (int i = 0; i < ehdr->e_phnum; i++) {
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

  void *base = NULL;
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
    base = (char *)hull - lo;
    base_offset = (uint64_t)base;
  } else {
    base = (void *)(uintptr_t)lo;
    base_offset = 0;
  }

  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type != PT_LOAD)
      continue;

    uint64_t page_base = phdr[i].p_vaddr & ~0xFFFull;
    uint64_t map_end = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFFu) & ~0xFFFull;
    uint64_t map_size = map_end - page_base;

    int prot = pf_to_prot(phdr[i].p_flags);

    void *target_addr = 0;
    if (ehdr->e_type == 3) {
      target_addr = (char *)base + page_base;
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

    if (!(prot & PROT_WRITE))
      z_syscall_mprotect(mapped, (size_t)map_size, prot);
  }

  uint64_t entry_point =
      (ehdr->e_type == 3) ? ((uint64_t)base + ehdr->e_entry) : (uint64_t)ehdr->e_entry;

  uint64_t hdr_vaddr = 0;
  if (ehdr->e_type == 3) {
    hdr_vaddr = (uint64_t)base;
  } else {
    hdr_vaddr = find_elf_header_vaddr(ehdr, phdr);
  }

  if (hdr_vaddr == 0)
    z_syscall_exit(1);

  {
    Elf64_auxv_t *auxv = get_auxv_from_original_stack(original_sp);
    auxv_up(auxv, AT_ENTRY, entry_point);
    auxv_up(auxv, AT_PHDR, (uint64_t)hdr_vaddr + ehdr->e_phoff);
    auxv_up(auxv, AT_PHNUM, ehdr->e_phnum);
    auxv_up(auxv, AT_PHENT, ehdr->e_phentsize);
    auxv_up(auxv, AT_BASE, 0);
  }

  (void)z_syscall_munmap(combined_data, alloc_size);

  set_stack_return(stack_frame, entry_point, original_sp);
}

void elfz_main_wrapper(unsigned long *stack_frame) {
  unsigned long *original_sp = (unsigned long *)stack_frame[2];
  uint64_t stub_runtime_addr = (uint64_t)stack_frame[3];

  const unsigned char *pb = (const unsigned char *)sym_addr_elfz_params();
  password_gate(pb);

  map_and_prepare(stack_frame, original_sp, stub_runtime_addr, pb);
}

int stub_dynamic_entry(unsigned long *original_sp) {
  (void)original_sp;
  return 0;
}
