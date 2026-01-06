#include "depacker.h"
#include "zelf_packer.h"
#include <elf.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../stub/codec_marker.h"

// Decompressors
#include "../decompressors/apultra/apultra_decompress.h"
#include "../decompressors/brieflz/brieflz_decompress.h"
#include "../decompressors/density/density_minidec.h"
#include "../decompressors/doboz/doboz_decompress.h"
#include "../decompressors/exo/exo_decompress.h"
#include "../decompressors/lz4/codec_lz4.h"
#include "../decompressors/lzav/lzav_decompress.h"
#include "../decompressors/lzham/lzham_decompressor.h"
#include "../decompressors/lzma/minlzma_decompress.h"
#include "../decompressors/lzsa/libs/decompression/lzsa2_decompress.h"
#include "../decompressors/pp/powerpacker_decompress.h"
#include "../decompressors/quicklz/quicklz.h"
#include "../decompressors/shrinkler/codec_shrinkler.h"
#include "../decompressors/snappy/snappy_decompress.h"
#include "../decompressors/stonecracker/sc_decompress.h"
#include "../decompressors/rnc/rnc_minidec.h"
#include "../decompressors/lzfse/lzfse_stub.h"
#include "../decompressors/csc/csc_dec.h"
#include "../decompressors/nz1/nz1_decompressor.h"
#include "../decompressors/zstd/zstd_minidec.h"
#include "../decompressors/zx0/zx0_decompress.h"
#include "../decompressors/zx7b/zx7b_decompress.h"

// Filters
#include "../filters/bcj_x86_min.h"
#include "../filters/kanzi_exe_decode_x86_tiny.h"

#include "../others/help_colors.h"

// Hardcoded offsets for generated header to avoid include path issues
#ifndef STAGE0_HDR_ULEN_OFF
#define STAGE0_HDR_ULEN_OFF 0x4a
#define STAGE0_HDR_CLEN_OFF 0x4e
#define STAGE0_HDR_DST_OFF 0x52
#define STAGE0_HDR_BLOB_OFF 0x5a
#define STAGE0_HDR_FLAGS_OFF 0x62
#endif

// Params Block Magic: +zELF-PR
static const unsigned char ELF_PARAMS_MAGIC[8] = {'+', 'z', 'E', 'L',
                                                  'F', '-', 'P', 'R'};

 static const char *elfz_marker_suffixes[] = {
     "l4", "ap", "zx", "z0", "bz", "ex", "pp", "sn", "dz", "qz", "lv",
     "la", "sh", "sc", "ls", "zd", "de", "lz", "rn", "se", "cs", "nz",
     NULL};

typedef struct {
  uint64_t version;
  uint64_t virtual_start;
  uint64_t packed_data_vaddr;
  uint64_t salt;
  uint64_t pwd_obfhash;
} elfz_params_t;

 static inline uint64_t load_u64(const void *p) {
   uint64_t v;
   memcpy(&v, p, sizeof(v));
   return v;
 }

 static inline uint32_t load_u32(const void *p) {
   uint32_t v;
   memcpy(&v, p, sizeof(v));
   return v;
 }

 static inline int32_t load_s32(const void *p) {
   int32_t v;
   memcpy(&v, p, sizeof(v));
   return v;
 }

 static inline size_t load_size(const void *p) {
   size_t v;
   memcpy(&v, p, sizeof(v));
   return v;
 }

static int elf_pt_load_end_offset(const unsigned char *data, size_t size,
                                  size_t *out_end_off) {
   if (!out_end_off)
     return 0;
   *out_end_off = 0;
   if (!data || size < sizeof(Elf64_Ehdr))
     return 0;

   const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
   if (eh->e_ident[EI_MAG0] != ELFMAG0 || eh->e_ident[EI_MAG1] != ELFMAG1 ||
       eh->e_ident[EI_MAG2] != ELFMAG2 || eh->e_ident[EI_MAG3] != ELFMAG3)
     return 0;
   if (eh->e_ident[EI_CLASS] != ELFCLASS64)
     return 0;
   if (eh->e_phentsize != sizeof(Elf64_Phdr))
     return 0;

   if (eh->e_phoff == 0 || eh->e_phnum == 0)
     return 0;
   if ((size_t)eh->e_phoff > size)
     return 0;
   size_t phdrs_size = (size_t)eh->e_phnum * sizeof(Elf64_Phdr);
   if ((size_t)eh->e_phoff + phdrs_size > size)
     return 0;

   const Elf64_Phdr *ph = (const Elf64_Phdr *)(data + eh->e_phoff);
   size_t end_off = 0;
   for (uint16_t i = 0; i < eh->e_phnum; i++) {
     if (ph[i].p_type != PT_LOAD)
       continue;
     if ((size_t)ph[i].p_offset > size)
       continue;
     size_t seg_end = (size_t)ph[i].p_offset + (size_t)ph[i].p_filesz;
     if (seg_end > size)
       continue;
     if (seg_end > end_off)
       end_off = seg_end;
   }

   if (end_off == 0 || end_off >= size)
     return 0;
   *out_end_off = end_off;
   return 1;
 }

static int find_elfz_params(const unsigned char *data, size_t size,
                            elfz_params_t *params) {
  // Search for magic
  // Params block is small, typically in .rodata or patched stub.
  // We scan the whole file (or first X MB?).
  // Given we map the whole file, scanning it all is fine for typical binaries.
  // Optimization: stub is usually at the beginning.

  size_t pt_load_end_off = 0;
  if (elf_pt_load_end_offset(data, size, &pt_load_end_off)) {
    for (size_t i = pt_load_end_off; i + 48 <= size; i++) {
      if (memcmp(data + i, ELF_PARAMS_MAGIC, 8) == 0) {
        // Found magic
        params->version = load_u64(data + i + 8);
        params->virtual_start = load_u64(data + i + 16);
        params->packed_data_vaddr = load_u64(data + i + 24);

        // Version check for v2 fields
        int ver = (int)(params->version & 0xFF);
        if (ver >= 2) {
          params->salt = load_u64(data + i + 32);
          params->pwd_obfhash = load_u64(data + i + 40);
        } else {
          params->salt = 0;
          params->pwd_obfhash = 0;
        }
        return 1;
      }
    }
  }

  // Fallback to whole-file scan
  for (size_t i = 0; i + 48 <= size; i++) {
    if (memcmp(data + i, ELF_PARAMS_MAGIC, 8) == 0) {
      // Found magic
      params->version = load_u64(data + i + 8);
      params->virtual_start = load_u64(data + i + 16);
      params->packed_data_vaddr = load_u64(data + i + 24);

      // Version check for v2 fields
      int ver = (int)(params->version & 0xFF);
      if (ver >= 2) {
        params->salt = load_u64(data + i + 32);
        params->pwd_obfhash = load_u64(data + i + 40);
      } else {
        params->salt = 0;
        params->pwd_obfhash = 0;
      }
      return 1;
    }
  }
  return 0;
}

 static int find_payload_marker_offset_best(const unsigned char *data,
                                            size_t size, size_t start,
                                            size_t *payload_offset);

static int find_payload_marker_offset(const unsigned char *data, size_t size,
                                      size_t *payload_offset) {
  if (!data || !payload_offset)
    return 0;
  if (size < 32)
    return 0;

  size_t pt_load_end_off = 0;
  if (elf_pt_load_end_offset(data, size, &pt_load_end_off)) {
    if (find_payload_marker_offset_best(data, size, pt_load_end_off,
                                        payload_offset))
      return 1;
    return find_payload_marker_offset_best(data, size, 0, payload_offset);
  }

  // Fallback to whole-file scan
  return find_payload_marker_offset_best(data, size, 0, payload_offset);
}

static int find_payload_marker_offset_best(const unsigned char *data,
                                           size_t size, size_t start,
                                           size_t *payload_offset) {
  if (!data || !payload_offset)
    return 0;
  if (size < 32)
    return 0;
  if (start >= size)
    return 0;

  // Select the marker candidate whose compressed block end is closest to EOF.
  // This avoids false positives when the marker bytes occur inside compressed
  // payload data or inside embedded stubs.
  size_t best_off = (size_t)-1;
  size_t best_tail = (size_t)-1;

  for (size_t i = start; i + 26 <= size; i++) {
    if (data[i] == 'z' && data[i + 1] == 'E' && data[i + 2] == 'L' &&
        data[i + 3] == 'F') {
      for (int k = 0; elfz_marker_suffixes[k] != NULL; k++) {
        if (data[i + 4] == (unsigned char)elfz_marker_suffixes[k][0] &&
            data[i + 5] == (unsigned char)elfz_marker_suffixes[k][1]) {
          const unsigned char *p = data + i + 6;
          size_t orig_size = load_size(p);
          p += 8;
          p += 8;
          int comp_size = (int)load_s32(p);

          if (orig_size == 0)
            continue;
          if (orig_size > (size_t)(1024u * 1024u * 1024u))
            continue;
          if (comp_size <= 0)
            continue;
          size_t remaining = size - i;
          if ((size_t)comp_size > remaining - 26)
            continue;

          size_t tail = remaining - 26 - (size_t)comp_size;
          if (tail < best_tail) {
            best_tail = tail;
            best_off = i;
            if (tail == 0) {
              *payload_offset = best_off;
              return 1;
            }
          }
        }
      }
    }
  }
  if (best_off == (size_t)-1)
    return 0;
  *payload_offset = best_off;
  return 1;
}

static int vaddr_to_offset(const unsigned char *data, size_t size,
                           uint64_t vaddr, size_t *offset_out) {
  if (size < sizeof(Elf64_Ehdr))
    return 0;
  const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;

  // Check ELF magic
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0)
    return 0;

  if (ehdr->e_phoff + ehdr->e_phnum * ehdr->e_phentsize > size)
    return 0;

  const Elf64_Phdr *phdr = (const Elf64_Phdr *)(data + ehdr->e_phoff);

  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
      uint64_t vstart = phdr[i].p_vaddr;
      uint64_t vend = vstart + phdr[i].p_filesz;
      if (vaddr >= vstart && vaddr < vend) {
        *offset_out = phdr[i].p_offset + (vaddr - vstart);
        return 1;
      }
    }
  }
  return 0;
}

static int is_kanzi_stub(const unsigned char *data, size_t size) {
  if (!data || size < sizeof(Elf64_Ehdr))
    return 0;
  const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;

  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0)
    return 0;

  for (size_t j = 0; j + 5 < size; j++) {
    if (data[j] == 0x35 && data[j + 1] == 0xF0 && data[j + 2] == 0xF0 &&
        data[j + 3] == 0xF0 && data[j + 4] == 0xF0) {
      return 1;
    }
    if (j + 6 < size && data[j] == 0x81 && (data[j + 1] & 0xF8) == 0xF0 &&
        data[j + 2] == 0xF0 && data[j + 3] == 0xF0 && data[j + 4] == 0xF0 &&
        data[j + 5] == 0xF0) {
      return 1;
    }
    if (data[j] == 0x48 && data[j + 1] == 0x35 && data[j + 2] == 0xF0 &&
        data[j + 3] == 0xF0 && data[j + 4] == 0xF0 && data[j + 5] == 0xF0) {
      return 1;
    }
  }
  return 0;
}

static int stage0_read_flags(const unsigned char *data, size_t size,
                             uint8_t *out_flags) {
  if (!data || !out_flags)
    return 0;
  if (size < sizeof(Elf64_Ehdr))
    return 0;

  const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;
  size_t entry_off = 0;
  if (!vaddr_to_offset(data, size, ehdr->e_entry, &entry_off))
    return 0;
  if (entry_off >= size)
    return 0;

  const unsigned char *stub = data + entry_off;
  size_t available = size - entry_off;
  if (available <= (size_t)STAGE0_HDR_FLAGS_OFF)
    return 0;

  uint32_t ulen = load_u32(stub + STAGE0_HDR_ULEN_OFF);
  uint32_t clen = load_u32(stub + STAGE0_HDR_CLEN_OFF);
  uint64_t blob_off = load_u64(stub + STAGE0_HDR_BLOB_OFF);

  if (ulen == 0 || ulen > (uint32_t)(1024u * 1024u))
    return 0;
  if (clen == 0)
    return 0;
  if (blob_off > (uint64_t)available)
    return 0;
  if ((uint64_t)clen > (uint64_t)available)
    return 0;
  if (blob_off + (uint64_t)clen > (uint64_t)available)
    return 0;

  *out_flags = stub[STAGE0_HDR_FLAGS_OFF];
  return 1;
}

static int write_file(const char *path, const void *data, size_t size) {
  if (!path || !*path) {
    errno = EINVAL;
    return -1;
  }
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;
  if (fwrite(data, 1, size, f) != size) {
    fclose(f);
    return -1;
  }
  fclose(f);
  return 0;
}

int elfz_depack(const char *input_path, const char *output_path) {
  char default_out[4096];
  if (!output_path || !*output_path) {
    if (input_path && *input_path && strcmp(input_path, "-") != 0) {
      size_t n = strlen(input_path);
      if (n + 9 < sizeof(default_out)) {
        memcpy(default_out, input_path, n);
        memcpy(default_out + n, ".unpacked", 10);
        output_path = default_out;
      } else {
        memcpy(default_out, "unpacked.elf", 13);
        output_path = default_out;
      }
    } else {
      memcpy(default_out, "unpacked.elf", 13);
      output_path = default_out;
    }
  }

  int fd = open(input_path, O_RDONLY);
  if (fd < 0) {
    perror("open input");
    return 1;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    perror("fstat");
    close(fd);
    return 1;
  }

  size_t size = st.st_size;
  unsigned char *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return 1;
  }

  // 1. Find Params
  elfz_params_t params;
  int params_found = find_elfz_params(data, size, &params);

  if (!params_found) {
    memset(&params, 0, sizeof(params));
    // Fallback to marker scan if params block not found (common for dynamic
    // binaries without password)
  } else {
    printf("[%s%s%s] ELFZ Packed Binary detected (Params found)\n", PK_INFO,
           PK_SYM_INFO, PK_RES);
    printf("[%s%s%s] Version: %ld, Flags: 0x%02x\n", PK_INFO, PK_SYM_INFO,
           PK_RES, params.version & 0xFF, (int)((params.version >> 8) & 0xFF));
    printf("[%s%s%s] Packed Data VAddr: 0x%lx\n", PK_INFO, PK_SYM_INFO, PK_RES,
           params.packed_data_vaddr);
  }

  // 2. Locate Packed Data Payload
  size_t payload_offset = 0;
  if (params.packed_data_vaddr == 0) {
    // Dynamic binary (PIE) or missing params: address not patched/known.
    // Scan for marker.
    if (!find_payload_marker_offset(data, size, &payload_offset)) {
      fprintf(stderr,
              "Error: Could not find compressed payload marker in file\n");
      munmap(data, size);
      close(fd);
      return 1;
    }
  } else {
    if (!vaddr_to_offset(data, size, params.packed_data_vaddr,
                         &payload_offset)) {
      // VAddr mapping failed - this can happen with stage0 codecs (ZSTD,
      // Density) where the vaddr was patched before stage0 compression.
      // Fallback to marker scan.
      printf("[%s%s%s] VAddr mapping failed, falling back to marker scan\n",
             PK_INFO, PK_SYM_INFO, PK_RES);
      if (!find_payload_marker_offset(data, size, &payload_offset)) {
        fprintf(stderr, "Error: Could not map VAddr nor find marker in file\n");
        munmap(data, size);
        close(fd);
        return 1;
      }
    }
  }

  if (payload_offset >= size) {
    fprintf(stderr, "Error: Invalid payload offset\n");
    munmap(data, size);
    close(fd);
    return 1;
  }

  const unsigned char *payload = data + payload_offset;
  size_t remaining = size - payload_offset;

  // 3. Read Header
  // Header: Marker(6) + OrigSize(8) + EntryOffset(8) + CompSize(4) [+
  // FilteredSize(4)]
  if (remaining < 26) {
    fprintf(stderr, "Error: Payload too small for header\n");
    munmap(data, size);
    close(fd);
    return 1;
  }

  char marker[7];
  memcpy(marker, payload, 6);
  marker[6] = '\0';
  const unsigned char *p = payload + 6;

  size_t orig_size = load_size(p);
  p += 8;
  uint64_t entry_offset = load_u64(p);
  p += 8;
  int comp_size = (int)load_s32(p);
  p += 4;

  int is_bcj_flag = (params.version >> 8) & 1;
  int is_legacy_layout = 0;

  // Detect Codec
  const char *codec_name = "Unknown";
  if (strcmp(marker, "zELFl4") == 0)
    codec_name = "LZ4";
  else if (strcmp(marker, "zELFap") == 0)
    codec_name = "Apultra";
  else if (strcmp(marker, "zELFzx") == 0)
    codec_name = "ZX7B";
  else if (strcmp(marker, "zELFz0") == 0)
    codec_name = "ZX0";
  else if (strcmp(marker, "zELFbz") == 0)
    codec_name = "BriefLZ";
  else if (strcmp(marker, "zELFex") == 0)
    codec_name = "Exo";
  else if (strcmp(marker, "zELFpp") == 0)
    codec_name = "PowerPacker";
  else if (strcmp(marker, "zELFsn") == 0)
    codec_name = "Snappy";
  else if (strcmp(marker, "zELFdz") == 0)
    codec_name = "Doboz";
  else if (strcmp(marker, "zELFqz") == 0)
    codec_name = "QuickLZ";
  else if (strcmp(marker, "zELFlv") == 0)
    codec_name = "LZAV";
  else if (strcmp(marker, "zELFla") == 0)
    codec_name = "LZMA";
  else if (strcmp(marker, "zELFsh") == 0)
    codec_name = "Shrinkler";
  else if (strcmp(marker, "zELFsc") == 0)
    codec_name = "StoneCracker";
  else if (strcmp(marker, "zELFls") == 0)
    codec_name = "LZSA2";
  else if (strcmp(marker, "zELFzd") == 0) {
    codec_name = "Zstd";
    is_legacy_layout = 1; // Zstd keeps legacy layout (no filtered_size), but
                          // may store a BCJ flag
  } else if (strcmp(marker, "zELFde") == 0) {
    codec_name = "Density";
    is_legacy_layout = 1; // Density uses legacy layout like ZSTD
  } else if (strcmp(marker, "zELFlz") == 0) {
    codec_name = "LZHAM";
  } else if (strcmp(marker, "zELFrn") == 0) {
    codec_name = "RNC";
  } else if (strcmp(marker, "zELFse") == 0) {
    codec_name = "LZFSE";
  } else if (strcmp(marker, "zELFcs") == 0) {
    codec_name = "CSC";
  } else if (strcmp(marker, "zELFnz") == 0) {
    codec_name = "NZ1";
  } else {
    fprintf(stderr, "Error: Unknown marker %s\n", marker);
    munmap(data, size);
    close(fd);
    return 1;
  }

  printf("[%s%s%s] Codec: %s\n", PK_INFO, PK_SYM_INFO, PK_RES, codec_name);
  printf("[%s%s%s] Original Size: %zu\n", PK_INFO, PK_SYM_INFO, PK_RES,
         orig_size);
  printf("[%s%s%s] Compressed Size: %d\n", PK_INFO, PK_SYM_INFO, PK_RES,
         comp_size);

  // Detect Kanzi stub by scanning for Kanzi-specific code patterns.
  // This is deterministic and reliable, no heuristics needed.
  int is_kanzi = is_kanzi_stub(data, size);
  printf("[%s%s%s] Initial Kanzi Stub Detection: %d\n", PK_INFO, PK_SYM_INFO,
         PK_RES, is_kanzi);

  // Stage0 wrapper: for Zstd (all) and Density (dynamic only), the wrapper
  // carries an explicit filter flags byte. Use it instead of any heuristics.
  {
    int is_density = (strcmp(codec_name, "Density") == 0);
    int is_zstd = (strcmp(codec_name, "Zstd") == 0);
    int allow_stage0 = 0;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    if (is_zstd)
      allow_stage0 = 1;
    else if (is_density && eh->e_type == ET_DYN)
      allow_stage0 = 1;

    if (allow_stage0) {
      uint8_t s0_flags = 0;
      if (stage0_read_flags(data, size, &s0_flags)) {
        is_bcj_flag = (s0_flags & 1u) ? 1 : 0;
        is_kanzi = (s0_flags & 2u) ? 1 : 0;
      }
    }
  }

  // If params missing, default to legacy layout.
  // Modern layout (with filtered_size) is only used for Kanzi filter.
  // BCJ/None filters always use legacy layout.
  if (!params_found && !is_legacy_layout) {
    // No params found means dynamic binary or old format.
    // Use stub detection to determine layout.
    is_legacy_layout = is_kanzi ? 0 : 1;
  } else if (is_bcj_flag) {
    is_legacy_layout = 1; // BCJ implies legacy layout (no filtered_size)
  } else if (!is_kanzi) {
    // Not a Kanzi stub (None filter uses BCJ stub), use legacy layout
    is_legacy_layout = 1;
  }

  size_t filtered_size = orig_size; // Default if not present
  if (!is_legacy_layout && is_kanzi) {
    if ((size_t)(p - payload) + 4 > remaining) {
      fprintf(stderr, "Error: Truncated header (filtered_size)\n");
      munmap(data, size);
      close(fd);
      return 1;
    }
    filtered_size = (size_t)(unsigned)load_u32(p);
    p += 4;
    printf("[%s%s%s] Filtered Size (Header): %zu\n", PK_INFO, PK_SYM_INFO,
           PK_RES, filtered_size);
  }

  const unsigned char *comp_data = p;

  // Validate remaining size
  size_t header_len = p - payload;
  if (remaining < header_len + comp_size) {
    fprintf(stderr, "Error: Truncated compressed data\n");
    munmap(data, size);
    close(fd);
    return 1;
  }

  // 4. Decompress
  // Special case for Shrinkler: needs extra margin buffer
  size_t alloc_size = is_legacy_layout ? orig_size : filtered_size;

  // For Kanzi in legacy layout (ZSTD/Density), we need more space than
  // orig_size because Kanzi output is larger than original.
  // Density also benefits from a larger safety margin.
  if ((is_kanzi && is_legacy_layout) || strcmp(codec_name, "Density") == 0) {
    size_t margin = (orig_size / 8) + 4096;
    alloc_size = orig_size + margin; // Safe margin
    printf(
        "[%s%s%s] Kanzi/Density: Increasing alloc_size to %zu (Margin: %zu)\n",
        PK_INFO, PK_SYM_INFO, PK_RES, alloc_size, margin);
  }
  if (strcmp(codec_name, "Shrinkler") == 0) {
    // Peek margin from Shrinkler header (offset 16, BE32)
    if (remaining >= (size_t)(p - payload) + 20) { // Check bounds
      const unsigned char *h = comp_data;
      unsigned margin = ((unsigned)h[16] << 24) | ((unsigned)h[17] << 16) |
                        ((unsigned)h[18] << 8) | (unsigned)h[19];
      alloc_size += margin + 256; // Add margin + safety
      printf("[%s%s%s] Shrinkler margin detected: %u (allocating %zu)\n",
             PK_INFO, PK_SYM_INFO, PK_RES, margin, alloc_size);
    }
  }

  printf("[%s%s%s] Final Alloc Size: %zu (Cap: %d)\n", PK_INFO, PK_SYM_INFO,
         PK_RES, alloc_size, (int)alloc_size);

  unsigned char *decomp_buf = malloc(alloc_size);
  if (!decomp_buf) {
    perror("malloc decomp");
    munmap(data, size);
    close(fd);
    return 1;
  }

  // Use filtered_size as capacity if available, otherwise orig_size
  // (In legacy BCJ, filtered_size is effectively orig_size/allocated buffer)
  // For Shrinkler, pass the full allocated size as capacity
  int dst_cap = (int)alloc_size;

  int ret = -1;

  if (strcmp(codec_name, "LZ4") == 0) {
    ret = lz4_decompress((const char *)comp_data, (char *)decomp_buf, comp_size,
                         dst_cap);
  } else if (strcmp(codec_name, "Apultra") == 0) {
    size_t r = apultra_decompress(comp_data, decomp_buf, (size_t)comp_size,
                                  (size_t)dst_cap, 0, 0);
    ret = r ? (int)r : -1;
  } else if (strcmp(codec_name, "ZX7B") == 0) {
    ret = zx7b_decompress(comp_data, comp_size, decomp_buf, dst_cap);
  } else if (strcmp(codec_name, "ZX0") == 0) {
    ret = zx0_decompress_to(comp_data, comp_size, decomp_buf, dst_cap);
  } else if (strcmp(codec_name, "BriefLZ") == 0) {
    ret = brieflz_decompress_to(comp_data, comp_size, decomp_buf, dst_cap);
  } else if (strcmp(codec_name, "Exo") == 0) {
    ret = exo_decompress(comp_data, comp_size, decomp_buf, dst_cap);
  } else if (strcmp(codec_name, "PowerPacker") == 0) {
    ret = powerpacker_decompress(comp_data, (unsigned int)comp_size, decomp_buf,
                                 (unsigned int)dst_cap);
  } else if (strcmp(codec_name, "Snappy") == 0) {
    size_t r = snappy_decompress_simple(comp_data, (size_t)comp_size,
                                        decomp_buf, (size_t)dst_cap);
    ret = r ? (int)r : -1;
  } else if (strcmp(codec_name, "Doboz") == 0) {
    ret = doboz_decompress((const char *)comp_data, (size_t)comp_size,
                           (char *)decomp_buf, (size_t)dst_cap);
  } else if (strcmp(codec_name, "QuickLZ") == 0) {
    qlz_state_decompress state = {0};
    size_t r =
        qlz_decompress((const char *)comp_data, (char *)decomp_buf, &state);
    ret = r > 0 ? (int)r : -1;
  } else if (strcmp(codec_name, "LZAV") == 0) {
    ret = lzav_decompress_c((const char *)comp_data, (char *)decomp_buf,
                            comp_size, dst_cap);
  } else if (strcmp(codec_name, "LZMA") == 0) {
    ret = minlzma_decompress_c((const char *)comp_data, (char *)decomp_buf,
                               comp_size, dst_cap);
  } else if (strcmp(codec_name, "Zstd") == 0) {
    ret = zstd_decompress_c(comp_data, (size_t)comp_size, decomp_buf,
                            (size_t)alloc_size);
    printf("[%s%s%s] Zstd Decompress Ret: %d\n", PK_INFO, PK_SYM_INFO, PK_RES,
           ret);
  } else if (strcmp(codec_name, "Shrinkler") == 0) {
    ret = shrinkler_decompress_c((const char *)comp_data, (char *)decomp_buf,
                                 comp_size, dst_cap);
  } else if (strcmp(codec_name, "StoneCracker") == 0) {
    size_t r = sc_decompress(comp_data, (size_t)comp_size, decomp_buf,
                             (size_t)dst_cap);
    ret = r ? (int)r : -1;
  } else if (strcmp(codec_name, "LZSA2") == 0) {
    ret = lzsa2_decompress(comp_data, decomp_buf, comp_size, dst_cap, 0);
  } else if (strcmp(codec_name, "Density") == 0) {
    ret = density_decompress_c(comp_data, (size_t)comp_size, decomp_buf,
                               (size_t)alloc_size);
    printf("[%s%s%s] Density Decompress Ret: %d\n", PK_INFO, PK_SYM_INFO,
           PK_RES, ret);
  } else if (strcmp(codec_name, "RNC") == 0) {
    ret = rnc_decompress_to(comp_data, comp_size, decomp_buf, dst_cap);
  } else if (strcmp(codec_name, "LZFSE") == 0) {
    void *scratch = malloc(lzfse_decode_scratch_size());
    if (!scratch) {
      fprintf(stderr, "Error: Failed to allocate LZFSE scratch\n");
      free(decomp_buf);
      munmap(data, size);
      close(fd);
      return 1;
    }
    size_t r = lzfse_decode_buffer(decomp_buf, (size_t)dst_cap, comp_data,
                                   (size_t)comp_size, scratch);
    free(scratch);
    ret = r ? (int)r : -1;
  } else if (strcmp(codec_name, "CSC") == 0) {
    intptr_t ws = CSC_Dec_GetWorkspaceSize(comp_data, (size_t)comp_size);
    if (ws < 0) {
      ret = -1;
    } else {
      void *workspace = malloc((size_t)ws);
      if (!workspace) {
        fprintf(stderr, "Error: Failed to allocate CSC workspace\n");
        free(decomp_buf);
        munmap(data, size);
        close(fd);
        return 1;
      }
      intptr_t r = CSC_Dec_Decompress(comp_data, (size_t)comp_size, decomp_buf,
                                      (size_t)dst_cap, workspace);
      free(workspace);
      ret = (r >= 0) ? (int)r : -1;
    }
  } else if (strcmp(codec_name, "NZ1") == 0) {
    ret = nanozip_decompress((const void *)comp_data, comp_size, (void *)decomp_buf,
                             dst_cap);
  } else if (strcmp(codec_name, "LZHAM") == 0) {
    // LZHAM requires workspace
    size_t ws_sz = 4 * 1024 * 1024; // 4MB workspace
    void *workspace = malloc(ws_sz);
    if (!workspace) {
      fprintf(stderr, "Error: Failed to allocate LZHAM workspace\n");
      free(decomp_buf);
      munmap(data, size);
      close(fd);
      return 1;
    }
    lzhamd_params_t params;
    lzhamd_params_init_default(&params, workspace, ws_sz, 29);
    size_t dst_len = (size_t)dst_cap;
    lzhamd_status_t status = lzhamd_decompress_memory(
        &params, decomp_buf, &dst_len, comp_data, (size_t)comp_size);
    free(workspace);
    ret = (status == LZHAMD_STATUS_SUCCESS) ? (int)dst_len : -1;
  }

  if (ret < 0) {
    fprintf(stderr, "Error: Decompression failed\n");
    free(decomp_buf);
    munmap(data, size);
    close(fd);
    return 1;
  }

  // 5. Unfilter
  unsigned char *final_buf = decomp_buf;
  size_t final_size =
      (size_t)ret; // Typically == dst_cap (filtered_size or orig_size)
  int free_final =
      1; // Should we free final_buf? (it points to decomp_buf initially)

  if (is_bcj_flag) {
    printf("[%s%s%s] Filter: BCJ detected (Flag)\n", PK_INFO, PK_SYM_INFO,
           PK_RES);
    size_t done = bcj_x86_decode(decomp_buf, final_size, 0);
    /* BCJ decoder stops 5 bytes before end safely, so size-done <= 5 is normal
     */
    if (done < final_size - 5) {
      fprintf(stderr,
              "Warning: BCJ decode processed different size: %zu vs %zu\n",
              done, final_size);
    }
    // BCJ is in-place
    // Final result is in decomp_buf
  } else {
    // Check for Kanzi
    if (final_size >= 9 && decomp_buf[0] == 0x40) {
      printf("[%s%s%s] Filter: Kanzi detected (Header)\n", PK_INFO, PK_SYM_INFO,
             PK_RES);
      unsigned char *unfiltered = malloc(orig_size);
      if (!unfiltered) {
        perror("malloc unfilter");
        free(decomp_buf);
        munmap(data, size);
        close(fd);
        return 1;
      }
      int processed = 0;
      int uf_sz = kanzi_exe_unfilter_x86(
          decomp_buf, (int)final_size, unfiltered, (int)orig_size, &processed);
      if (uf_sz > 0 && processed == (int)final_size) {
        free(decomp_buf);  // Free compressed/filtered buffer
        decomp_buf = NULL; // To avoid double free confusion
        final_buf = unfiltered;
        final_size = (size_t)uf_sz;
        // Check size match
        if (final_size != orig_size) {
          printf("Warning: Unfiltered size %zu != Original Size %zu\n",
                 final_size, orig_size);
        }
      } else {
        fprintf(stderr, "Warning: Kanzi unfilter failed, using raw data\n");
        free(unfiltered);
        // Keep decomp_buf as final_buf
      }
    } else {
      printf("[%s%s%s] Filter: None detected\n", PK_INFO, PK_SYM_INFO, PK_RES);
    }
  }

  // 6. Write Output
  if (write_file(output_path, final_buf, final_size) != 0) {
    fprintf(stderr, "Error: Failed to write output file: %s\n",
            strerror(errno));
    if (free_final && final_buf)
      free(final_buf);
    else if (decomp_buf)
      free(decomp_buf); // Fallback if final_buf wasn't allocated separately
    munmap(data, size);
    close(fd);
    return 1;
  }

  // Make executable
  if (chmod(output_path, 0755) != 0) {
    perror("chmod");
    // Not fatal, just warning
  }

  printf("[%s%s%s] Unpacked to %s\n", PK_OK, PK_SYM_OK, PK_RES, output_path);

  if (free_final && final_buf)
    free(final_buf);
  // Note: if final_buf == decomp_buf, it's freed.
  // If final_buf was reallocated (Kanzi), decomp_buf was already freed.

  munmap(data, size);
  close(fd);
  return 0;
}
