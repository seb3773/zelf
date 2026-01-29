#ifndef STUB_LOADER_H
#define STUB_LOADER_H

#include "stub_defs.h"
#include "stub_elf.h"
#include "stub_syscalls.h"
#include "stub_utils.h"

typedef struct {
  void *base_addr;
  uint64_t entry_point;
  const char *interp_path;
  int loaded;
} DynamicLoader;

typedef struct {
  void *hatch_ptr;
  void *interp_base;
  void *elf_base;
  uint64_t interp_entry;
  uint64_t elf_entry;
  unsigned long *stack;
} loader_context_t;

typedef struct {
  uint64_t loader_entry;
  Elf64_auxv_t *at_null_entry;
  void *hatch_ptr;
  void *interp_base;
} loader_info_t;

extern void elfz_fold_final_sequence(uint64_t, unsigned long *, uint64_t,
                                     uint64_t, void *, void *, void *, uint64_t)
    __attribute__((noreturn, visibility("hidden")));

// Complexity: O(phnum + file IO). Dependency: interp_path is a valid ELF64 ET_DYN.
static inline void *load_interpreter(const char *interp_path,
                                     unsigned long *original_stack) {
  if (!interp_path || !interp_path[0])
    return NULL;

  int fd = (int)z_syscall_open(interp_path, 0, 0);
  if (fd < 0)
    return NULL;

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
  if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' ||
      ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
    z_syscall_munmap(header_buf, header_bufsize);
    z_syscall_close(fd);
    return NULL;
  }

  if (ehdr->e_type != 3) {
    z_syscall_munmap(header_buf, header_bufsize);
    z_syscall_close(fd);
    return NULL;
  }

  Elf64_Phdr *phdr = (Elf64_Phdr *)(header_buf + ehdr->e_phoff);

  size_t phdr_size = (size_t)ehdr->e_phnum * sizeof(Elf64_Phdr);
  Elf64_Phdr *safe_phdr =
      (Elf64_Phdr *)z_syscall_mmap(NULL, phdr_size, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)safe_phdr < 0 && (long)safe_phdr >= -4096) {
    z_syscall_munmap(header_buf, header_bufsize);
    z_syscall_close(fd);
    return NULL;
  }

  for (size_t i = 0; i < phdr_size / 8; i++)
    ((uint64_t *)safe_phdr)[i] = ((uint64_t *)phdr)[i];

  uint16_t loader_phnum = ehdr->e_phnum;

  z_syscall_munmap(header_buf, header_bufsize);
  header_buf = 0;

  uint64_t lo = ~0ULL;
  uint64_t hi = 0;
  for (int i = 0; i < loader_phnum; i++) {
    if (safe_phdr[i].p_type == PT_LOAD) {
      if (safe_phdr[i].p_vaddr < lo)
        lo = safe_phdr[i].p_vaddr;
      uint64_t end = safe_phdr[i].p_vaddr + safe_phdr[i].p_memsz;
      if (end > hi)
        hi = end;
    }
  }

  lo &= ~0xFFFULL;
  size_t hull_len = (size_t)((hi - lo + 0xFFFULL) & ~0xFFFULL);
  void *hull = (void *)z_syscall_mmap(NULL, hull_len, PROT_NONE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)hull < 0 && (long)hull >= -4096) {
    z_syscall_munmap(safe_phdr, phdr_size);
    z_syscall_close(fd);
    return NULL;
  }

  char *base = (char *)hull - lo;

  if (original_stack) {
    Elf64_auxv_t *av = get_auxv_from_original_stack(original_stack);
    auxv_up(av, AT_BASE, (uint64_t)base);
  }

  int mapped_count = 0;
  for (int i = 0; i < loader_phnum; i++) {
    if (safe_phdr[i].p_type != PT_LOAD)
      continue;

    mapped_count++;

    int prot = 0;
    if (safe_phdr[i].p_flags & PF_R)
      prot |= PROT_READ;
    if (safe_phdr[i].p_flags & PF_W)
      prot |= PROT_WRITE;
    if (safe_phdr[i].p_flags & PF_X)
      prot |= PROT_EXEC;

    void *addr = (char *)base + safe_phdr[i].p_vaddr;
    size_t page_offset = (size_t)(safe_phdr[i].p_vaddr & 0xFFFULL);
    void *page_addr = (char *)addr - page_offset;
    size_t map_offset = (size_t)safe_phdr[i].p_offset - page_offset;
    size_t map_size = (size_t)safe_phdr[i].p_filesz + page_offset;
    map_size = (map_size + 0xFFF) & ~0xFFF;

    void *result = (void *)z_syscall_mmap(page_addr, map_size, prot,
                                         MAP_FIXED | MAP_PRIVATE, fd,
                                         (off_t)map_offset);
    if ((long)result < 0 || result != page_addr) {
      z_syscall_munmap(hull, hull_len);
      z_syscall_munmap(safe_phdr, phdr_size);
      z_syscall_close(fd);
      return NULL;
    }

    if (safe_phdr[i].p_memsz > safe_phdr[i].p_filesz) {
      void *bss_start = (char *)addr + safe_phdr[i].p_filesz;
      size_t hi_frag = (size_t)(-(long)bss_start) & 0xFFF;
      if (hi_frag > 0 && (prot & PROT_WRITE)) {
        simple_memset(bss_start, 0, hi_frag);
        bss_start = (char *)bss_start + hi_frag;
      }
      void *page_aligned_bss =
          (void *)(((uint64_t)bss_start + 0xFFF) & ~0xFFF);
      void *bss_end = (char *)addr + safe_phdr[i].p_memsz;
      if (page_aligned_bss < bss_end) {
        size_t bss_page_size = (size_t)((char *)bss_end - (char *)page_aligned_bss);
        bss_page_size = (bss_page_size + 0xFFF) & ~0xFFF;
        if (bss_page_size > 0) {
          z_syscall_mmap(page_aligned_bss, bss_page_size, prot,
                         MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
      }
    }
  }

  z_syscall_close(fd);
  z_syscall_munmap(safe_phdr, phdr_size);

  if (mapped_count == 0) {
    z_syscall_munmap(hull, hull_len);
    return NULL;
  }

  return base;
}

// Complexity: O(phnum + IO). Dependency: /proc/self/exe readable.
static inline loader_info_t handle_dynamic_interpreter_complete(
    void *base_addr, unsigned long *original_stack, void *hatch_from_binary) {
  (void)base_addr;
  (void)hatch_from_binary;

  const char *interp_path = NULL;
  int found_interp = 0;
  {
    int fd = (int)z_syscall_open("/proc/self/exe", 0, 0);
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
            fph = (Elf64_Phdr *)z_syscall_mmap(NULL, phdr_bytes,
                                               PROT_READ | PROT_WRITE,
                                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if ((long)fph > 0) {
              long rn = z_syscall_pread(fd, fph, phdr_bytes, (off_t)fe->e_phoff);
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
                char *pbuf = (char *)z_syscall_mmap(NULL, psz + 1,
                                                    PROT_READ | PROT_WRITE,
                                                    MAP_PRIVATE | MAP_ANONYMOUS,
                                                    -1, 0);
                if ((long)pbuf > 0) {
                  long rn2 = z_syscall_pread(fd, pbuf, psz, (off_t)fph[i].p_offset);
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
            if ((void *)fph != (void *)(hdr + fe->e_phoff))
              z_syscall_munmap(fph, phdr_bytes);
          }
        }
        z_syscall_munmap(hdr, hdr_sz);
      }
      z_syscall_close(fd);
    }

    if (!found_interp) {
      interp_path = "/lib/ld-linux-aarch64.so.1";
    }
  }

  void *hatch =
      (void *)z_syscall_mmap(NULL, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)hatch < 0) {
    loader_info_t info = {0, NULL, NULL, NULL};
    return info;
  }

  uint32_t *hc = (uint32_t *)hatch;
  hc[0] = 0xd4000001u; // svc #0
  hc[1] = 0xf94003e0u;
  hc[2] = 0xf94007e1u;
  hc[3] = 0xf9400be2u;
  hc[4] = 0xf9400fe3u;
  hc[5] = 0xf94013feu;
  hc[6] = 0x9100c3ffu;
  hc[7] = 0xd65f03c0u;

  void *interp_base = load_interpreter(interp_path, original_stack);
  if (!interp_base) {
    loader_info_t info = {0, NULL, NULL, NULL};
    return info;
  }

  Elf64_Ehdr *loader_ehdr = (Elf64_Ehdr *)interp_base;
  uint64_t loader_entry = (uint64_t)interp_base + loader_ehdr->e_entry;

  if (loader_entry != 0) {
    Elf64_auxv_t *at_null_entry = get_auxv_from_original_stack(original_stack);
    while (at_null_entry->a_type != AT_NULL)
      at_null_entry++;

    loader_info_t info;
    info.loader_entry = loader_entry;
    info.at_null_entry = at_null_entry;
    info.hatch_ptr = hatch;
    info.interp_base = interp_base;
    return info;
  }

  {
    loader_info_t info = {0, NULL, NULL, NULL};
    return info;
  }
}

#endif // STUB_LOADER_H
