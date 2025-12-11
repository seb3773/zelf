#include "elf_utils.h"
#include "zelf_packer.h"

void fail(const char *msg) {
  perror(msg);
  exit(1);
}

size_t align_up(size_t val, size_t align) {
  return (val + align - 1) & ~(align - 1);
}

// Patch helper: find the .elfz_params block in a stub blob and
// write the 64-bit values for virtual_start and packed_data_vaddr.
int patch_elfz_params(unsigned char *buf, size_t sz, uint64_t virtual_start,
                      uint64_t packed_data_vaddr) {
  // New magic for the .elfz_params block (8 bytes)
  const unsigned char magic[8] = {'+', 'z', 'E', 'L', 'F', '-', 'P', 'R'};
  if (sz < 8 + 8 + 8 + 8)
    return 0;
  for (size_t i = 0; i + 8 + 8 + 8 + 8 <= sz; i++) {
    if (memcmp(buf + i, magic, 8) == 0) {
      // Layout: magic[8], version[8], virtual_start[8], packed_data_vaddr[8],
      // reserved[8]
      memcpy(buf + i + 16, &virtual_start, sizeof(uint64_t));
      memcpy(buf + i + 24, &packed_data_vaddr, sizeof(uint64_t));
      return 1;
    }
  }
  return 0;
}

// Patch helper v2: write virtual_start, packed_data_vaddr, salt, obfuscated
// hash
int patch_elfz_params_v2(unsigned char *buf, size_t sz, uint64_t virtual_start,
                         uint64_t packed_data_vaddr, uint64_t salt,
                         uint64_t obfhash) {
  const unsigned char magic[8] = {'+', 'z', 'E', 'L', 'F', '-', 'P', 'R'};
  if (sz < 8 + 8 + 8 + 8 + 8 + 8)
    return 0;
  for (size_t i = 0; i + 48 <= sz; i++) {
    if (memcmp(buf + i, magic, 8) == 0) {
      memcpy(buf + i + 16, &virtual_start, sizeof(uint64_t));
      memcpy(buf + i + 24, &packed_data_vaddr, sizeof(uint64_t));
      memcpy(buf + i + 32, &salt, sizeof(uint64_t));
      memcpy(buf + i + 40, &obfhash, sizeof(uint64_t));
      return 1;
    }
  }
  return 0;
}

// Patch helper: OR runtime flags into the 64-bit version field
// Layout: magic[8], version[8], ...
// We encode flags in bits [15:8] of version, preserving base version (1 or 2)
int patch_elfz_flags(unsigned char *buf, size_t sz, int base_version,
                     uint8_t flags) {
  const unsigned char magic[8] = {'+', 'z', 'E', 'L', 'F', '-', 'P', 'R'};
  if (sz < 16)
    return 0;
  for (size_t i = 0; i + 16 <= sz; i++) {
    if (memcmp(buf + i, magic, 8) == 0) {
      uint64_t v = (uint64_t)(unsigned)base_version;
      v |= ((uint64_t)flags) << 8;
      memcpy(buf + i + 8, &v, sizeof(v));
      return 1;
    }
  }
  return 0;
}

Elf64_Addr find_safe_stub_vaddr(Elf64_Phdr *phdr, int phnum) {
  Elf64_Addr max_end = 0;

  VPRINTF("[%s%s%s] Computing a safe address for the stub:\n",
          PK_ARROW, PK_SYM_ARROW, PK_RES);
  for (int i = 0; i < phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
      Elf64_Addr end = phdr[i].p_vaddr + phdr[i].p_memsz;
      VPRINTF("[%s%s%s] Segment LOAD: 0x%lx - 0x%lx\n",
              PK_INFO, PK_SYM_INFO, PK_RES, phdr[i].p_vaddr, end);
      if (end > max_end)
        max_end = end;
    }
  }

  // Align to 0x100000 to avoid conflicts
  Elf64_Addr stub_vaddr = (max_end + 0xFFFFF) & ~0xFFFFF;
  VPRINTF(
      "[%s%s%s] Safe address computed: 0x%lx (after 0x%lx)\n",
      PK_INFO, PK_SYM_INFO, PK_RES, stub_vaddr, max_end);
  return stub_vaddr;
}
