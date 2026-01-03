#ifndef ELF_BUILDER_H
#define ELF_BUILDER_H

#include "packer_defs.h"

int write_packed_binary(const char *output_path, int has_pt_interp,
                        const unsigned char *stub_data, size_t stub_sz,
                        size_t stub_memsz,
                        const unsigned char *compressed_data, size_t comp_size,
                        size_t actual_size, uint64_t entry_offset,
                        size_t filtered_size, const char *stub_codec,
                        exe_filter_t filter_type, Elf64_Off stub_off,
                        Elf64_Addr stub_vaddr, Elf64_Addr packed_vaddr,
                        size_t packed_off, const char *marker,
                        uint8_t elfz_flags, int use_password,
                        uint64_t pwd_salt, uint64_t pwd_obf);

#endif
