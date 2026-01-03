#include "elf_builder.h"
#include "elf_utils.h"
// Version and color macros (monochrome gating via g_no_colors)
#include "zelf_packer.h"

int write_packed_binary(
    const char *output_path, int has_pt_interp, const unsigned char *stub_data,
    size_t stub_sz, size_t stub_memsz, const unsigned char *compressed_data,
    size_t comp_size, size_t actual_size, uint64_t entry_offset,
    size_t filtered_size, const char *stub_codec, exe_filter_t g_exe_filter,
    Elf64_Off stub_off, Elf64_Addr stub_vaddr, Elf64_Addr packed_vaddr,
    size_t packed_off, const char *marker, uint8_t elfz_flags, int use_password,
    uint64_t pwd_salt, uint64_t pwd_obf) {
  size_t stub_memsz_effective = stub_memsz ? stub_memsz : stub_sz;

  // Prepare the patched stub (mutable copy)
  unsigned char *stub_data_final = malloc(stub_sz);
  if (!stub_data_final) {
    perror("malloc stub_data_final");
    exit(1);
  }
  memcpy(stub_data_final, stub_data, stub_sz);

  VPRINTF("[%s%s%s] Final stub: %zu bytes\n", PK_INFO, PK_SYM_INFO, PK_RES,
          stub_sz);

  // NOTE: for zstd and density DYNAMIC stage-0 wrappers, the file stub bytes
  // are
  //   [stage0][hdr][NRV-compressed-stub]
  // and patching .elfz_params here would corrupt the NRV stream.
  // The real stub is patched *before* NRV compression in zelf_packer.c.
  //
  // Density STATIC does NOT use stage0 (the stub doesn't compress well enough),
  // so it must be patched here like other codecs.
  int uses_stage0 = (strcmp(stub_codec, "zstd") == 0) ||
                    (strcmp(stub_codec, "density") == 0 && has_pt_interp);
  if (!uses_stage0) {
    if (use_password) {
      uint64_t param_vstart = has_pt_interp ? 0 : stub_vaddr;
      uint64_t param_paddr = has_pt_interp ? 0 : packed_vaddr;
      if (patch_elfz_params_v2(stub_data_final, stub_sz, param_vstart,
                               param_paddr, pwd_salt, pwd_obf)) {
        VPRINTF("[%s%s%s] .elfz_params v2 patched: vstart=0x%lx, paddr=0x%lx, "
                "salt=0x%lx\n",
                PK_INFO, PK_SYM_INFO, PK_RES, (unsigned long)param_vstart,
                (unsigned long)param_paddr, (unsigned long)pwd_salt);
      } else {
        VPRINTF("[%s%s%s] Warning: .elfz_params v2 not found in stub; no patch "
                "applied\n",
                PK_WARN, PK_SYM_WARN, PK_RES);
      }
    } else {
      if (!has_pt_interp) {
        if (patch_elfz_params(stub_data_final, stub_sz, stub_vaddr,
                              packed_vaddr)) {
          VPRINTF("[%s%s%s] .elfz_params patched: virtual_start=0x%lx, "
                  "packed_data_vaddr=0x%lx\n",
                  PK_INFO, PK_SYM_INFO, PK_RES, (unsigned long)stub_vaddr,
                  (unsigned long)packed_vaddr);
        } else {
          VPRINTF("[%s%s%s] Warning: .elfz_params not found in STATIC stub; no "
                  "patch applied\n",
                  PK_WARN, PK_SYM_WARN, PK_RES);
        }
      }
    }

    {
      int base_ver = use_password ? 2 : 1;
      if (patch_elfz_flags(stub_data_final, stub_sz, base_ver, elfz_flags)) {
        VPRINTF("[%s%s%s] .elfz_params flags patched: base=%d, flags=0x%02x\n",
                PK_INFO, PK_SYM_INFO, PK_RES, base_ver, (unsigned)elfz_flags);
      } else {
        if (has_pt_interp) {
          /* For dynamic binaries the .elfz_params block may be absent or
           * unused (runtime resolves addresses). Emit a non-warning info
           * message to avoid noisy output while keeping original behaviour. */
          VPRINTF("[%s%s%s] Info: unable to patch .elfz_params flags (dynamic "
                  "binary)\n",
                  PK_INFO, PK_SYM_INFO, PK_RES);
        } else {
          VPRINTF("[%s%s%s] Warning: unable to patch .elfz_params flags\n",
                  PK_WARN, PK_SYM_WARN, PK_RES);
        }
      }
    }
  }

  // Build the ELF header
  Elf64_Ehdr new_ehdr = {0};
  memcpy(new_ehdr.e_ident,
         "\x7f"
         "ELF",
         4);
  new_ehdr.e_ident[EI_CLASS] = ELFCLASS64;
  new_ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
  new_ehdr.e_ident[EI_VERSION] = EV_CURRENT;

  if (has_pt_interp) {
    new_ehdr.e_type = ET_DYN;
    VPRINTF("[%s%s%s] ELF type: ET_DYN (dynamic binary)\n", PK_INFO,
            PK_SYM_INFO, PK_RES);
  } else {
    new_ehdr.e_type = ET_EXEC;
    VPRINTF("[%s%s%s] ELF type: ET_EXEC (static binary)\n", PK_INFO,
            PK_SYM_INFO, PK_RES);
  }
  new_ehdr.e_machine = EM_X86_64;
  new_ehdr.e_version = EV_CURRENT;

  // Compute entry point
  if (has_pt_interp) {
    Elf64_Off stub_file_offset = 0x100;
    // Entry in C_TEXT: vaddr C_TEXT 0x5000 + stub offset 0x100
    new_ehdr.e_entry = 0x5000 + stub_file_offset;
    VPRINTF("[%s%s%s] Dynamic entry: 0x%lx (vaddr C_TEXT 0x5000 + stub offset "
            "0x%lx)\n",
            PK_INFO, PK_SYM_INFO, PK_RES, new_ehdr.e_entry, stub_file_offset);
  } else {
    new_ehdr.e_entry = stub_vaddr;
    VPRINTF("[%s%s%s] Static entry (absolute): 0x%lx (stub_vaddr)\n", PK_INFO,
            PK_SYM_INFO, PK_RES, new_ehdr.e_entry);
  }

  new_ehdr.e_phoff = sizeof(Elf64_Ehdr);
  new_ehdr.e_ehsize = sizeof(Elf64_Ehdr);
  new_ehdr.e_phentsize = sizeof(Elf64_Phdr);
  new_ehdr.e_shoff = 0;
  new_ehdr.e_shnum = 0;
  new_ehdr.e_shstrndx = 0;

  // Segments differ for static/dynamic builds
  Elf64_Phdr stub_segment, packed_segment, stack_segment;

  if (has_pt_interp) {
    new_ehdr.e_phnum = 3;

    // Dynamic packed ELFs use a file layout:
    //   [ELF+PHDRs][pad to 0x100][stub bytes][packed block...]
    // For zstd stage-0, the "stub bytes" are
    // [stage0][hdr][nrv-compressed-stub]. The decompressed stub is written
    // in-memory and requires extra p_memsz, but p_filesz must remain <= actual
    // file size to avoid SIGBUS.

    stub_segment = (Elf64_Phdr){.p_type = PT_LOAD,
                                .p_offset = 0,
                                .p_vaddr = 0x0,
                                .p_paddr = 0x0,
                                .p_filesz = 0x100 + stub_sz,
                                .p_memsz = 0x100 + stub_sz,
                                .p_flags = PF_W | PF_R | PF_X,
                                .p_align = ALIGN};

    int legacy_bcj_layout =
        ((strcmp(stub_codec, "zstd") == 0 ||
          strcmp(stub_codec, "density") == 0) ||
         ((g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) &&
          (strcmp(stub_codec, "lz4") == 0 ||
           strcmp(stub_codec, "apultra") == 0 ||
           strcmp(stub_codec, "lzav") == 0 || strcmp(stub_codec, "lzma") == 0 ||
           strcmp(stub_codec, "blz") == 0 || strcmp(stub_codec, "doboz") == 0 ||
           strcmp(stub_codec, "exo") == 0 || strcmp(stub_codec, "zx0") == 0 ||
           strcmp(stub_codec, "zx7b") == 0 || strcmp(stub_codec, "pp") == 0 ||
           strcmp(stub_codec, "qlz") == 0 ||
           strcmp(stub_codec, "snappy") == 0 ||
           strcmp(stub_codec, "shrinkler") == 0 ||
           strcmp(stub_codec, "rnc") == 0 ||
           strcmp(stub_codec, "lzfse") == 0 ||
           strcmp(stub_codec, "csc") == 0 ||
           strcmp(stub_codec, "nz1") == 0 ||
           strcmp(stub_codec, "lzsa") == 0 ||
           strcmp(stub_codec, "lzham") == 0 ||
           strcmp(stub_codec, "density") == 0)));
    int marker_len = strlen(marker);
    size_t packed_block_size =
        marker_len + sizeof(size_t) + sizeof(uint64_t) + sizeof(int) +
        (legacy_bcj_layout ? 0 : sizeof(int)) + comp_size;
    size_t ctext_filesz = 0x100 + stub_sz + packed_block_size;
    size_t ctext_memsz = ctext_filesz;
    if (strcmp(stub_codec, "zstd") == 0 || strcmp(stub_codec, "density") == 0) {
      // Reserve enough memory for stage-0 to write the decompressed stub.
      ctext_memsz = 0x100 + stub_memsz_effective;
      if (ctext_memsz < ctext_filesz)
        ctext_memsz = ctext_filesz;
    }
    packed_segment =
        (Elf64_Phdr){.p_type = PT_LOAD,
                     .p_offset = 0,
                     .p_vaddr = 0x5000,
                     .p_paddr = 0x5000,
                     .p_filesz = ctext_filesz,
                     .p_memsz = ctext_memsz,
                     .p_flags = (strcmp(stub_codec, "zstd") == 0 ||
                                 strcmp(stub_codec, "density") == 0)
                                    ? (PF_X | PF_W | PF_R)
                                    : (PF_X | PF_R),
                     .p_align = ALIGN};

    stack_segment = (Elf64_Phdr){.p_type = 0x6474e551, // PT_GNU_STACK
                                 .p_offset = 0,
                                 .p_vaddr = 0,
                                 .p_paddr = 0,
                                 .p_filesz = 0,
                                 .p_memsz = 0,
                                 .p_flags = PF_W | PF_R,
                                 .p_align = 0x10};

    VPRINTF("[%s%s%s] 3 segments: C_BASE(RW), C_TEXT(R E), GNU_STACK\n",
            PK_INFO, PK_SYM_INFO, PK_RES);
  } else {
    new_ehdr.e_phnum = 2;

    // For static binaries: ZSTD uses stage0 (needs PF_W), density does NOT use
    // stage0
    stub_segment = (Elf64_Phdr){.p_type = PT_LOAD,
                                .p_offset = stub_off,
                                .p_vaddr = stub_vaddr,
                                .p_paddr = stub_vaddr,
                                .p_filesz = stub_sz,
                                .p_memsz = stub_memsz_effective,
                                .p_flags = (strcmp(stub_codec, "zstd") == 0)
                                               ? (PF_W | PF_R | PF_X)
                                               : (PF_X | PF_R),
                                .p_align = ALIGN};

    int legacy_bcj_layout =
        (strcmp(stub_codec, "zstd") == 0 || strcmp(stub_codec, "density") == 0)
            ? 1
            : ((g_exe_filter == EXE_FILTER_BCJ ||
                g_exe_filter == EXE_FILTER_NONE) &&
               (strcmp(stub_codec, "lz4") == 0 ||
                strcmp(stub_codec, "apultra") == 0 ||
                strcmp(stub_codec, "lzav") == 0 ||
                strcmp(stub_codec, "lzma") == 0 ||
                strcmp(stub_codec, "zstd") == 0 ||
                strcmp(stub_codec, "blz") == 0 ||
                strcmp(stub_codec, "doboz") == 0 ||
                strcmp(stub_codec, "exo") == 0 ||
                strcmp(stub_codec, "zx0") == 0 ||
                strcmp(stub_codec, "zx7b") == 0 ||
                strcmp(stub_codec, "pp") == 0 ||
                strcmp(stub_codec, "snappy") == 0 ||
                strcmp(stub_codec, "shrinkler") == 0 ||
                strcmp(stub_codec, "rnc") == 0 ||
                strcmp(stub_codec, "lzfse") == 0 ||
                strcmp(stub_codec, "csc") == 0 ||
                strcmp(stub_codec, "nz1") == 0 ||
                strcmp(stub_codec, "lzsa") == 0 ||
                (strcmp(stub_codec, "lzham") == 0 &&
                 g_exe_filter != EXE_FILTER_KANZIEXE) ||
                strcmp(stub_codec, "density") == 0));
    int marker_len = strlen(marker);
    size_t p_memsz_val = marker_len + sizeof(size_t) + sizeof(uint64_t) +
                         sizeof(int) + (legacy_bcj_layout ? 0 : sizeof(int)) +
                         comp_size;

    packed_segment = (Elf64_Phdr){.p_type = PT_LOAD,
                                  .p_offset = packed_off,
                                  .p_vaddr = packed_vaddr,
                                  .p_paddr = packed_vaddr,
                                  .p_filesz = p_memsz_val,
                                  .p_memsz = p_memsz_val,
                                  .p_flags = PF_R,
                                  .p_align = ALIGN};
  }

  // Calculate exact final size BEFORE writing anything
  int legacy_bcj_layout_pre =
      ((strcmp(stub_codec, "zstd") == 0 ||
        strcmp(stub_codec, "density") == 0) ||
       ((g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) &&
        (strcmp(stub_codec, "lz4") == 0 || strcmp(stub_codec, "apultra") == 0 ||
         strcmp(stub_codec, "lzav") == 0 || strcmp(stub_codec, "lzma") == 0 ||
         strcmp(stub_codec, "blz") == 0 || strcmp(stub_codec, "doboz") == 0 ||
         strcmp(stub_codec, "exo") == 0 || strcmp(stub_codec, "zx0") == 0 ||
         strcmp(stub_codec, "zx7b") == 0 || strcmp(stub_codec, "pp") == 0 ||
         strcmp(stub_codec, "qlz") == 0 || strcmp(stub_codec, "snappy") == 0 ||
         strcmp(stub_codec, "shrinkler") == 0 ||
         strcmp(stub_codec, "rnc") == 0 ||
         strcmp(stub_codec, "lzfse") == 0 ||
         strcmp(stub_codec, "csc") == 0 ||
         strcmp(stub_codec, "nz1") == 0 ||
         strcmp(stub_codec, "lzsa") == 0 ||
         strcmp(stub_codec, "density") == 0)));
  int marker_len_pre = strlen(marker);
  size_t packed_block_size_pre =
      marker_len_pre + sizeof(size_t) + sizeof(uint64_t) + sizeof(int) +
      (legacy_bcj_layout_pre ? 0 : sizeof(int)) + comp_size;

  size_t predicted_final_size;
  if (has_pt_interp) {
    // Dynamic: 0x100 + stub + packed_block (no extra padding)
    predicted_final_size = 0x100 + stub_sz + packed_block_size_pre;
  } else {
    // Static: packed_off + packed_block
    // Check for recalculation case (pad3 > 1000000)
    size_t headers_size = sizeof(Elf64_Ehdr) + 2 * sizeof(Elf64_Phdr);
    size_t after_stub = stub_off + stub_sz;
    if (after_stub > packed_off) {
      // Would trigger recalculation - use aligned position
      size_t new_packed_off = align_up(after_stub, ALIGN);
      predicted_final_size = new_packed_off + packed_block_size_pre;
    } else {
      predicted_final_size = packed_off + packed_block_size_pre;
    }
  }

  // Abort if packing does not reduce size
  if (predicted_final_size >= actual_size) {
    fprintf(stderr,
            "[%s%s%s] Packing did not reduce size. Packed: %zu bytes >= "
            "Original: %zu bytes. Nothing written.\n",
            PK_ERR, PK_SYM_ERR, PK_RES, predicted_final_size, actual_size);
    free(stub_data_final);
    return 2;
  }

  const char *final_bin = output_path ? output_path : FINAL_BIN;
  FILE *out = fopen(final_bin, "wb");
  if (!out)
    fail("fopen output");

  fwrite(&new_ehdr, 1, sizeof(new_ehdr), out);
  fwrite(&stub_segment, 1, sizeof(stub_segment), out);
  fwrite(&packed_segment, 1, sizeof(packed_segment), out);
  if (has_pt_interp) {
    fwrite(&stack_segment, 1, sizeof(stack_segment), out);
  }

  long current_pos;
  if (has_pt_interp) {
    VPRINTF("[%s%s%s] dynamic stub.bin already extracted (VIRTUAL_START "
            "preconfigured)\n",
            PK_INFO, PK_SYM_INFO, PK_RES);
    current_pos = ftell(out);
    size_t pad_headers = 0x100 - current_pos;
    for (size_t i = 0; i < pad_headers; i++) {
      fputc(0, out);
    }
    fwrite(stub_data_final, 1, stub_sz, out);
    VPRINTF("[%s%s%s] Stub written at offset 0x100 (after %zu-byte padding)\n",
            PK_INFO, PK_SYM_INFO, PK_RES, pad_headers);
  } else {
    VPRINTF("[%s%s%s] static stub.bin not needed (precompiled stub)\n", PK_INFO,
            PK_SYM_INFO, PK_RES);
    current_pos = ftell(out);
    size_t pad2 = stub_off - current_pos;
    for (size_t i = 0; i < pad2; i++) {
      fputc(0, out);
    }
    fwrite(stub_data_final, 1, stub_sz, out);
  }

  current_pos = ftell(out);
  size_t pad3;

  if (has_pt_interp) {
    pad3 = 0;
    VPRINTF(
        "[%s%s%s] Data immediately after stub (shared offset, no recompute)\n",
        PK_INFO, PK_SYM_INFO, PK_RES);
  } else {
    pad3 = packed_off - current_pos;
    if (pad3 > 1000000) {
      if (current_pos > (long)packed_off) {
        packed_off = align_up(current_pos, ALIGN);
        packed_vaddr = stub_vaddr + packed_off - stub_off;
        pad3 = packed_off - current_pos;
        packed_segment.p_offset = packed_off;
        packed_segment.p_vaddr = packed_vaddr;
        packed_segment.p_paddr = packed_vaddr;

        long current_fpos = ftell(out);
        fseek(out, sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr), SEEK_SET);
        fwrite(&packed_segment, 1, sizeof(packed_segment), out);
        fseek(out, current_fpos, SEEK_SET);

        /* Re-patch .elfz_params with corrected packed_vaddr after recalculation
         */
        /* Skip re-patching for stage0 codecs (ZSTD always, density only for
         * dynamic) */
        if (!uses_stage0 && !has_pt_interp) {
          VPRINTF("[%s%s%s] Re-patching .elfz_params after offset "
                  "recalculation: packed_vaddr=0x%lx\n",
                  PK_INFO, PK_SYM_INFO, PK_RES, (unsigned long)packed_vaddr);
          /* Re-patch the in-memory stub and rewrite it to the file */
          if (use_password) {
            (void)patch_elfz_params_v2(stub_data_final, stub_sz, stub_vaddr,
                                       packed_vaddr, pwd_salt, pwd_obf);
          } else {
            (void)patch_elfz_params(stub_data_final, stub_sz, stub_vaddr,
                                    packed_vaddr);
          }
          /* Rewrite the patched stub to the file */
          long saved_pos = ftell(out);
          fseek(out, stub_off, SEEK_SET);
          fwrite(stub_data_final, 1, stub_sz, out);
          fseek(out, saved_pos, SEEK_SET);
        }
      } else {
        fail("padding too large");
      }
    }
  }

  VPRINTF("[%s%s%s] Padding to data: %zu bytes (current_pos: %ld, packed_off: "
          "0x%lx)\n",
          PK_INFO, PK_SYM_INFO, PK_RES, pad3, current_pos, packed_off);

  for (size_t i = 0; i < pad3; i++) {
    fputc(0, out);
  }

  int legacy_bcj_layout =
      (strcmp(stub_codec, "zstd") == 0 || strcmp(stub_codec, "density") == 0)
          ? 1
          : ((g_exe_filter == EXE_FILTER_BCJ ||
              g_exe_filter == EXE_FILTER_NONE) &&
             (strcmp(stub_codec, "lz4") == 0 ||
              strcmp(stub_codec, "apultra") == 0 ||
              strcmp(stub_codec, "lzav") == 0 ||
              strcmp(stub_codec, "lzma") == 0 ||
              strcmp(stub_codec, "zstd") == 0 ||
              strcmp(stub_codec, "blz") == 0 ||
              strcmp(stub_codec, "doboz") == 0 ||
              strcmp(stub_codec, "exo") == 0 ||
              strcmp(stub_codec, "zx0") == 0 ||
              strcmp(stub_codec, "zx7b") == 0 ||
              strcmp(stub_codec, "pp") == 0 || strcmp(stub_codec, "qlz") == 0 ||
              strcmp(stub_codec, "snappy") == 0 ||
              strcmp(stub_codec, "shrinkler") == 0 ||
              strcmp(stub_codec, "lzsa") == 0 ||
              strcmp(stub_codec, "rnc") == 0 ||
              strcmp(stub_codec, "lzfse") == 0 ||
              strcmp(stub_codec, "csc") == 0 ||
              strcmp(stub_codec, "nz1") == 0 ||
              strcmp(stub_codec, "lzham") == 0 ||
              strcmp(stub_codec, "stcr") == 0 ||
              strcmp(stub_codec, "sc") == 0 ||
              strcmp(stub_codec, "density") == 0));
  int marker_len = strlen(marker);
  size_t packed_block_size = marker_len + sizeof(size_t) + sizeof(uint64_t) +
                             sizeof(int) +
                             (legacy_bcj_layout ? 0 : sizeof(int)) + comp_size;
  unsigned char *packed_block = malloc(packed_block_size);
  unsigned char *cursor = packed_block;

  memcpy(cursor, marker, marker_len);
  cursor += marker_len;

  memcpy(cursor, &actual_size, sizeof(size_t));
  cursor += sizeof(size_t);
  memcpy(cursor, &entry_offset, sizeof(uint64_t));
  cursor += sizeof(uint64_t);
  int c_size_int = (int)comp_size;
  memcpy(cursor, &c_size_int, sizeof(int));
  cursor += sizeof(int);
  if (!legacy_bcj_layout) {
    int f_size_int = (int)filtered_size;
    memcpy(cursor, &f_size_int, sizeof(int));
    cursor += sizeof(int);
  }

  memcpy(cursor, compressed_data, comp_size);

  fwrite(packed_block, 1, packed_block_size, out);
  free(packed_block);

  long final_size = ftell(out);
  fclose(out);
  chmod(final_bin, 0755);

  printf("[%s%s%s] Completed.\n", PK_OK, PK_SYM_OK, PK_RES);
  printf("[%s%s%s] Final size: %ld bytes (vs %zu original)\n", PK_ACC1,
         PK_SYM_DOT, PK_RES, final_size, actual_size);
  printf("[%s%s%s] Ratio: %.1f%%\n", PK_ACC2, PK_SYM_RARROW, PK_RES,
         ((double)final_size / (double)actual_size) * 100);

  free(stub_data_final);
  return 0;
}
