#ifndef STUB_LOADER_H
#define STUB_LOADER_H

#include "stub_defs.h"
#include "stub_elf.h"
#include "stub_syscalls.h"
#include "stub_utils.h"

// Holds dynamic loader metadata
typedef struct {
  void *base_addr;
  uint64_t entry_point;
  const char *interp_path;
  int loaded;
} DynamicLoader;

// Loader context (used by ASM)
typedef struct {
  void *hatch_ptr;       // Pointeur vers l'adresse du hatch
  void *interp_base;     // Loader base in memory
  void *elf_base;        // Base of the decompressed binary
  uint64_t interp_entry; // Loader entry point
  uint64_t elf_entry;    // Original binary entry point
  unsigned long *stack;  // Prepared stack
} loader_context_t;

// Return entry point and AT_NULL address
typedef struct {
  uint64_t loader_entry;
  Elf64_auxv_t *at_null_entry;
  void *hatch_ptr;
  void *interp_base;
} loader_info_t;

// ASM function for final handover
extern void elfz_fold_final_sequence(uint64_t, unsigned long *, uint64_t,
                                     uint64_t, void *, void *, void *, uint64_t)
    __attribute__((noreturn));

// Load the dynamic interpreter into memory
static inline void *load_interpreter(const char *interp_path,
                                     unsigned long *original_stack) {
  if (!interp_path || !interp_path[0]) {
    return NULL;
  }

  // Open loader file
  int fd = (int)z_syscall_open(interp_path, 0, 0); // O_RDONLY = 0
  if (fd < 0) {
    return NULL;
  }

  // Allocate buffer with mmap to avoid stack corruption
  size_t header_bufsize = 2048;
  char *header_buf =
      (char *)z_syscall_mmap(NULL, header_bufsize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)header_buf < 0 && (long)header_buf >= -4096) {
    z_syscall_close(fd);
    return NULL;
  }

  long bytes_read = z_syscall_read(fd, header_buf, header_bufsize);
  if (bytes_read < (long)sizeof(Elf64_Ehdr)) {
    z_syscall_munmap(header_buf, header_bufsize);
    z_syscall_close(fd);
    return NULL;
  }

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)header_buf;

  // Validate ELF magic
  if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' ||
      ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
    z_syscall_munmap(header_buf, header_bufsize);
    z_syscall_close(fd);
    return NULL;
  }

  // Require ET_DYN
  if (ehdr->e_type != 3) { // ET_DYN = 3
    z_syscall_munmap(header_buf, header_bufsize);
    z_syscall_close(fd);
    return NULL;
  }

  // Get program headers
  Elf64_Phdr *phdr = (Elf64_Phdr *)(header_buf + ehdr->e_phoff);
  int load_count = 0;
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_LOAD)
      load_count++;
  }

  // Stabilization guard
  if (ehdr->e_phnum > 0) {
    volatile uint32_t sink = ((volatile Elf64_Phdr *)phdr)[0].p_type;
    (void)sink;
  }

  // COPY program headers into a safe buffer
  size_t phdr_size = ehdr->e_phnum * sizeof(Elf64_Phdr);
  Elf64_Phdr *safe_phdr =
      (Elf64_Phdr *)z_syscall_mmap(NULL, phdr_size, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)safe_phdr < 0 && (long)safe_phdr >= -4096) {
    z_syscall_munmap(header_buf, header_bufsize);
    z_syscall_close(fd);
    return NULL;
  }
  // Copy phdrs
  for (size_t i = 0; i < phdr_size / 8; i++) {
    ((uint64_t *)safe_phdr)[i] = ((uint64_t *)phdr)[i];
  }
  phdr = safe_phdr;

  uint16_t loader_phnum = ehdr->e_phnum;
  uint64_t loader_entry =
      ehdr->e_entry; // unused variable but kept for logic consistency

  z_syscall_munmap(header_buf, header_bufsize);
  header_buf = 0;

  // Reserve the convex hull of PT_LOAD segments
  uint64_t lo = ~0ULL;
  uint64_t hi = 0;
  for (int i = 0; i < loader_phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
      if (phdr[i].p_vaddr < lo)
        lo = phdr[i].p_vaddr;
      uint64_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
      if (end > hi)
        hi = end;
    }
  }
  lo &= ~0xFFFULL;
  size_t hull_len = (size_t)((hi - lo + 0xFFFULL) & ~0xFFFULL);

  void *hull = (void *)z_syscall_mmap(NULL, hull_len, PROT_NONE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  long mmap_result = (long)hull;
  if (mmap_result < 0 && mmap_result >= -4096) {
    z_syscall_munmap(safe_phdr, phdr_size); // Clean up safe_phdr too
    z_syscall_close(fd);
    return NULL;
  }
  char *base = (char *)hull - lo;

  if (original_stack) {
    Elf64_auxv_t *av = get_auxv_from_original_stack(original_stack);
    auxv_up(av, AT_BASE, (uint64_t)base);
  }

  int pt_load_count = 0;

  struct {
    uint64_t vaddr;
    uint64_t memsz;
    uint32_t flags;
  } saved_loads[16];

  for (int i = 0; i < loader_phnum && pt_load_count < 16; i++) {
    if (safe_phdr[i].p_type == PT_LOAD) {
      saved_loads[pt_load_count].vaddr = safe_phdr[i].p_vaddr;
      saved_loads[pt_load_count].memsz = safe_phdr[i].p_memsz;
      saved_loads[pt_load_count].flags = safe_phdr[i].p_flags;
      pt_load_count++;
    }
  }

  int mapped_count = 0;
  for (int i = 0; i < loader_phnum && mapped_count < pt_load_count; i++) {
    if (phdr[i].p_type != PT_LOAD)
      continue;
    mapped_count++;

    int prot = 0;
    if (phdr[i].p_flags & PF_R)
      prot |= PROT_READ;
    if (phdr[i].p_flags & PF_W)
      prot |= PROT_WRITE;
    if (phdr[i].p_flags & PF_X)
      prot |= PROT_EXEC;

    void *addr = (char *)base + phdr[i].p_vaddr;
    size_t page_offset = phdr[i].p_vaddr & 0xFFFULL;
    void *page_addr = (char *)addr - page_offset;
    size_t map_offset = phdr[i].p_offset - page_offset;
    size_t map_size = phdr[i].p_filesz + page_offset;
    map_size = (map_size + 0xFFF) & ~0xFFF;

    void *result = (void *)z_syscall_mmap(
        page_addr, map_size, prot, MAP_FIXED | MAP_PRIVATE, fd, map_offset);

    if ((long)result < 0 || result != page_addr) {
      z_syscall_munmap(hull, hull_len);
      z_syscall_munmap(safe_phdr, phdr_size);
      z_syscall_close(fd);
      return NULL;
    }

    if (phdr[i].p_memsz > phdr[i].p_filesz) {
      void *bss_start = (char *)addr + phdr[i].p_filesz;
      size_t hi_frag = (-(long)bss_start) & 0xFFF;
      if (hi_frag > 0 && (prot & PROT_WRITE)) {
        simple_memset(bss_start, 0, hi_frag);
        bss_start = (char *)bss_start + hi_frag;
      }
      void *page_aligned_bss = (void *)(((uint64_t)bss_start + 0xFFF) & ~0xFFF);
      void *bss_end = (char *)addr + phdr[i].p_memsz;
      if (page_aligned_bss < bss_end) {
        size_t bss_page_size = (char *)bss_end - (char *)page_aligned_bss;
        bss_page_size = (bss_page_size + 0xFFF) & ~0xFFF;
        if (bss_page_size > 0) {
          z_syscall_mmap(page_aligned_bss, bss_page_size, prot,
                         MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
      }
    }
  }
  z_syscall_close(fd);

  if (original_stack) {
    Elf64_auxv_t *av = get_auxv_from_original_stack(original_stack);
    auxv_up(av, AT_BASE, (uint64_t)base);
  }

  z_syscall_munmap(safe_phdr, phdr_size);
  return base;
}

// Load the loader and return its entry point
static inline loader_info_t handle_dynamic_interpreter_complete(
    void *base_addr, unsigned long *original_stack, void *hatch_from_binary) {
  (void)base_addr;         // unused param
  (void)hatch_from_binary; // unused param

  const char *interp_path = NULL;
  int found_interp = 0;
  {
    int fd = (int)z_syscall_open("/proc/self/exe", 0, 0); // O_RDONLY
    if (fd >= 0) {
      size_t hdr_sz = 4096;
      char *hdr = (char *)z_syscall_mmap(NULL, hdr_sz, PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if ((long)hdr > 0) {
        long n = z_syscall_read(fd, hdr, hdr_sz);
        if (n >= (long)sizeof(Elf64_Ehdr)) {
          Elf64_Ehdr *fe = (Elf64_Ehdr *)hdr;
          size_t phdr_bytes = (size_t)fe->e_phnum * sizeof(Elf64_Phdr);
          Elf64_Phdr *fph = NULL;
          if (fe->e_phoff + phdr_bytes <= (size_t)n) {
            fph = (Elf64_Phdr *)(hdr + fe->e_phoff);
          } else {
            fph = (Elf64_Phdr *)z_syscall_mmap(
                NULL, phdr_bytes, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if ((long)fph > 0) {
              long rn = z_syscall_pread(fd, fph, phdr_bytes, fe->e_phoff);
              if (rn != (long)phdr_bytes) {
                z_syscall_munmap(fph, phdr_bytes);
                fph = NULL;
              }
            } else {
              fph = NULL;
            }
          }
          if (fph) {
            for (int i = 0; i < fe->e_phnum; i++) {
              if (fph[i].p_type == PT_INTERP && fph[i].p_filesz > 0 &&
                  fph[i].p_filesz < 4096) {
                size_t psz = (size_t)fph[i].p_filesz;
                char *pbuf = (char *)z_syscall_mmap(
                    NULL, psz + 1, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if ((long)pbuf > 0) {
                  long rn2 = z_syscall_pread(fd, pbuf, psz, fph[i].p_offset);
                  if (rn2 > 0) {
                    if (rn2 == (long)psz)
                      pbuf[psz] = '\0';
                    else
                      pbuf[rn2] = '\0';
                    if (pbuf[0]) {
                      interp_path = pbuf;
                      found_interp = 1;
                    } else {
                      z_syscall_munmap(pbuf, psz + 1);
                    }
                  }
                }
                break;
              }
            }
            if ((void *)fph != (void *)(hdr + fe->e_phoff)) {
              z_syscall_munmap(fph, phdr_bytes);
            }
          }
        }
        z_syscall_munmap(hdr, hdr_sz);
      }
      z_syscall_close(fd);
    }
    if (!found_interp) {
      interp_path = "/lib64/ld-linux-x86-64.so.2";
    }
  }

  void *hatch =
      (void *)z_syscall_mmap(NULL, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)hatch < 0) {
    loader_info_t info = {0, NULL, NULL, NULL};
    return info;
  }

  unsigned char *hc = (unsigned char *)hatch;
  hc[0] = 0x0f; // syscall
  hc[1] = 0x05;
  hc[2] = 0x5a; // pop %rdx
  hc[3] = 0xc3; // ret

  void *interp_base = load_interpreter(interp_path, original_stack);
  if (!interp_base) {
    loader_info_t info = {0, NULL, NULL, NULL};
    return info;
  }

  Elf64_Ehdr *loader_ehdr = (Elf64_Ehdr *)interp_base;
  uint64_t loader_entry = (uint64_t)interp_base + loader_ehdr->e_entry;

  if (loader_entry != 0) {
    Elf64_auxv_t *at_null_entry = get_auxv_from_original_stack(original_stack);
    while (at_null_entry->a_type != AT_NULL) {
      at_null_entry++;
    }

    loader_info_t info;
    info.loader_entry = loader_entry;
    info.at_null_entry = at_null_entry;
    info.hatch_ptr = hatch;
    info.interp_base = interp_base;
    return info;
  } else {
    loader_info_t info = {0, NULL, NULL, NULL};
    return info;
  }
}

#endif // STUB_LOADER_H
