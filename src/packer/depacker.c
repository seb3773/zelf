#include "depacker.h"
#include "zelf_packer.h"
#include <elf.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../stub/codec_marker.h"

// Decompressors
#include "../decompressors/apultra/apultra_decompress.h"
#include "../decompressors/brieflz/brieflz_decompress.h"
#include "../decompressors/doboz/doboz_decompress.h"
#include "../decompressors/exo/exo_decompress.h"
#include "../decompressors/lz4/codec_lz4.h"
#include "../decompressors/lzav/lzav_decompress.h"
#include "../decompressors/lzma/minlzma_decompress.h"
#include "../decompressors/lzsa/libs/decompression/lzsa2_decompress.h"
#include "../decompressors/pp/powerpacker_decompress.h"
#include "../decompressors/quicklz/quicklz.h"
#include "../decompressors/shrinkler/codec_shrinkler.h"
#include "../decompressors/snappy/snappy_decompress.h"
#include "../decompressors/stonecracker/sc_decompress.h"
#include "../decompressors/zstd/zstd_minidec.h"
#include "../decompressors/zx0/zx0_decompress.h"
#include "../decompressors/zx7b/zx7b_decompress.h"

// Filters
#include "../filters/bcj_x86_min.h"
#include "../filters/kanzi_exe_decode_x86_tiny.h"

// Params Block Magic: +zELF-PR
static const unsigned char ELF_PARAMS_MAGIC[8] = {'+', 'z', 'E', 'L',
                                                  'F', '-', 'P', 'R'};

typedef struct {
  uint64_t version;
  uint64_t virtual_start;
  uint64_t packed_data_vaddr;
  uint64_t salt;
  uint64_t pwd_obfhash;
} elfz_params_t;

static int find_elfz_params(const unsigned char *data, size_t size,
                            elfz_params_t *params) {
  // Search for magic
  // Params block is small, typically in .rodata or patched stub.
  // We scan the whole file (or first X MB?).
  // Given we map the whole file, scanning it all is fine for typical binaries.
  // Optimization: stub is usually at the beginning.

  for (size_t i = 0; i + 48 <= size; i++) {
    if (memcmp(data + i, ELF_PARAMS_MAGIC, 8) == 0) {
      // Found magic
      params->version = *(uint64_t *)(data + i + 8);
      params->virtual_start = *(uint64_t *)(data + i + 16);
      params->packed_data_vaddr = *(uint64_t *)(data + i + 24);

      // Version check for v2 fields
      int ver = (int)(params->version & 0xFF);
      if (ver >= 2) {
        params->salt = *(uint64_t *)(data + i + 32);
        params->pwd_obfhash = *(uint64_t *)(data + i + 40);
      } else {
        params->salt = 0;
        params->pwd_obfhash = 0;
      }
      return 1;
    }
  }
  return 0;
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

static int write_file(const char *path, const void *data, size_t size) {
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
    printf("[%s%s%s] ELFZ Packed Binary detected (Params found)\n", PK_INFO, PK_SYM_INFO, PK_RES);
    printf("[%s%s%s] Version: %ld, Flags: 0x%02x\n", PK_INFO, PK_SYM_INFO, PK_RES,
           params.version & 0xFF, (int)((params.version >> 8) & 0xFF));
    printf("[%s%s%s] Packed Data VAddr: 0x%lx\n", PK_INFO, PK_SYM_INFO, PK_RES,
           params.packed_data_vaddr);
  }

  // 2. Locate Packed Data Payload
  size_t payload_offset = 0;
  if (params.packed_data_vaddr == 0) {
    // Dynamic binary (PIE) or missing params: address not patched/known.
    // Scan for marker.
    int found_marker = 0;
    // Known markers suffixes (after zELF)
    const char *suffixes[] = {"l4", "ap", "zx", "z0", "bz", "ex",
                              "pp", "sn", "dz", "qz", "lv", "la",
                              "sh", "sc", "ls", "zd", NULL};

    for (size_t i = 0; i < size - 6; i++) {
      if (data[i] == 'z' && data[i + 1] == 'E' && data[i + 2] == 'L' &&
          data[i + 3] == 'F') {
        // Check suffix
        for (int k = 0; suffixes[k] != NULL; k++) {
          if (data[i + 4] == suffixes[k][0] && data[i + 5] == suffixes[k][1]) {
            payload_offset = i;
            found_marker = 1;
            break;
          }
        }
        if (found_marker)
          break;
      }
    }
    if (!found_marker) {
      fprintf(stderr,
              "Error: Could not find compressed payload marker in file\n");
      munmap(data, size);
      close(fd);
      return 1;
    }
  } else {
    if (!vaddr_to_offset(data, size, params.packed_data_vaddr,
                         &payload_offset)) {
      fprintf(stderr,
              "Error: Could not map Packed Data VAddr to file offset\n");
      munmap(data, size);
      close(fd);
      return 1;
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

  size_t orig_size = *(size_t *)p;
  p += 8;
  uint64_t entry_offset = *(uint64_t *)p;
  p += 8;
  int comp_size = *(int *)p;
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
    is_legacy_layout = 1; // ZSTD is always legacy
  } else {
    fprintf(stderr, "Error: Unknown marker %s\n", marker);
    munmap(data, size);
    close(fd);
    return 1;
  }

  printf("[%s%s%s] Codec: %s\n", PK_INFO, PK_SYM_INFO, PK_RES, codec_name);
  printf("[%s%s%s] Original Size: %zu\n", PK_INFO, PK_SYM_INFO, PK_RES, orig_size);
  printf("[%s%s%s] Compressed Size: %d\n", PK_INFO, PK_SYM_INFO, PK_RES, comp_size);

  // If params missing, guess layout based on filtered_size presence check
  if (!params_found && !is_legacy_layout) { // ZSTD already set legacy
    // Check next 4 bytes (filtered_size candidate)
    if ((size_t)(p - payload) + 4 <= remaining) {
      int val = *(int *)p;
      size_t fsz = (size_t)val;
      // Heuristic: filtered size should be roughly related to orig_size
      if (fsz > 0 && fsz < 0x40000000 && fsz >= orig_size / 2 &&
          fsz <= orig_size * 2) {
        // Looks like valid filtered_size -> New Layout (Kanzi/None)
        is_legacy_layout = 0;
        is_bcj_flag = 0;
      } else {
        // Looks like random compressed data -> Legacy Layout (BCJ)
        is_legacy_layout = 1;
        is_bcj_flag = 1; // Assume BCJ if legacy layout inferred
      }
      printf("[%s%s%s] Layout heuristic: %s (Value: %d)\n", PK_INFO, PK_SYM_INFO, PK_RES,
             is_legacy_layout ? "Legacy (BCJ)" : "Modern (Kanzi/None)", val);
    }
  } else if (is_bcj_flag) {
    is_legacy_layout = 1; // BCJ implies legacy layout (no filtered_size)
  }

  size_t filtered_size = orig_size; // Default if not present
  if (!is_legacy_layout) {
    if ((size_t)(p - payload) + 4 > remaining) {
      fprintf(stderr, "Error: Truncated header (filtered_size)\n");
      munmap(data, size);
      close(fd);
      return 1;
    }
    filtered_size = (size_t)*(int *)p;
    p += 4;
    printf("[%s%s%s] Filtered Size: %zu\n", PK_INFO, PK_SYM_INFO, PK_RES, filtered_size);
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
                            (size_t)dst_cap);
  } else if (strcmp(codec_name, "Shrinkler") == 0) {
    ret = shrinkler_decompress_c((const char *)comp_data, (char *)decomp_buf,
                                 comp_size, dst_cap);
  } else if (strcmp(codec_name, "StoneCracker") == 0) {
    size_t r = sc_decompress(comp_data, (size_t)comp_size, decomp_buf,
                             (size_t)dst_cap);
    ret = r ? (int)r : -1;
  } else if (strcmp(codec_name, "LZSA2") == 0) {
    ret = lzsa2_decompress(comp_data, decomp_buf, comp_size, dst_cap, 0);
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
    printf("[%s%s%s] Filter: BCJ detected (Flag)\n", PK_INFO, PK_SYM_INFO, PK_RES);
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
      printf("[%s%s%s] Filter: Kanzi detected (Header)\n", PK_INFO, PK_SYM_INFO, PK_RES);
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
    fprintf(stderr, "Error: Failed to write output file\n");
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
