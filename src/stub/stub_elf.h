#ifndef STUB_ELF_H
#define STUB_ELF_H

#include "stub_defs.h"
#include "stub_syscalls.h"
#include "stub_utils.h"

// rtld uses the LAST value if multiple slots share the same tag
static inline void auxv_up(Elf64_auxv_t *av, unsigned const tag,
                           uint64_t const value) {
  if (!av || (1 & (size_t)av)) { // none, or inhibited for PT_INTERP
    return;
  }
  // Multiple slots can have 'tag' which wastes space but is legal.
  // rtld (ld-linux) uses the last one, so we must scan the whole table.
  Elf64_auxv_t *ignore_slot = 0;
  int found = 0;
  for (;; ++av) {
    if (av->a_type == tag) {
      av->a_un.a_val = value;
      ++found;
    } else if (av->a_type == AT_IGNORE) {
      ignore_slot = av;
    }
    if (av->a_type == AT_NULL) { // done scanning
      if (found) {
        return;
      }
      if (ignore_slot) {
        ignore_slot->a_type = tag;
        ignore_slot->a_un.a_val = value;
        return;
      }
      return;
    }
  }
}

static inline Elf64_auxv_t *
get_auxv_from_original_stack(unsigned long *original_stack) {
  unsigned long *sp = original_stack;
  int argc = *(sp++);
  sp += argc + 1; // argv + NULL
  while (*sp)
    sp++; // envp...
  sp++;   // NULL
  return (Elf64_auxv_t *)sp;
}

// Helpers to parse /proc/self/maps and find the VMA containing a given address
static inline int is_hex_digit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

static inline int hex_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return -1;
}

// Parse one maps line: "start-end perms ..."; returns 0 on success
static inline int parse_maps_line_range(const char *line, int len,
                                        uint64_t *start, uint64_t *end,
                                        int *has_x) {
  int i = 0;
  uint64_t v = 0;
  // parse start
  while (i < len && is_hex_digit(line[i])) {
    int hv = hex_value(line[i++]);
    v = (v << 4) | (uint64_t)hv;
  }
  if (i >= len || line[i] != '-')
    return -1;
  *start = v;
  i++; // skip '-'
  // parse end
  v = 0;
  while (i < len && is_hex_digit(line[i])) {
    int hv = hex_value(line[i++]);
    v = (v << 4) | (uint64_t)hv;
  }
  if (i >= len || line[i] != ' ')
    return -1;
  *end = v;
  // skip space
  i++;
  // parse perms (up to 4 chars like "r-xp")
  int x = 0;
  for (int k = 0; k < 4 && i + k < len; k++) {
    char c = line[i + k];
    if (c == ' ')
      break;
    if (c == 'x')
      x = 1;
  }
  if (has_x)
    *has_x = x;
  return 0;
}

// Find the VMA covering addr by scanning /proc/self/maps; returns 0 on success
static inline int find_vma_for_addr(uint64_t addr, uint64_t *out_start,
                                    uint64_t *out_end) {
  int fd = (int)z_syscall_open("/proc/self/maps", 0, 0); // O_RDONLY
  if (fd < 0)
    return -1;
  size_t bufsize = 262144; // 256 KB should fit typical maps
  char *buf = (char *)z_syscall_mmap(NULL, bufsize, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)buf < 0) {
    z_syscall_close(fd);
    return -1;
  }
  long n = z_syscall_read(fd, buf, bufsize);
  z_syscall_close(fd);
  if (n <= 0) {
    z_syscall_munmap(buf, bufsize);
    return -1;
  }
  int found = 0;
  int line_start = 0;
  for (int i = 0; i < n; i++) {
    if (buf[i] == '\n' || i == n - 1) {
      int line_len = (i - line_start) + (buf[i] == '\n' ? 0 : 1);
      uint64_t s = 0, e = 0;
      int has_x = 0;
      if (line_len > 0 && parse_maps_line_range(buf + line_start, line_len, &s,
                                                &e, &has_x) == 0) {
        if (addr >= s && addr < e) {
          // Prefer executable mapping, but accept any that contains addr
          *out_start = s;
          *out_end = e;
          found = 1;
          // if exec, good enough; break early
          if (has_x)
            break;
        }
      }
      line_start = i + 1;
    }
  }
  z_syscall_munmap(buf, bufsize);
  return found ? 0 : -1;
}

// Create the hatch while mapping the binary
static inline void *make_hatch_x86_64(Elf64_Phdr const *const phdr,
                                      uint64_t reloc, uint64_t frag_mask) {
  unsigned *hatch = 0;

  if (phdr->p_type == PT_LOAD && (phdr->p_flags & PF_X)) {
    // Force allocation of a new page so the hatch cannot be overwritten
    hatch = (unsigned *)z_syscall_mmap(0, 0x1000,
                                       PROT_WRITE | PROT_READ | PROT_EXEC,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (hatch != (void *)-1 && hatch != NULL) {
      hatch[0] = 0xc35a050f; // 0x0f 0x05 0x5a 0xc3 = syscall; pop %rdx; ret
    } else {
      hatch = 0;
    }
  }
  return hatch;
}

#endif // STUB_ELF_H
