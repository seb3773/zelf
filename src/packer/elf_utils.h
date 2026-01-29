#ifndef ELF_UTILS_H
#define ELF_UTILS_H
#include "packer_defs.h"

void fail(const char *msg);
size_t align_up(size_t val, size_t align);
Elf64_Addr find_safe_stub_vaddr(Elf64_Phdr *phdr, int phnum);
int patch_elfz_params(unsigned char *buf, size_t sz, uint64_t virtual_start,
                      uint64_t packed_data_vaddr);
int patch_elfz_params_v2(unsigned char *buf, size_t sz, uint64_t virtual_start,
                         uint64_t packed_data_vaddr, uint64_t salt,
                         uint64_t obfhash);
int patch_elfz_flags(unsigned char *buf, size_t sz, int base_version,
                     uint8_t flags);

#endif
