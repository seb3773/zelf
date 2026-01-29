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
#include "../filters/bcj_arm64_min.h"
#include "../filters/kanzi_exe_decode_x86_tiny.h"
#include "../filters/kanzi_exe_decode_arm64_tiny.h"

#include "../others/help_colors.h"

// Stage0 header offsets (generated at build time, with fallback values)
#include "stage0_x86_64/stage0_nrv2b_le32_offsets_x86_64.h"
#include "stage0_arm64/stage0_nrv2b_le32_offsets_arm64.h"

extern int g_verbose;

// Complexity: O(n) over buffer size.
// Dependencies: none.
// Assumptions: input pointer valid for n bytes.
static inline uint64_t fnv1a64_hash(const void *p, size_t n) {
  const uint8_t *s = (const uint8_t *)p;
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; ++i) {
    h ^= (uint64_t)s[i];
    h *= 1099511628211ull;
  }
  return h;
}

#ifndef STAGE0_X86_HDR_ULEN_OFF
#define STAGE0_X86_HDR_ULEN_OFF 0x4a
#define STAGE0_X86_HDR_CLEN_OFF 0x4e
#define STAGE0_X86_HDR_DST_OFF 0x52
#define STAGE0_X86_HDR_BLOB_OFF 0x5a
#define STAGE0_X86_HDR_FLAGS_OFF 0x62
#endif

#ifndef STAGE0_ARM64_HDR_ULEN_OFF
#define STAGE0_ARM64_HDR_ULEN_OFF 0x4c
#define STAGE0_ARM64_HDR_CLEN_OFF 0x50
#define STAGE0_ARM64_HDR_DST_OFF 0x58
#define STAGE0_ARM64_HDR_BLOB_OFF 0x60
#define STAGE0_ARM64_HDR_FLAGS_OFF 0x68
#endif

typedef struct {
  size_t ulen_off;
  size_t clen_off;
  size_t dst_off;
  size_t blob_off;
  size_t flags_off;
} stage0_hdr_desc_t;

static inline stage0_hdr_desc_t stage0_hdr_get_desc(Elf64_Half machine) {
  stage0_hdr_desc_t d;
  if (machine == EM_AARCH64) {
    d.ulen_off = (size_t)STAGE0_ARM64_HDR_ULEN_OFF;
    d.clen_off = (size_t)STAGE0_ARM64_HDR_CLEN_OFF;
    d.dst_off = (size_t)STAGE0_ARM64_HDR_DST_OFF;
    d.blob_off = (size_t)STAGE0_ARM64_HDR_BLOB_OFF;
    d.flags_off = (size_t)STAGE0_ARM64_HDR_FLAGS_OFF;
  } else {
    d.ulen_off = (size_t)STAGE0_X86_HDR_ULEN_OFF;
    d.clen_off = (size_t)STAGE0_X86_HDR_CLEN_OFF;
    d.dst_off = (size_t)STAGE0_X86_HDR_DST_OFF;
    d.blob_off = (size_t)STAGE0_X86_HDR_BLOB_OFF;
    d.flags_off = (size_t)STAGE0_X86_HDR_FLAGS_OFF;
  }
  return d;
}

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

static int find_payload_marker_offset_density_kanzi_arm64_best(const unsigned char *data,
                                                              size_t size,
                                                              size_t *payload_offset) {
  if (!data || !payload_offset)
    return 0;
  if (size < sizeof(Elf64_Ehdr))
    return 0;

  const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
  if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0)
    return 0;
  if (eh->e_machine != EM_AARCH64)
    return 0;

  size_t pt_load_end_off = 0;
  if (!elf_pt_load_end_offset(data, size, &pt_load_end_off))
    return 0;

  const size_t window = (size_t)(64u << 20);
  size_t start = 0;
  if (pt_load_end_off > window)
    start = pt_load_end_off - window;

  size_t best_off = (size_t)-1;
  size_t best_tail = (size_t)-1;

  for (size_t i = start; i + 30u <= pt_load_end_off; i++) {
    if (data[i] != 'z' || data[i + 1] != 'E' || data[i + 2] != 'L' ||
        data[i + 3] != 'F' || data[i + 4] != 'd' || data[i + 5] != 'e')
      continue;

    const unsigned char *p = data + i + 6;
    size_t orig_size = load_size(p);
    p += 8;
    p += 8;
    int comp_size = (int)load_s32(p);
    p += 4;
    uint32_t fs_u32 = load_u32(p);
    size_t fs = (size_t)fs_u32;

    if ((uint64_t)orig_size <= 1000ull || orig_size > (size_t)0x20000000u)
      continue;
    if (comp_size <= 100 || (uint32_t)comp_size > 0x20000000u)
      continue;
    if (fs == 0 || fs > (size_t)0x20000000u)
      continue;
    if (fs < orig_size || (fs - orig_size) > 256u)
      continue;

    size_t end = i + 30u + (size_t)(uint32_t)comp_size;
    if (end > pt_load_end_off)
      continue;

    size_t tail = pt_load_end_off - end;
    if (tail < best_tail) {
      best_tail = tail;
      best_off = i;
      if (tail == 0)
        break;
    }
  }

  if (best_off == (size_t)-1)
    return 0;
  *payload_offset = best_off;
  return 1;
}

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

          const size_t hdr_legacy = 26u;
          const size_t hdr_modern = 30u;

          if (remaining <= hdr_legacy)
            continue;
          if ((size_t)comp_size > remaining - hdr_legacy)
            continue;

          size_t cand_tail = remaining - hdr_legacy - (size_t)comp_size;

          if (remaining > hdr_modern && (size_t)comp_size <= remaining - hdr_modern) {
            uint32_t fs_u32 = load_u32(data + i + hdr_legacy);
            size_t fs = (size_t)fs_u32;
            if (fs > 0 && fs <= (size_t)0x20000000u) {
              if (fs >= orig_size && (fs - orig_size) <= 256u) {
                size_t tail2 = remaining - hdr_modern - (size_t)comp_size;
                if (tail2 < cand_tail)
                  cand_tail = tail2;
              }
            }
          }

          if (cand_tail < best_tail) {
            best_tail = cand_tail;
            best_off = i;
            if (cand_tail == 0) {
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
  const stage0_hdr_desc_t d = stage0_hdr_get_desc(ehdr->e_machine);
  size_t entry_off = 0;
  if (!vaddr_to_offset(data, size, ehdr->e_entry, &entry_off))
    return 0;
  if (entry_off >= size)
    return 0;

  const unsigned char *stub = data + entry_off;
  size_t available = size - entry_off;
  if (available <= d.flags_off)
    return 0;

  uint32_t ulen = load_u32(stub + d.ulen_off);
  uint32_t clen = load_u32(stub + d.clen_off);
  uint64_t blob_off = load_u64(stub + d.blob_off);

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

  *out_flags = stub[d.flags_off];
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
  int used_marker_scan = 0;
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
    used_marker_scan = 1;
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
      used_marker_scan = 1;
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
  int is_kanzi_flag = ((params.version >> 8) & 2) ? 1 : 0;
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
    // Layout depends on filter:
    // - None/BCJ => legacy layout (no filtered_size)
    // - KanziEXE => modern layout (filtered_size present)
    // Decide later after reading params / stage0 flags.
    is_legacy_layout = 0;
  } else if (strcmp(marker, "zELFde") == 0) {
    codec_name = "Density";
    // Layout depends on filter (same rule as ZSTD). Decide later.
    is_legacy_layout = 0;
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

  // Detect Kanzi stub by scanning for Kanzi-specific code patterns.
  // This is deterministic and reliable, no heuristics needed.
  int is_kanzi_stub_scan = is_kanzi_stub(data, size);
  int is_kanzi = params_found ? is_kanzi_flag : is_kanzi_stub_scan;
  printf("[%s%s%s] Initial Kanzi Stub Detection: %d\n", PK_INFO, PK_SYM_INFO,
         PK_RES, is_kanzi_stub_scan);

  // Stage0 wrapper: for Zstd (all) and Density (dynamic only), the wrapper
  // carries an explicit filter flags byte. Use it instead of any heuristics.
  {
    int is_density = (strcmp(codec_name, "Density") == 0);
    int is_zstd = (strcmp(codec_name, "Zstd") == 0);
    int allow_stage0 = 0;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    if (is_zstd)
      allow_stage0 = 1;
    else if (is_density && (eh->e_type == ET_DYN || eh->e_type == ET_EXEC))
      allow_stage0 = 1;

    if (allow_stage0) {
      uint8_t s0_flags = 0;
      if (stage0_read_flags(data, size, &s0_flags)) {
        is_bcj_flag = (s0_flags & 1u) ? 1 : 0;
        is_kanzi = (s0_flags & 2u) ? 1 : 0;
      }
    }
  }

  // Strictly scoped fix: ARM64 dyn(PIE) + Density + KanziEXE + marker-scan fallback.
  // In stage0 layouts, packed_data_vaddr may not map, and a generic marker scan
  // can pick a false positive inside embedded/stage0 data.
  if (used_marker_scan) {
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    if (eh->e_machine == EM_AARCH64 && eh->e_type == ET_DYN &&
        strcmp(marker, "zELFde") == 0 && is_kanzi) {
      size_t po2 = 0;
      if (find_payload_marker_offset_density_kanzi_arm64_best(data, size, &po2) &&
          po2 != payload_offset) {
        payload_offset = po2;
        payload = data + payload_offset;
        remaining = size - payload_offset;

        if (remaining < 30u) {
          fprintf(stderr, "Error: Payload too small for header\n");
          munmap(data, size);
          close(fd);
          return 1;
        }

        memcpy(marker, payload, 6);
        marker[6] = '\0';
        p = payload + 6;

        orig_size = load_size(p);
        p += 8;
        entry_offset = load_u64(p);
        p += 8;
        comp_size = (int)load_s32(p);
        p += 4;

        codec_name = "Density";
        is_legacy_layout = 0;
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

  // Layout compatibility:
  // - ZSTD payload header is legacy on all targets (no filtered_size field)
  //   (x86 stubs assume this, and arm64 remains compatible).
  // - Density uses modern header only for AArch64 + KanziEXE; all other cases
  //   keep legacy header.
  {
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    if (strcmp(codec_name, "Zstd") == 0) {
      if (!(eh->e_machine == EM_AARCH64 && is_kanzi))
        is_legacy_layout = 1;
    } else if (strcmp(codec_name, "Density") == 0) {
      if (!(eh->e_machine == EM_AARCH64 && is_kanzi))
        is_legacy_layout = 1;
    }
  }

  size_t filtered_size = orig_size; // Default if not present
  const unsigned char *density_kexe_tail40 = NULL;
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

    {
      const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
      if (strcmp(codec_name, "Density") == 0 && eh->e_machine == EM_AARCH64 &&
          eh->e_type == ET_DYN) {
        if ((size_t)(p - payload) + 4u + 40u <= remaining) {
          uint32_t tag = 0;
          memcpy(&tag, p, sizeof(tag));
          if (tag == 0x21303454u) {
            density_kexe_tail40 = p + 4u;
            p += (sizeof(uint32_t) + 40u);
          }
        }
      }
    }
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
  memset(decomp_buf, 0, alloc_size);

  {
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    if (!is_legacy_layout && is_kanzi && strcmp(codec_name, "Density") == 0 &&
        eh->e_machine == EM_AARCH64 && eh->e_type == ET_DYN &&
        filtered_size > 0 && filtered_size <= alloc_size) {
      memset(decomp_buf, 0xA5, filtered_size);
    }
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

  {
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    if (!is_legacy_layout && is_kanzi && strcmp(codec_name, "Density") == 0 &&
        eh->e_machine == EM_AARCH64 && eh->e_type == ET_DYN && ret > 0) {
      size_t fr = (size_t)ret;
      if (filtered_size > fr && filtered_size <= alloc_size &&
          (filtered_size - fr) <= 256u) {
        size_t diff = filtered_size - fr;
        unsigned char *tail = decomp_buf + fr;
        size_t overwritten = 0;
        for (size_t i = 0; i < diff; i++) {
          overwritten += (tail[i] != (unsigned char)0xA5);
        }

        if (density_kexe_tail40 && diff == 40u && overwritten == 0) {
          memcpy(tail, density_kexe_tail40, 40u);
          ret = (int)filtered_size;
        } else {
          if (overwritten == 0) {
            memset(tail, 0, diff);
            ret = (int)filtered_size;
          } else {
            ret = (int)filtered_size;
          }
        }
      }
    }
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
    size_t done;
    int machine = 0;
    if (final_size >= sizeof(Elf64_Ehdr)) {
      const Elf64_Ehdr *eh = (const Elf64_Ehdr *)decomp_buf;
      if (eh->e_ident[EI_MAG0] == ELFMAG0 && eh->e_ident[EI_MAG1] == ELFMAG1 &&
          eh->e_ident[EI_MAG2] == ELFMAG2 && eh->e_ident[EI_MAG3] == ELFMAG3) {
        machine = (int)eh->e_machine;
      }
    }
    if (machine == EM_AARCH64)
      done = bcj_arm64_decode(decomp_buf, final_size, 0);
    else
      done = bcj_x86_decode(decomp_buf, final_size, 0);
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
      int in_sz = (int)final_size;
      int in_sz_alt = -1;
      if (!is_legacy_layout && is_kanzi && strcmp(codec_name, "Density") == 0) {
        if (filtered_size > final_size && filtered_size <= alloc_size &&
            (filtered_size - final_size) <= 256u) {
          in_sz_alt = (int)filtered_size;
        }
      }

      int uf_sz = kanzi_exe_unfilter_x86(decomp_buf, in_sz, unfiltered,
                                        (int)orig_size, &processed);
      int ok = (uf_sz > 0 && processed == in_sz && uf_sz == (int)orig_size);
      if (!ok && in_sz_alt > 0 && in_sz_alt != in_sz) {
        processed = 0;
        uf_sz = kanzi_exe_unfilter_x86(decomp_buf, in_sz_alt, unfiltered,
                                       (int)orig_size, &processed);
        ok = (uf_sz > 0 && processed == in_sz_alt && uf_sz == (int)orig_size);
        if (ok)
          in_sz = in_sz_alt;
      }

      if (ok) {
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
    } else if (final_size >= 9 && decomp_buf[0] == 0x20) {
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
      int in_sz = (int)final_size;
      int in_sz_alt = -1;
      if (!is_legacy_layout && is_kanzi && strcmp(codec_name, "Density") == 0) {
        if (filtered_size > final_size && filtered_size <= alloc_size &&
            (filtered_size - final_size) <= 256u) {
          in_sz_alt = (int)filtered_size;
        }
      }

      {
        const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
        if (g_verbose && !is_legacy_layout && is_kanzi &&
            strcmp(codec_name, "Density") == 0 && eh->e_machine == EM_AARCH64 &&
            eh->e_type == ET_DYN) {
          (void)in_sz;
          (void)in_sz_alt;
        }
      }

      int uf_sz = 0;
      int ok = 0;
      {
        const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
        int prefer_alt_first =
            (!is_legacy_layout && is_kanzi && strcmp(codec_name, "Density") == 0 &&
             eh->e_machine == EM_AARCH64 && eh->e_type == ET_DYN && in_sz_alt > 0 &&
             in_sz_alt != in_sz);

        if (prefer_alt_first) {
          processed = 0;
          uf_sz = kanzi_exe_unfilter_arm64(decomp_buf, in_sz_alt, unfiltered,
                                           (int)orig_size, &processed);
          ok = (uf_sz > 0 && processed == in_sz_alt && uf_sz == (int)orig_size);
          if (ok)
            in_sz = in_sz_alt;
        }

        if (!ok) {
          processed = 0;
          uf_sz = kanzi_exe_unfilter_arm64(decomp_buf, in_sz, unfiltered,
                                           (int)orig_size, &processed);
          ok = (uf_sz > 0 && processed == in_sz && uf_sz == (int)orig_size);
        }

        if (!ok && !prefer_alt_first && in_sz_alt > 0 && in_sz_alt != in_sz) {
          processed = 0;
          uf_sz = kanzi_exe_unfilter_arm64(decomp_buf, in_sz_alt, unfiltered,
                                           (int)orig_size, &processed);
          ok = (uf_sz > 0 && processed == in_sz_alt && uf_sz == (int)orig_size);
          if (ok)
            in_sz = in_sz_alt;
        }
      }

      if (ok) {
        free(decomp_buf);  // Free compressed/filtered buffer
        decomp_buf = NULL; // To avoid double free confusion
        final_buf = unfiltered;
        final_size = (size_t)uf_sz;

        {
          const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
          if (g_verbose && !is_legacy_layout && is_kanzi &&
              strcmp(codec_name, "Density") == 0 && eh->e_machine == EM_AARCH64 &&
              eh->e_type == ET_DYN) {
            (void)final_buf;
            (void)final_size;
          }
        }

        {
          const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
          if (g_verbose && !is_legacy_layout && is_kanzi &&
              strcmp(codec_name, "Density") == 0 && eh->e_machine == EM_AARCH64 &&
              eh->e_type == ET_DYN && final_size >= sizeof(Elf64_Ehdr)) {
            (void)final_buf;
          }
        }
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
