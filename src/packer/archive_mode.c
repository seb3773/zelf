#include "archive_mode.h"
#include "../stub/codec_marker.h"
#include "compression.h"
#include "depacker.h"
#include "packer_defs.h"
#include "zelf_packer.h"
#include "help_display.h"
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Include LZ4 decompressor header (we will link against codec_lz4.c)
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

// Helper to write buffer to file
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

// Helper to read file content
static void *read_file(const char *path, size_t *size_out) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz < 0) {
    fclose(f);
    return NULL;
  }
  void *data = malloc(sz);
  if (!data) {
    fclose(f);
    return NULL;
  }
  if (fread(data, 1, sz, f) != (size_t)sz) {
    free(data);
    fclose(f);
    return NULL;
  }
  fclose(f);
  *size_out = (size_t)sz;
  return data;
}

int archive_file(const char *input_path, const char *output_path,
                 const char *codec) {
  size_t input_size = 0;
  unsigned char *input_data = read_file(input_path, &input_size);
  if (!input_data) {
    fprintf(stderr, "Error: Cannot read input file %s\n", input_path);
    return 1;
  }

  unsigned char *compressed_data = NULL;
  int compressed_size = 0;
  // Unified banner consistent with packer main
  printf("\n");
  pk_emit_raw(ZELF_H2);
  printf("%s\n", PK_RES);
  printf("         %sv" ZELF_VERSION "%s -- By Seb3773%s\n\n",
         PKC(FYW), PKC(GREY17), PK_RES);
  printf("%s❖ Archive mode%s\n", PKC(FYW), PK_RES);

  printf("[%s%s%s] Archiving %s with %s...\n", PK_INFO, PK_SYM_INFO, PK_RES, input_path,
         codec);
  printf("[%s%s%s] Input size: %zu bytes\n", PK_INFO, PK_SYM_INFO, PK_RES, input_size);

  if (compress_data_with_codec(codec, input_data, input_size, &compressed_data,
                               &compressed_size) != 0) {
    fprintf(stderr, "Error: Compression failed\n");
    free(input_data);
    return 1;
  }

  // Determine marker
  const char *marker = "zELFl4"; // Default
  if (strcmp(codec, "lz4") == 0)
    marker = "zELFl4";
  else if (strcmp(codec, "apultra") == 0)
    marker = "zELFap";
  else if (strcmp(codec, "zx7b") == 0)
    marker = "zELFzx";
  else if (strcmp(codec, "zx0") == 0)
    marker = "zELFz0";
  else if (strcmp(codec, "zstd") == 0)
    marker = "zELFzd";
  else if (strcmp(codec, "blz") == 0)
    marker = "zELFbz";
  else if (strcmp(codec, "exo") == 0)
    marker = "zELFex";
  else if (strcmp(codec, "pp") == 0)
    marker = "zELFpp";
  else if (strcmp(codec, "snappy") == 0)
    marker = "zELFsn";
  else if (strcmp(codec, "doboz") == 0)
    marker = "zELFdz";
  else if (strcmp(codec, "qlz") == 0)
    marker = "zELFqz";
  else if (strcmp(codec, "lzav") == 0)
    marker = "zELFlv";
  else if (strcmp(codec, "lzma") == 0)
    marker = "zELFla";
  else if (strcmp(codec, "shrinkler") == 0)
    marker = "zELFsh";
  else if (strcmp(codec, "sc") == 0)
    marker = "zELFsc";
  else if (strcmp(codec, "lzsa") == 0 || strcmp(codec, "lzsa2") == 0)
    marker = "zELFls";

  // Header format: [MARKER 6b] [ORIG_SIZE 8b] [COMP_SIZE 4b] [DATA...]
  size_t header_size = 6 + 8 + 4;
  size_t total_size = header_size + compressed_size;
  unsigned char *final_data = malloc(total_size);
  if (!final_data) {
    free(input_data);
    free(compressed_data);
    return 1;
  }

  unsigned char *p = final_data;
  memcpy(p, marker, 6);
  p += 6;
  memcpy(p, &input_size, 8);
  p += 8;
  memcpy(p, &compressed_size, 4);
  p += 4;
  memcpy(p, compressed_data, compressed_size);

  // Determine output path
  char final_out[4096];
  if (output_path) {
    // Handle directory logic if needed (same as packer)
    struct stat st;
    if (stat(output_path, &st) == 0 && S_ISDIR(st.st_mode)) {
      char *base_dup = strdup(input_path);
      char *base = basename(base_dup);
      snprintf(final_out, sizeof(final_out), "%s/%s.zlf", output_path, base);
      free(base_dup);
    } else {
      strncpy(final_out, output_path, sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    }
  } else {
    // Default: write to current directory with .zlf extension
    char *base_dup = strdup(input_path);
    char *base = basename(base_dup);
    snprintf(final_out, sizeof(final_out), "./%s.zlf", base);
    free(base_dup);
  }

  if (write_file(final_out, final_data, total_size) != 0) {
    fprintf(stderr, "Error: Cannot write output file %s\n", final_out);
    free(final_data);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  printf("[%s%s%s] Archived to %s\n", PK_OK, PK_SYM_OK, PK_RES, final_out);
  printf("[%s%s%s] Compressed size: %d bytes\n",
         PK_INFO, PK_SYM_INFO, PK_RES, compressed_size);
  printf("[%s%s%s] Ratio: %.1f%%\n",
         PK_INFO, PK_SYM_INFO, PK_RES, 100.0 * compressed_size / input_size);

  // Warn if archive did not reduce size
  if (total_size >= input_size) {
    printf("[%s%s%s] Warning: Archive did not reduce size. Archive: %zu bytes >= Original: %zu bytes.\n",
           PK_WARN, PK_SYM_WARN, PK_RES, total_size, input_size);
  }

  free(final_data);
  free(input_data);
  free(compressed_data);
  return 0;
}

int unpack_file(const char *input_path, const char *output_path) {
  size_t file_size = 0;
  unsigned char *file_data = read_file(input_path, &file_size);
  if (!file_data) {
    fprintf(stderr, "Error: Cannot read input file %s\n", input_path);
    return 1;
  }

  if (file_size < 18) {
    fprintf(stderr, "Error: File too small to be a valid archive\n");
    free(file_data);
    return 1;
  }

  unsigned char *p = file_data;
  char marker[7];
  memcpy(marker, p, 6);
  marker[6] = '\0';
  p += 6;

  size_t orig_size = *(size_t *)p;
  p += 8;
  int comp_size = *(int *)p;
  p += 4;

  if (file_size < 18 + comp_size) {
    fprintf(stderr, "Error: Truncated archive\n");
    free(file_data);
    return 1;
  }

  printf("[%s%s%s] Unpacking %s...\n", PK_INFO, PK_SYM_INFO, PK_RES, input_path);

  unsigned char *decompressed = malloc(orig_size);
  if (!decompressed) {
    fprintf(stderr, "Error: Malloc failed for decompression\n");
    free(file_data);
    return 1;
  }

  int ret = -1;
  const char *detected_codec = "Unknown";

  if (strcmp(marker, "zELFl4") == 0) {
    detected_codec = "LZ4";
    ret = lz4_decompress((const char *)p, (char *)decompressed, comp_size,
                         (int)orig_size);
  } else if (strcmp(marker, "zELFap") == 0) {
    detected_codec = "Apultra";
    size_t r = apultra_decompress((const unsigned char *)p,
                                  (unsigned char *)decompressed,
                                  (size_t)comp_size, (size_t)orig_size, 0, 0);
    ret = r ? (int)r : -1;
  } else if (strcmp(marker, "zELFzx") == 0) {
    detected_codec = "ZX7B";
    ret = zx7b_decompress((const unsigned char *)p, comp_size,
                          (unsigned char *)decompressed, (int)orig_size);
  } else if (strcmp(marker, "zELFz0") == 0) {
    detected_codec = "ZX0";
    ret = zx0_decompress_to((const unsigned char *)p, comp_size,
                            (unsigned char *)decompressed, (int)orig_size);
  } else if (strcmp(marker, "zELFbz") == 0) {
    detected_codec = "BriefLZ";
    ret = brieflz_decompress_to((const unsigned char *)p, comp_size,
                                (unsigned char *)decompressed, (int)orig_size);
  } else if (strcmp(marker, "zELFex") == 0) {
    detected_codec = "Exo";
    ret = exo_decompress((const unsigned char *)p, comp_size,
                         (unsigned char *)decompressed, (int)orig_size);
  } else if (strcmp(marker, "zELFpp") == 0) {
    detected_codec = "PowerPacker";
    ret = powerpacker_decompress(
        (const unsigned char *)p, (unsigned int)comp_size,
        (unsigned char *)decompressed, (unsigned int)orig_size);
  } else if (strcmp(marker, "zELFsn") == 0) {
    detected_codec = "Snappy";
    size_t r = snappy_decompress_simple(
        (const unsigned char *)p, (size_t)comp_size,
        (unsigned char *)decompressed, (size_t)orig_size);
    ret = r ? (int)r : -1;
  } else if (strcmp(marker, "zELFdz") == 0) {
    detected_codec = "Doboz";
    ret = doboz_decompress((const char *)p, (size_t)comp_size,
                           (char *)decompressed, (size_t)orig_size);
  } else if (strcmp(marker, "zELFqz") == 0) {
    detected_codec = "QuickLZ";
    qlz_state_decompress state = {0};
    size_t r = qlz_decompress((const char *)p, (char *)decompressed, &state);
    ret = r > 0 ? (int)r : -1;
  } else if (strcmp(marker, "zELFlv") == 0) {
    detected_codec = "LZAV";
    ret = lzav_decompress_c((const char *)p, (char *)decompressed, comp_size,
                            (int)orig_size);
  } else if (strcmp(marker, "zELFla") == 0) {
    detected_codec = "LZMA";
    ret = minlzma_decompress_c((const char *)p, (char *)decompressed, comp_size,
                               (int)orig_size);
  } else if (strcmp(marker, "zELFzd") == 0) {
    detected_codec = "Zstd";
    ret = zstd_decompress_c((const unsigned char *)p, (size_t)comp_size,
                            (unsigned char *)decompressed, (size_t)orig_size);
  } else if (strcmp(marker, "zELFsh") == 0) {
    detected_codec = "Shrinkler";
    ret = shrinkler_decompress_c((const char *)p, (char *)decompressed,
                                 comp_size, (int)orig_size);
  } else if (strcmp(marker, "zELFsc") == 0) {
    detected_codec = "StoneCracker";
    size_t r = sc_decompress((const uint8_t *)p, (size_t)comp_size,
                             (uint8_t *)decompressed, (size_t)orig_size);
    ret = r ? (int)r : -1;
  } else if (strcmp(marker, "zELFls") == 0) {
    detected_codec = "LZSA2";
    ret = lzsa2_decompress((const unsigned char *)p,
                           (unsigned char *)decompressed, comp_size,
                           (int)orig_size, 0);
  } else {
    fprintf(stderr, "Error: Unsupported codec marker: %s\n", marker);
    free(decompressed);
    free(file_data);
    return 1;
  }

  printf("[%s%s%s] Detected codec: %s\n", PK_INFO, PK_SYM_INFO, PK_RES, detected_codec);

  if (ret < 0) {
    fprintf(stderr, "Error: Decompression failed\n");
    free(decompressed);
    free(file_data);
    return 1;
  }

  // Determine output path
  char final_out[4096];
  if (output_path) {
    struct stat st;
    if (stat(output_path, &st) == 0 && S_ISDIR(st.st_mode)) {
      // Directory: use original basename without .zlf
      char *base_dup = strdup(input_path);
      char *base = basename(base_dup);
      // Remove .zlf extension if present
      char *ext = strstr(base, ".zlf");
      if (ext && strlen(ext) == 4)
        *ext = '\0';
      snprintf(final_out, sizeof(final_out), "%s/%s", output_path, base);
      free(base_dup);
    } else {
      strncpy(final_out, output_path, sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    }
  } else {
    // Default: write to current directory, remove .zlf from basename
    char *base_dup = strdup(input_path);
    char *base = basename(base_dup);

    char *ext = strstr(base, ".zlf");
    if (ext && strlen(ext) == 4)
      *ext = '\0';
    else {
      // If no .zlf extension, append .unpacked
      // We need to be careful about buffer size if we append, but base is
      // smaller than 4096 Let's use a safe cat
      size_t len = strlen(base);
      if (len + 10 < sizeof(final_out)) {
        strcat(base, ".unpacked");
      }
    }
    snprintf(final_out, sizeof(final_out), "./%s", base);
    free(base_dup);
  }

  if (write_file(final_out, decompressed, orig_size) != 0) {
    fprintf(stderr, "Error: Cannot write output file %s\n", final_out);
    free(decompressed);
    free(file_data);
    return 1;
  }

  printf("[%s%s%s] Unpacked to %s\n", PK_OK, PK_SYM_OK, PK_RES, final_out);
  printf("[%s%s%s] Unpacked size: %zu bytes\n", PK_INFO, PK_SYM_INFO, PK_RES, orig_size);

  free(decompressed);
  free(file_data);
  return 0;
}
