#ifndef STUB_RELOC_H
#define STUB_RELOC_H

#include "stub_defs.h"

// Relocate pointers inside PT_DYNAMIC
static inline void relocate_dynamic_pointers(void *base_addr,
                                             uint64_t base_offset) {
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base_addr;
  Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)base_addr + ehdr->e_phoff);

  Elf64_Dyn *dynamic = NULL;
  uint64_t dynamic_size = 0;

  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      dynamic = (Elf64_Dyn *)((char *)base_addr + phdr[i].p_vaddr);
      dynamic_size = phdr[i].p_memsz;
      break;
    }
  }

  if (!dynamic)
    return;

  // Calculate max entries from PT_DYNAMIC size
  // Fallback to 1024 if size is missing (should not happen on valid ELF)
  int max_entries =
      (dynamic_size > 0) ? (int)(dynamic_size / sizeof(Elf64_Dyn)) : 1024;

  // Tags that contain pointers (d_ptr) and must be relocated
  const uint64_t ptr_tags[] = {DT_PLTGOT,
                               DT_HASH,
                               0x6ffffef5 /* DT_GNU_HASH */,
                               DT_STRTAB,
                               DT_SYMTAB,
                               DT_RELA,
                               DT_INIT,
                               DT_FINI,
                               0x19 /* DT_INIT_ARRAY */,
                               0x1a /* DT_FINI_ARRAY */,
                               0x1e /* DT_PREINIT_ARRAY */,
                               DT_JMPREL,
                               0x6ffffffe /* DT_VERNEED */,
                               0x6ffffffc /* DT_VERDEF */,
                               0x6ffffff0 /* DT_VERSYM */,
                               DT_RELR /* 0x24 - pour loader moderne */,
                               0};

  int loop_guard = 0;

  for (Elf64_Dyn *d = dynamic; d->d_tag != DT_NULL; d++) {
    // Guard against infinite loops (corruption)
    // Limit based on the actual size of the DYNAMIC segment
    if (++loop_guard > max_entries) {
      break;
    }

    // Check if this tag holds a pointer
    int is_ptr = 0;
    for (int i = 0; ptr_tags[i] != 0; i++) {
      if (d->d_tag == ptr_tags[i]) {
        is_ptr = 1;
        break;
      }
    }

    if (is_ptr && d->d_un.d_ptr != 0) {
      d->d_un.d_ptr += base_offset;
    }
  }
}

// Strategy 1: standard DYNAMIC relocations
static inline int apply_dynamic_relocations(void *base_addr,
                                            uint64_t base_offset) {
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base_addr;
  Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)base_addr + ehdr->e_phoff);

  Elf64_Dyn *dynamic = NULL;
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      dynamic = (Elf64_Dyn *)((char *)base_addr + phdr[i].p_vaddr);
      break;
    }
  }

  if (!dynamic) {
    return 0;
  }

  Elf64_Rela *rela_table = NULL;
  uint64_t rela_size = 0;
  for (Elf64_Dyn *d = dynamic; d->d_tag != DT_NULL; d++) {
    switch (d->d_tag) {
    case DT_RELA:
      rela_table = (Elf64_Rela *)d->d_un.d_ptr;
      break;
    case DT_RELASZ:
      rela_size = d->d_un.d_val;
      break;
    }
  }

  int count = 0;
  // Apply RELA entries
  if (rela_table && rela_size > 0) {
    int entries = rela_size / sizeof(Elf64_Rela);
    for (int i = 0; i < entries; i++) {
      Elf64_Rela *rela = &rela_table[i];
      uint32_t type = rela->r_info & 0xFFFFFFFF;

      if (type == R_X86_64_RELATIVE) {
        uint64_t *target = (uint64_t *)((char *)base_addr + rela->r_offset);
        *target = base_offset + rela->r_addend;
        count++;
      }
    }
  }
  return count;
}

// Strategy 2: relocations via DT_PLTGOT (GOT)
static inline int apply_got_relocations(void *base_addr, uint64_t base_offset) {
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base_addr;
  Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)base_addr + ehdr->e_phoff);
  Elf64_Dyn *dynamic = NULL;

  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      dynamic = (Elf64_Dyn *)((char *)base_addr + phdr[i].p_vaddr);
      break;
    }
  }

  if (!dynamic)
    return 0;

  uint64_t got_addr = 0;

  // Note: dynamic pointers have already been relocated by
  // relocate_dynamic_pointers
  for (Elf64_Dyn *d = dynamic; d->d_tag != DT_NULL; d++) {
    if (d->d_tag == DT_PLTGOT) {
      got_addr = d->d_un.d_ptr;
      break;
    }
  }

  if (got_addr == 0)
    return 0;

  // GOT start (first 3 entries reserved on x86_64)
  uint64_t *got = (uint64_t *)got_addr;
  int count = 0;

  // DEBUG TRACE (commented out)
  // char msg[] = "Using DT_PLTGOT strategy\n";
  // z_syscall_write(1, msg, sizeof(msg)-1);

  // Scan GOT entries
  // We don't have exact size, so we use a safe heuristic loop
  // Valid entries in PIE are usually small virtual addresses (< Image Size)
  for (int i = 3; i < 2048; i++) {
    uint64_t val = got[i];

    // Stop on null or very large value (likely end of GOT or garbage)
    if (val == 0)
      break;

    // Check if value looks like a relative pointer (offset within binary)
    // 0x1000 is min page size, 0x20000000 is 512MB max binary size
    if (val >= 0x1000 && val < 0x20000000) {
      got[i] += base_offset;
      count++;
    } else {
      // Only break if we hit something that definitely isn't a pointer?
      // For now, let's be conservative and continue carefully
      if (val > 0x7FFFFFFFFFFF)
        break; // Kernel address / garbage
    }
  }
  return count;
}

// Strategy 3: full scan of read-write segments (FALLBACK)
// DOCUMENTATION: brute-force heuristic to find pointers to relocate. Scans
// RW memory for values that look like valid offsets (0x1000..0x100000).
// RISK: may corrupt data that only looks like a pointer. USED ONLY IF
// STANDARD METHODS (DT_RELA, DT_PLTGOT) FAIL.
static inline int apply_rw_segments_relocations(void *base_addr,
                                                uint64_t base_offset) {
  // DEBUG TRACE (commented out)
  // char msg[] = "WARNING: Triggering heuristic RW relocation fallback!\n";
  // z_syscall_write(1, msg, sizeof(msg)-1);

  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base_addr;
  Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)base_addr + ehdr->e_phoff);
  int count = 0;
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == 1 && (phdr[i].p_flags & 2)) { // PT_LOAD + WRITE
      uint64_t start = (uint64_t)base_addr + phdr[i].p_vaddr;
      uint64_t end = start + phdr[i].p_memsz;

      for (uint64_t addr = start; addr < end - 8; addr += 8) {
        uint64_t *ptr = (uint64_t *)addr;
        uint64_t value = *ptr;

        if (value >= 0x1000 && value < 0x100000 && (value & 0x7) == 0) {

          *ptr = value + base_offset;
          count++;

          if (count > 1000)
            break;
        }
      }

      if (count > 1000)
        break;
    }
  }
  return count;
}

// Main entry applying all strategies
static inline void apply_complete_relocations(void *base_addr,
                                              uint64_t base_offset) {
  int total_relocations = 0;
  relocate_dynamic_pointers(base_addr, base_offset);
  total_relocations += apply_dynamic_relocations(base_addr, base_offset);

  if (total_relocations == 0) {
    total_relocations += apply_got_relocations(base_addr, base_offset);
  }

  if (total_relocations == 0) {
    total_relocations += apply_rw_segments_relocations(base_addr, base_offset);
  }
}

#endif // STUB_RELOC_H
