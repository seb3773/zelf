#include "archive_mode.h"
#include "../stub/codec_marker.h"
#include "compression.h"
#include "depacker.h"
#include "help_display.h"
#include "packer_defs.h"
#include "zelf_packer.h"
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Include LZ4 decompressor header (we will link against codec_lz4.c)
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
#include "../decompressors/zstd/zstd_minidec.h"
#include "../decompressors/rnc/rnc_minidec.h"
#include "../decompressors/lzfse/lzfse_stub.h"
#include "../decompressors/csc/csc_dec.h"
#include "../decompressors/nz1/nz1_decompressor.h"
#include "../decompressors/zx0/zx0_decompress.h"
#include "../decompressors/zx7b/zx7b_decompress.h"

// Helper to write buffer to file
static int write_file(const char *path, const void *data, size_t size) {
  if (path && strcmp(path, "-") == 0) {
    const unsigned char *p = (const unsigned char *)data;
    size_t left = size;
    while (left) {
      ssize_t w = write(STDOUT_FILENO, p, left);
      if (w < 0) {
        if (errno == EINTR)
          continue;
        return -1;
      }
      if (w == 0)
        return -1;
      p += (size_t)w;
      left -= (size_t)w;
    }
    return 0;
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

static void *read_all_fd(int fd, size_t *size_out) {
  size_t cap = 1u << 20;
  size_t len = 0;
  unsigned char *buf = (unsigned char *)malloc(cap);
  if (!buf)
    return NULL;
  for (;;) {
    if (len == cap) {
      size_t ncap = cap << 1;
      if (__builtin_expect(ncap < cap, 0)) {
        free(buf);
        return NULL;
      }
      unsigned char *nbuf = (unsigned char *)realloc(buf, ncap);
      if (!nbuf) {
        free(buf);
        return NULL;
      }
      buf = nbuf;
      cap = ncap;
    }
    ssize_t r = read(fd, buf + len, cap - len);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      free(buf);
      return NULL;
    }
    if (r == 0)
      break;
    len += (size_t)r;
  }
  unsigned char *out = (unsigned char *)realloc(buf, len ? len : 1);
  if (!out) {
    free(buf);
    return NULL;
  }
  *size_out = len;
  return out;
}

static char *dup_trim_trailing_slashes(const char *path) {
  if (!path)
    return NULL;
  size_t n = strlen(path);
  char *s = (char *)malloc(n + 1);
  if (!s)
    return NULL;
  memcpy(s, path, n + 1);
  while (n > 1 && s[n - 1] == '/') {
    s[n - 1] = '\0';
    --n;
  }
  return s;
}

static void *read_tar_stream_from_dir(const char *dir_path, size_t *size_out) {
  int pipefd[2];
  if (pipe(pipefd) != 0)
    return NULL;

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return NULL;
  }

  if (pid == 0) {
    (void)dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    char *const argv[] = {(char *)"tar", (char *)"-C", (char *)dir_path,
                          (char *)"-cf", (char *)"-", (char *)".", NULL};
    execvp("tar", argv);
    _exit(127);
  }

  close(pipefd[1]);
  void *data = read_all_fd(pipefd[0], size_out);
  close(pipefd[0]);

  int st = 0;
  while (waitpid(pid, &st, 0) < 0) {
    if (errno == EINTR)
      continue;
    break;
  }
  if (!data)
    return NULL;
  if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
    free(data);
    return NULL;
  }
  return data;
}

// Helper to read file content
static void *read_file(const char *path, size_t *size_out) {
  if (path && strcmp(path, "-") == 0) {
    return read_all_fd(STDIN_FILENO, size_out);
  }
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

static int str_endswith(const char *s, const char *suffix) {
  if (!s || !suffix)
    return 0;
  size_t ns = strlen(s);
  size_t nx = strlen(suffix);
  if (nx > ns)
    return 0;
  return memcmp(s + (ns - nx), suffix, nx) == 0;
}

static int buf_looks_like_tar(const unsigned char *p, size_t n) {
  if (!p)
    return 0;
  if (n < 262)
    return 0;
  return memcmp(p + 257, "ustar", 5) == 0;
}

static int ensure_dir_exists(const char *dir_path) {
  if (!dir_path || !*dir_path)
    return -1;

  struct stat st;
  if (stat(dir_path, &st) == 0) {
    if (S_ISDIR(st.st_mode))
      return 0;
    errno = ENOTDIR;
    return -1;
  }
  if (mkdir(dir_path, 0755) == 0)
    return 0;
  return -1;
}

static int untar_stream_to_dir(const unsigned char *tar_data, size_t tar_size,
                               const char *out_dir) {
  if (!tar_data || !out_dir)
    return 1;

  int pipefd[2];
  if (pipe(pipefd) != 0)
    return 1;

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return 1;
  }

  if (pid == 0) {
    (void)dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    char *const argv[] = {(char *)"tar", (char *)"-C", (char *)out_dir,
                          (char *)"-xf", (char *)"-", NULL};
    execvp("tar", argv);
    _exit(127);
  }

  close(pipefd[0]);
  const unsigned char *p = tar_data;
  size_t left = tar_size;
  while (left) {
    ssize_t w = write(pipefd[1], p, left);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      close(pipefd[1]);
      (void)waitpid(pid, NULL, 0);
      return 1;
    }
    if (w == 0)
      break;
    p += (size_t)w;
    left -= (size_t)w;
  }
  close(pipefd[1]);

  int st = 0;
  while (waitpid(pid, &st, 0) < 0) {
    if (errno == EINTR)
      continue;
    break;
  }
  if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
    return 1;
  return 0;
}

int archive_tar_dir(const char *dir_path, const char *output_path,
                    const char *codec) {
  FILE *logf = stdout;
  if (output_path && strcmp(output_path, "-") == 0)
    logf = stderr;

  struct stat st;
  if (!dir_path || stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "Error: Invalid directory %s\n", dir_path ? dir_path : "(null)");
    return 1;
  }

  size_t input_size = 0;
  unsigned char *input_data =
      (unsigned char *)read_tar_stream_from_dir(dir_path, &input_size);
  if (!input_data) {
    fprintf(stderr, "Error: Cannot create tar stream from %s\n", dir_path);
    return 1;
  }

  unsigned char *compressed_data = NULL;
  int compressed_size = 0;

  fprintf(logf, "[%s%s%s] Archiving %s (tar) with %s...\n", PK_INFO,
          PK_SYM_INFO, PK_RES, dir_path, codec);
  fprintf(logf, "[%s%s%s] Input size: %zu bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, input_size);

  if (compress_data_with_codec(codec, input_data, input_size, &compressed_data,
                               &compressed_size) != 0) {
    fprintf(stderr, "Error: Compression failed\n");
    free(input_data);
    return 1;
  }

  const char *marker = "zELFla";
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
  else if (strcmp(codec, "density") == 0)
    marker = "zELFde";
  else if (strcmp(codec, "lzham") == 0)
    marker = "zELFlz";
  else if (strcmp(codec, "rnc") == 0)
    marker = "zELFrn";
  else if (strcmp(codec, "lzfse") == 0)
    marker = "zELFse";
  else if (strcmp(codec, "csc") == 0)
    marker = "zELFcs";
  else if (strcmp(codec, "nz1") == 0)
    marker = "zELFnz";

  size_t header_size = 6 + 8 + 4;
  size_t total_size = header_size + (size_t)compressed_size;
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
  memcpy(p, compressed_data, (size_t)compressed_size);

  char final_out[4096];
  if (output_path) {
    if (strcmp(output_path, "-") == 0) {
      strncpy(final_out, "-", sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    } else {
      struct stat st_out;
      if (stat(output_path, &st_out) == 0 && S_ISDIR(st_out.st_mode)) {
        char *dir_dup = dup_trim_trailing_slashes(dir_path);
        if (!dir_dup) {
          free(final_data);
          free(input_data);
          free(compressed_data);
          return 1;
        }
        char *base = basename(dir_dup);
        snprintf(final_out, sizeof(final_out), "%s/%s.tar.zlf", output_path,
                 base);
        free(dir_dup);
      } else {
        strncpy(final_out, output_path, sizeof(final_out) - 1);
        final_out[sizeof(final_out) - 1] = '\0';
      }
    }
  } else {
    char *dir_dup = dup_trim_trailing_slashes(dir_path);
    if (!dir_dup) {
      free(final_data);
      free(input_data);
      free(compressed_data);
      return 1;
    }
    char *base = basename(dir_dup);
    snprintf(final_out, sizeof(final_out), "./%s.tar.zlf", base);
    free(dir_dup);
  }

  if (write_file(final_out, final_data, total_size) != 0) {
    fprintf(stderr, "Error: Cannot write output file %s\n", final_out);
    free(final_data);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  fprintf(logf, "[%s%s%s] Archived to %s\n", PK_OK, PK_SYM_OK, PK_RES,
          final_out);
  fprintf(logf, "[%s%s%s] Compressed size: %d bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, compressed_size);
  fprintf(logf, "[%s%s%s] Ratio: %.1f%%\n", PK_INFO, PK_SYM_INFO, PK_RES,
          100.0 * compressed_size / input_size);

  free(final_data);
  free(input_data);
  free(compressed_data);
  return 0;
}

int archive_file(const char *input_path, const char *output_path,
                 const char *codec) {
  FILE *logf = stdout;
  if (output_path && strcmp(output_path, "-") == 0)
    logf = stderr;

  size_t input_size = 0;
  unsigned char *input_data = read_file(input_path, &input_size);
  if (!input_data) {
    fprintf(stderr, "Error: Cannot read input file %s\n", input_path);
    return 1;
  }

  unsigned char *compressed_data = NULL;
  int compressed_size = 0;

  fprintf(logf, "[%s%s%s] Archiving %s with %s...\n", PK_INFO, PK_SYM_INFO,
          PK_RES, input_path, codec);
  fprintf(logf, "[%s%s%s] Input size: %zu bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, input_size);

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
  else if (strcmp(codec, "density") == 0)
    marker = "zELFde";
  else if (strcmp(codec, "lzham") == 0)
    marker = "zELFlz";
  else if (strcmp(codec, "rnc") == 0)
    marker = "zELFrn";
  else if (strcmp(codec, "lzfse") == 0)
    marker = "zELFse";
  else if (strcmp(codec, "csc") == 0)
    marker = "zELFcs";
  else if (strcmp(codec, "nz1") == 0)
    marker = "zELFnz";

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
    if (strcmp(output_path, "-") == 0) {
      strncpy(final_out, "-", sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    } else {
      // Handle directory logic if needed (same as packer)
      struct stat st;
      if (stat(output_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        char *base_dup = strdup(input_path);
        char *base = basename(base_dup);
        if (strcmp(base, "-") == 0)
          base = (char *)"stdin";
        snprintf(final_out, sizeof(final_out), "%s/%s.zlf", output_path, base);
        free(base_dup);
      } else {
        strncpy(final_out, output_path, sizeof(final_out) - 1);
        final_out[sizeof(final_out) - 1] = '\0';
      }
    }
  } else {
    // Default: write to current directory with .zlf extension
    char *base_dup = strdup(input_path);
    char *base = basename(base_dup);
    if (strcmp(base, "-") == 0)
      base = (char *)"stdin";
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

  fprintf(logf, "[%s%s%s] Archived to %s\n", PK_OK, PK_SYM_OK, PK_RES,
          final_out);
  fprintf(logf, "[%s%s%s] Compressed size: %d bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, compressed_size);
  fprintf(logf, "[%s%s%s] Ratio: %.1f%%\n", PK_INFO, PK_SYM_INFO, PK_RES,
          100.0 * compressed_size / input_size);

  // Warn if archive did not reduce size
  if (total_size >= input_size) {
    fprintf(logf,
            "[%s%s%s] Warning: Archive did not reduce size. Archive: %zu bytes "
            ">= Original: %zu bytes.\n",
            PK_WARN, PK_SYM_WARN, PK_RES, total_size, input_size);
  }

  free(final_data);
  free(input_data);
  free(compressed_data);
  return 0;
}

int unpack_file(const char *input_path, const char *output_path) {
  FILE *logf = stdout;
  if (output_path && strcmp(output_path, "-") == 0)
    logf = stderr;

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

  size_t orig_size;
  memcpy(&orig_size, p, sizeof(orig_size));
  p += 8;
  int comp_size;
  memcpy(&comp_size, p, sizeof(comp_size));
  p += 4;

  if (file_size < 18 + comp_size) {
    fprintf(stderr, "Error: Truncated archive\n");
    free(file_data);
    return 1;
  }

  fprintf(logf, "[%s%s%s] Unpacking %s...\n", PK_INFO, PK_SYM_INFO, PK_RES,
          input_path);

  /* Density may need slightly more buffer space for internal processing.
   * Add a small margin to avoid STALL_ON_OUTPUT errors. */
  size_t alloc_size = orig_size + 4096;
  unsigned char *decompressed = malloc(alloc_size);
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
  } else if (strcmp(marker, "zELFde") == 0) {
    detected_codec = "Density";
    ret =
        density_decompress_c((const unsigned char *)p, (size_t)comp_size,
                             (unsigned char *)decompressed, (size_t)alloc_size);
  } else if (strcmp(marker, "zELFlz") == 0) {
    detected_codec = "LZHAM";
    // Use larger workspace (4MB) to rule out memory allocation issues
    size_t ws_sz = 4 * 1024 * 1024;
    uint8_t *workspace = (uint8_t *)malloc(ws_sz);
    if (!workspace) {
      fprintf(stderr, "Error: Failed to allocate LZHAM workspace\n");
      free(decompressed);
      free(file_data);
      return 1;
    }
    lzhamd_params_t params;
    lzhamd_params_init_default(&params, workspace, ws_sz,
                               29); // Must match compression dict_size_log2
    size_t dst_len = (size_t)orig_size;
    lzhamd_status_t st = lzhamd_decompress_memory(
        &params, decompressed, &dst_len, (const uint8_t *)p, (size_t)comp_size);
    free(workspace);
    ret = (st == LZHAMD_STATUS_SUCCESS) ? (int)dst_len : -1;
  } else if (strcmp(marker, "zELFrn") == 0) {
    detected_codec = "RNC";
    ret = rnc_decompress_to((const unsigned char *)p, comp_size,
                            (unsigned char *)decompressed, (int)orig_size);
  } else if (strcmp(marker, "zELFse") == 0) {
    detected_codec = "LZFSE";
    void *scratch = malloc(lzfse_decode_scratch_size());
    if (!scratch) {
      fprintf(stderr, "Error: Failed to allocate LZFSE scratch\n");
      free(decompressed);
      free(file_data);
      return 1;
    }
    size_t r = lzfse_decode_buffer((uint8_t *)decompressed, (size_t)orig_size,
                                   (const uint8_t *)p, (size_t)comp_size,
                                   scratch);
    free(scratch);
    ret = r ? (int)r : -1;
  } else if (strcmp(marker, "zELFcs") == 0) {
    detected_codec = "CSC";
    intptr_t ws = CSC_Dec_GetWorkspaceSize(p, (size_t)comp_size);
    if (ws < 0) {
      ret = -1;
    } else {
      void *workspace = malloc((size_t)ws);
      if (!workspace) {
        fprintf(stderr, "Error: Failed to allocate CSC workspace\n");
        free(decompressed);
        free(file_data);
        return 1;
      }
      intptr_t r = CSC_Dec_Decompress(p, (size_t)comp_size, decompressed,
                                      (size_t)orig_size, workspace);
      free(workspace);
      ret = (r >= 0) ? (int)r : -1;
    }
  } else if (strcmp(marker, "zELFnz") == 0) {
    detected_codec = "NZ1";
    ret = nanozip_decompress((const void *)p, comp_size, (void *)decompressed,
                             (int)orig_size);
  } else {
    fprintf(stderr, "Error: Unsupported codec marker: %s\n", marker);
    free(decompressed);
    free(file_data);
    return 1;
  }

  fprintf(logf, "[%s%s%s] Detected codec: %s\n", PK_INFO, PK_SYM_INFO, PK_RES,
          detected_codec);

  if (ret < 0) {
    fprintf(stderr, "Error: Decompression failed\n");
    free(decompressed);
    free(file_data);
    return 1;
  }

  int want_untar = 0;
  if (input_path && strcmp(input_path, "-") != 0) {
    if (str_endswith(input_path, ".tar.zlf"))
      want_untar = 1;
  }
  if (!want_untar && buf_looks_like_tar(decompressed, orig_size))
    want_untar = 1;

  if (want_untar) {
    if (output_path && strcmp(output_path, "-") == 0) {
      if (write_file("-", decompressed, orig_size) != 0) {
        fprintf(stderr, "Error: Cannot write tar stream to stdout\n");
        free(decompressed);
        free(file_data);
        return 1;
      }
      free(decompressed);
      free(file_data);
      return 0;
    }

    char out_dir[4096];
    if (output_path) {
      strncpy(out_dir, output_path, sizeof(out_dir) - 1);
      out_dir[sizeof(out_dir) - 1] = '\0';
    } else {
      char *base_dup = strdup(input_path ? input_path : "stdin.tar.zlf");
      if (!base_dup) {
        fprintf(stderr, "Error: OOM\n");
        free(decompressed);
        free(file_data);
        return 1;
      }
      char *base = basename(base_dup);
      if (!base)
        base = (char *)"archive.tar.zlf";

      char name_buf[4096];
      if (str_endswith(base, ".tar.zlf")) {
        size_t n = strlen(base) - strlen(".tar.zlf");
        if (n >= sizeof(name_buf))
          n = sizeof(name_buf) - 1;
        memcpy(name_buf, base, n);
        name_buf[n] = '\0';
      } else if (str_endswith(base, ".zlf")) {
        size_t n = strlen(base) - strlen(".zlf");
        if (n >= sizeof(name_buf))
          n = sizeof(name_buf) - 1;
        memcpy(name_buf, base, n);
        name_buf[n] = '\0';
      } else {
        snprintf(name_buf, sizeof(name_buf), "%s.unpacked", base);
      }

      snprintf(out_dir, sizeof(out_dir), "./%s", name_buf);
      free(base_dup);
    }

    if (ensure_dir_exists(out_dir) != 0) {
      fprintf(stderr, "Error: Cannot create output directory %s: %s\n", out_dir,
              strerror(errno));
      free(decompressed);
      free(file_data);
      return 1;
    }

    fprintf(logf, "[%s%s%s] Extracting tar to %s ...\n", PK_INFO, PK_SYM_INFO,
            PK_RES, out_dir);
    if (untar_stream_to_dir(decompressed, orig_size, out_dir) != 0) {
      fprintf(stderr, "Error: tar extraction failed\n");
      free(decompressed);
      free(file_data);
      return 1;
    }

    fprintf(logf, "[%s%s%s] Extracted to %s\n", PK_OK, PK_SYM_OK, PK_RES,
            out_dir);
    free(decompressed);
    free(file_data);
    return 0;
  }

  // Determine output path
  char final_out[4096];
  if (output_path) {
    if (strcmp(output_path, "-") == 0) {
      strncpy(final_out, "-", sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    } else {
      struct stat st;
      if (stat(output_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        // Directory: use original basename without .zlf
        char *base_dup = strdup(input_path);
        char *base = basename(base_dup);
        if (strcmp(base, "-") == 0)
          base = (char *)"stdin";
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
    }
  } else {
    // Default: write to current directory, remove .zlf from basename
    char *base_dup = strdup(input_path);
    char *base = basename(base_dup);
    if (strcmp(base, "-") == 0)
      base = (char *)"stdin";

    char name_buf[4096];
    char *ext = strstr(base, ".zlf");
    if (ext && strlen(ext) == 4) {
      size_t n = (size_t)(ext - base);
      if (n >= sizeof(name_buf))
        n = sizeof(name_buf) - 1;
      memcpy(name_buf, base, n);
      name_buf[n] = '\0';
    } else {
      snprintf(name_buf, sizeof(name_buf), "%s.unpacked", base);
    }
    snprintf(final_out, sizeof(final_out), "./%s", name_buf);
    free(base_dup);
  }

  if (write_file(final_out, decompressed, orig_size) != 0) {
    fprintf(stderr, "Error: Cannot write output file %s\n", final_out);
    free(decompressed);
    free(file_data);
    return 1;
  }

  fprintf(logf, "[%s%s%s] Unpacked to %s\n", PK_OK, PK_SYM_OK, PK_RES,
          final_out);
  fprintf(logf, "[%s%s%s] Unpacked size: %zu bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, orig_size);

  free(decompressed);
  free(file_data);
  return 0;
}
