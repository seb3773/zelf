#include "archive_mode.h"
#include "../stub/codec_marker.h"
#include "compression.h"
#include "depacker.h"
#include "help_display.h"
#include "packer_defs.h"
#include "stub_selection.h"
#include "zelf_packer.h"
#include <dirent.h>
#include <errno.h>
#include <elf.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
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

static int str_endswith(const char *s, const char *suffix) {
  if (!s || !suffix)
    return 0;
  size_t ns = strlen(s);
  size_t nf = strlen(suffix);
  if (nf > ns)
    return 0;
  return memcmp(s + (ns - nf), suffix, nf) == 0;
}

static char *dup_trim_trailing_slashes(const char *path) {
  if (!path)
    return NULL;
  char *s = strdup(path);
  if (!s)
    return NULL;
  size_t n = strlen(s);
  while (n > 1 && s[n - 1] == '/') {
    s[n - 1] = '\0';
    --n;
  }
  return s;
}

static int ensure_dir_exists(const char *path) {
  if (!path || !*path)
    return -1;
  char tmp[4096];
  size_t n = strlen(path);
  if (n >= sizeof(tmp))
    return -1;
  memcpy(tmp, path, n + 1);
  if (tmp[0] == '/' && tmp[1] == '\0')
    return 0;

  for (size_t i = 1; i <= n; ++i) {
    if (tmp[i] == '/' || tmp[i] == '\0') {
      char saved = tmp[i];
      tmp[i] = '\0';
      if (tmp[0] != '\0') {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
          tmp[i] = saved;
          return -1;
        }
      }
      tmp[i] = saved;
    }
  }
  return 0;
}

static int ensure_parent_dir_exists(const char *path) {
  if (!path || !*path)
    return -1;
  char tmp[4096];
  size_t n = strlen(path);
  if (n >= sizeof(tmp))
    return -1;
  memcpy(tmp, path, n + 1);
  char *slash = strrchr(tmp, '/');
  if (!slash)
    return 0;
  if (slash == tmp)
    return 0;
  *slash = '\0';
  return ensure_dir_exists(tmp);
}

static int sanitize_relpath(const char *in, char *out, size_t cap) {
  if (!out || cap == 0)
    return -1;
  out[0] = '\0';
  if (!in || !*in)
    return -1;

  while (*in == '/')
    ++in;
  if (!*in)
    return -1;

  size_t w = 0;
  const char *p = in;
  while (*p) {
    while (*p == '/')
      ++p;
    if (!*p)
      break;
    const char *seg = p;
    while (*p && *p != '/')
      ++p;
    size_t seglen = (size_t)(p - seg);
    if (seglen == 0)
      continue;
    if (seglen == 1 && seg[0] == '.')
      continue;
    if (seglen == 2 && seg[0] == '.' && seg[1] == '.')
      return -1;
    if (w) {
      if (w + 1 >= cap)
        return -1;
      out[w++] = '/';
    }
    if (w + seglen >= cap)
      return -1;
    memcpy(out + w, seg, seglen);
    w += seglen;
    out[w] = '\0';
  }
  return (w != 0) ? 0 : -1;
}

static unsigned char *read_file(const char *path, size_t *out_size) {
  if (out_size)
    *out_size = 0;
  if (!path)
    return NULL;

  int fd = -1;
  if (strcmp(path, "-") == 0)
    fd = STDIN_FILENO;
  else
    fd = open(path, O_RDONLY);
  if (fd < 0)
    return NULL;

  size_t cap = 0;
  size_t sz = 0;
  unsigned char *buf = NULL;

  for (;;) {
    if (sz == cap) {
      size_t ncap = cap ? (cap + (cap >> 1)) : 8192u;
      unsigned char *nbuf = (unsigned char *)realloc(buf, ncap);
      if (!nbuf) {
        free(buf);
        if (fd != STDIN_FILENO)
          close(fd);
        return NULL;
      }
      buf = nbuf;
      cap = ncap;
    }
    ssize_t r = read(fd, buf + sz, cap - sz);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      free(buf);
      if (fd != STDIN_FILENO)
        close(fd);
      return NULL;
    }
    if (r == 0)
      break;
    sz += (size_t)r;
  }

  if (fd != STDIN_FILENO)
    close(fd);
  if (out_size)
    *out_size = sz;
  return buf;
}

static int buf_looks_like_tar(const unsigned char *buf, size_t n) {
  if (!buf || n < 512)
    return 0;
  if ((n & 511u) != 0)
    return 0;
  if (memcmp(buf + 257, "ustar", 5) == 0)
    return 1;
  return 0;
}

struct dynbuf {
  unsigned char *p;
  size_t sz;
  size_t cap;
};

static int db_reserve(struct dynbuf *b, size_t add) {
  if (!b)
    return -1;
  size_t need = b->sz + add;
  if (need <= b->cap)
    return 0;
  size_t ncap = b->cap ? b->cap : 8192u;
  while (ncap < need)
    ncap += (ncap >> 1) + 4096u;
  unsigned char *np = (unsigned char *)realloc(b->p, ncap);
  if (!np)
    return -1;
  b->p = np;
  b->cap = ncap;
  return 0;
}

static int db_append(struct dynbuf *b, const void *data, size_t n) {
  if (db_reserve(b, n) != 0)
    return -1;
  memcpy(b->p + b->sz, data, n);
  b->sz += n;
  return 0;
}

static void tar_octal(char *dst, size_t dst_len, uint64_t v) {
  if (dst_len == 0)
    return;
  if (dst_len == 1) {
    dst[0] = '\0';
    return;
  }

  memset(dst, '0', dst_len);
  dst[dst_len - 1] = '\0';

  size_t pos = dst_len - 2;
  for (;;) {
    dst[pos] = (char)('0' + (v & 7u));
    v >>= 3;
    if (pos == 0)
      break;
    if (v == 0) {
      while (pos > 0)
        dst[--pos] = '0';
      break;
    }
    --pos;
  }
}

static int tar_add_header(struct dynbuf *b, const char *name, const struct stat *st,
                          char typeflag, uint64_t fsize) {
  unsigned char hdr[512];
  memset(hdr, 0, sizeof(hdr));

  if (!name)
    return -1;
  size_t nn = strlen(name);
  if (nn == 0)
    return -1;
  if (nn <= 100) {
    memcpy(hdr + 0, name, nn);
  } else if (nn <= 255) {
    const char *slash = strrchr(name, '/');
    if (!slash)
      return -1;
    size_t pfx = (size_t)(slash - name);
    size_t bnm = nn - pfx - 1;
    if (pfx > 155 || bnm > 100)
      return -1;
    memcpy(hdr + 345, name, pfx);
    memcpy(hdr + 0, slash + 1, bnm);
  } else {
    return -1;
  }

  unsigned mode = 0644u;
  if (st)
    mode = (unsigned)(st->st_mode & 0777u);
  tar_octal((char *)(hdr + 100), 8, mode);
  tar_octal((char *)(hdr + 108), 8, 0);
  tar_octal((char *)(hdr + 116), 8, 0);
  tar_octal((char *)(hdr + 124), 12, fsize);
  tar_octal((char *)(hdr + 136), 12, (uint64_t)time(NULL));
  memset(hdr + 148, ' ', 8);
  hdr[156] = (unsigned char)typeflag;
  memcpy(hdr + 257, "ustar", 5);
  hdr[262] = '\0';
  memcpy(hdr + 263, "00", 2);

  unsigned sum = 0;
  for (size_t i = 0; i < 512; ++i)
    sum += (unsigned)hdr[i];
  tar_octal((char *)(hdr + 148), 8, sum);

  return db_append(b, hdr, 512);
}

static int tar_add_file_data(struct dynbuf *b, const unsigned char *data, size_t n) {
  if (n) {
    if (db_append(b, data, n) != 0)
      return -1;
  }
  size_t pad = (512u - (n & 511u)) & 511u;
  if (pad) {
    unsigned char z[512];
    memset(z, 0, sizeof(z));
    if (db_append(b, z, pad) != 0)
      return -1;
  }
  return 0;
}

static int tar_add_path_recursive(struct dynbuf *b, const char *root_fs,
                                  const char *root_name, const char *rel) {
  char fs_path[4096];
  if (!root_fs || !root_name)
    return -1;
  if (!rel)
    rel = "";

  if (rel[0] == '\0') {
    snprintf(fs_path, sizeof(fs_path), "%s", root_fs);
  } else {
    snprintf(fs_path, sizeof(fs_path), "%s/%s", root_fs, rel);
  }

  struct stat st;
  if (lstat(fs_path, &st) != 0)
    return -1;

  char tar_name[512];
  if (rel[0] == '\0') {
    snprintf(tar_name, sizeof(tar_name), "%s/", root_name);
  } else {
    snprintf(tar_name, sizeof(tar_name), "%s/%s", root_name, rel);
  }

  if (S_ISDIR(st.st_mode)) {
    if (tar_name[strlen(tar_name) - 1] != '/') {
      size_t tn = strlen(tar_name);
      if (tn + 1 < sizeof(tar_name)) {
        tar_name[tn] = '/';
        tar_name[tn + 1] = '\0';
      }
    }
    if (tar_add_header(b, tar_name, &st, '5', 0) != 0)
      return -1;

    DIR *d = opendir(fs_path);
    if (!d)
      return -1;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
      if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
        continue;
      char next_rel[4096];
      if (rel[0] == '\0')
        snprintf(next_rel, sizeof(next_rel), "%s", de->d_name);
      else
        snprintf(next_rel, sizeof(next_rel), "%s/%s", rel, de->d_name);
      if (tar_add_path_recursive(b, root_fs, root_name, next_rel) != 0) {
        closedir(d);
        return -1;
      }
    }
    closedir(d);
    return 0;
  }

  if (S_ISREG(st.st_mode)) {
    size_t fsz = (size_t)st.st_size;
    if (tar_add_header(b, tar_name, &st, '0', (uint64_t)fsz) != 0)
      return -1;
    unsigned char *data = NULL;
    size_t rd = 0;
    data = read_file(fs_path, &rd);
    if (!data && fsz != 0)
      return -1;
    int rc = tar_add_file_data(b, data, rd);
    free(data);
    return rc;
  }

  return 0;
}

static unsigned char *read_tar_stream_from_dir(const char *dir_path,
                                                size_t *out_size) {
  if (out_size)
    *out_size = 0;
  if (!dir_path)
    return NULL;

  char *dir_dup = dup_trim_trailing_slashes(dir_path);
  if (!dir_dup)
    return NULL;
  const char *base = basename(dir_dup);
  if (!base || !*base)
    base = "archive";

  struct dynbuf b;
  memset(&b, 0, sizeof(b));
  int rc = tar_add_path_recursive(&b, dir_dup, base, "");
  free(dir_dup);
  if (rc != 0) {
    free(b.p);
    return NULL;
  }

  unsigned char z[1024];
  memset(z, 0, sizeof(z));
  if (db_append(&b, z, sizeof(z)) != 0) {
    free(b.p);
    return NULL;
  }

  if (out_size)
    *out_size = b.sz;
  return b.p;
}

static uint64_t tar_parse_octal(const char *s, size_t n) {
  uint64_t v = 0;
  for (size_t i = 0; i < n; ++i) {
    char c = s[i];
    if (c == '\0' || c == ' ')
      break;
    if (c < '0' || c > '7')
      break;
    v = (v << 3) + (uint64_t)(c - '0');
  }
  return v;
}

static int untar_stream_to_dir(const unsigned char *tar, size_t tar_size,
                                const char *out_dir) {
  if (!tar || !out_dir)
    return -1;
  if ((tar_size & 511u) != 0)
    return -1;
  if (ensure_dir_exists(out_dir) != 0)
    return -1;

  size_t off = 0;
  while (off + 512 <= tar_size) {
    const unsigned char *hdr = tar + off;
    int allz = 1;
    for (size_t i = 0; i < 512; ++i)
      if (hdr[i] != 0) {
        allz = 0;
        break;
      }
    if (allz)
      break;

    char name0[101];
    memcpy(name0, hdr + 0, 100);
    name0[100] = '\0';
    char prefix[156];
    memcpy(prefix, hdr + 345, 155);
    prefix[155] = '\0';
    char name_full[512];
    if (prefix[0])
      snprintf(name_full, sizeof(name_full), "%s/%s", prefix, name0);
    else
      snprintf(name_full, sizeof(name_full), "%s", name0);

    char rel[4096];
    if (sanitize_relpath(name_full, rel, sizeof(rel)) != 0)
      return -1;

    uint64_t fsz = tar_parse_octal((const char *)(hdr + 124), 12);
    char typeflag = (char)hdr[156];
    off += 512;

    char out_path[8192];
    snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, rel);

    if (typeflag == '5') {
      if (ensure_dir_exists(out_path) != 0)
        return -1;
    } else {
      if (ensure_parent_dir_exists(out_path) != 0)
        return -1;
      FILE *f = fopen(out_path, "wb");
      if (!f)
        return -1;
      if (fsz) {
        if (off + (size_t)fsz > tar_size) {
          fclose(f);
          return -1;
        }
        if (fwrite(tar + off, 1, (size_t)fsz, f) != (size_t)fsz) {
          fclose(f);
          return -1;
        }
      }
      fclose(f);
    }

    size_t adv = (size_t)((fsz + 511u) & ~(uint64_t)511u);
    if (off + adv > tar_size)
      return -1;
    off += adv;
  }
  return 0;
}

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

static int write_all_fd(int fd, const void *data, size_t size) {
  const unsigned char *p = (const unsigned char *)data;
  size_t left = size;
  while (left) {
    ssize_t w = write(fd, p, left);
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

#define ZLF_VERSION 1u
#define ZLF_FLAG_TAR 1u

#define SFX_FOOTER_MAGIC0 0x00584653464c457au
#define SFX_FOOTER_VER 1u

static uint32_t crc32_ieee(const void *data, size_t n) {
  static uint32_t table[256];
  static int have_table = 0;
  if (!have_table) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (uint32_t k = 0; k < 8; ++k)
        c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
      table[i] = c;
    }
    have_table = 1;
  }
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i)
    crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
  return ~crc;
}

static int write_sfx_executable(const char *out_path,
                                const unsigned char *stub_data,
                                size_t stub_size,
                                const unsigned char *payload,
                                size_t payload_size, uint32_t footer_flags) {
  if (!out_path || !stub_data || !payload)
    return -1;

  const size_t stub_file_off = 0x1000u;
  const Elf64_Addr stub_vaddr = 0x5000u;
  const size_t hdr_sz = sizeof(Elf64_Ehdr) + 2u * sizeof(Elf64_Phdr);
  if (hdr_sz > stub_file_off)
    return -1;

  Elf64_Ehdr eh;
  memset(&eh, 0, sizeof(eh));
  eh.e_ident[EI_MAG0] = ELFMAG0;
  eh.e_ident[EI_MAG1] = ELFMAG1;
  eh.e_ident[EI_MAG2] = ELFMAG2;
  eh.e_ident[EI_MAG3] = ELFMAG3;
  eh.e_ident[EI_CLASS] = ELFCLASS64;
  eh.e_ident[EI_DATA] = ELFDATA2LSB;
  eh.e_ident[EI_VERSION] = EV_CURRENT;
  eh.e_type = ET_DYN;
 #if defined(__aarch64__)
  eh.e_machine = EM_AARCH64;
 #else
  eh.e_machine = EM_X86_64;
 #endif
  eh.e_version = EV_CURRENT;
  eh.e_entry = stub_vaddr;
  eh.e_phoff = sizeof(Elf64_Ehdr);
  eh.e_ehsize = sizeof(Elf64_Ehdr);
  eh.e_phentsize = sizeof(Elf64_Phdr);
  eh.e_phnum = 2;

  Elf64_Phdr ph[2];
  memset(ph, 0, sizeof(ph));
  ph[0].p_type = PT_LOAD;
  ph[0].p_offset = (Elf64_Off)stub_file_off;
  ph[0].p_vaddr = stub_vaddr;
  ph[0].p_paddr = stub_vaddr;
  ph[0].p_filesz = stub_size;
  ph[0].p_memsz = stub_size;
  ph[0].p_flags = PF_R | PF_X;
  ph[0].p_align = 0x1000u;

  ph[1].p_type = PT_GNU_STACK;
  ph[1].p_flags = PF_R | PF_W;
  ph[1].p_align = 0x10u;

  struct {
    uint64_t magic;
    uint32_t ver;
    uint32_t flags;
    uint64_t payload_off;
    uint64_t payload_size;
  } footer;

  footer.magic = SFX_FOOTER_MAGIC0;
  footer.ver = SFX_FOOTER_VER;
  footer.flags = footer_flags;
  footer.payload_off = (uint64_t)stub_file_off + (uint64_t)stub_size;
  footer.payload_size = (uint64_t)payload_size;

  int fd = -1;
  if (strcmp(out_path, "-") == 0) {
    fd = STDOUT_FILENO;
  } else {
    fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd < 0)
      return -1;
  }

  int rc = 0;
  if (write_all_fd(fd, &eh, sizeof(eh)) != 0)
    rc = -1;
  if (!rc && write_all_fd(fd, ph, sizeof(ph)) != 0)
    rc = -1;

  if (!rc) {
    unsigned char zpad[256];
    memset(zpad, 0, sizeof(zpad));
    size_t cur = hdr_sz;
    while (cur < stub_file_off) {
      size_t want = stub_file_off - cur;
      if (want > sizeof(zpad))
        want = sizeof(zpad);
      if (write_all_fd(fd, zpad, want) != 0) {
        rc = -1;
        break;
      }
      cur += want;
    }
  }

  if (!rc && write_all_fd(fd, stub_data, stub_size) != 0)
    rc = -1;
  if (!rc && write_all_fd(fd, payload, payload_size) != 0)
    rc = -1;
  if (!rc && write_all_fd(fd, &footer, sizeof(footer)) != 0)
    rc = -1;

  if (fd != STDOUT_FILENO)
    close(fd);
  return rc;
}

static const char *marker_for_codec(const char *codec) {
  if (strcmp(codec, "lz4") == 0)
    return "zELFl4";
  if (strcmp(codec, "apultra") == 0)
    return "zELFap";
  if (strcmp(codec, "zx7b") == 0)
    return "zELFzx";
  if (strcmp(codec, "zx0") == 0)
    return "zELFz0";
  if (strcmp(codec, "zstd") == 0)
    return "zELFzd";
  if (strcmp(codec, "blz") == 0)
    return "zELFbz";
  if (strcmp(codec, "exo") == 0)
    return "zELFex";
  if (strcmp(codec, "pp") == 0)
    return "zELFpp";
  if (strcmp(codec, "snappy") == 0)
    return "zELFsn";
  if (strcmp(codec, "doboz") == 0)
    return "zELFdz";
  if (strcmp(codec, "qlz") == 0)
    return "zELFqz";
  if (strcmp(codec, "lzav") == 0)
    return "zELFlv";
  if (strcmp(codec, "lzma") == 0)
    return "zELFla";
  if (strcmp(codec, "shrinkler") == 0)
    return "zELFsh";
  if (strcmp(codec, "sc") == 0)
    return "zELFsc";
  if (strcmp(codec, "lzsa") == 0 || strcmp(codec, "lzsa2") == 0)
    return "zELFls";
  if (strcmp(codec, "density") == 0)
    return "zELFde";
  if (strcmp(codec, "lzham") == 0)
    return "zELFlz";
  if (strcmp(codec, "rnc") == 0)
    return "zELFrn";
  if (strcmp(codec, "lzfse") == 0)
    return "zELFse";
  if (strcmp(codec, "csc") == 0)
    return "zELFcs";
  if (strcmp(codec, "nz1") == 0)
    return "zELFnz";
  return "zELFl4";
}

static const char *codec_name_for_marker(const char *marker) {
  if (!marker)
    return "Unknown";
  if (strcmp(marker, "zELFl4") == 0)
    return "LZ4";
  if (strcmp(marker, "zELFap") == 0)
    return "Apultra";
  if (strcmp(marker, "zELFzx") == 0)
    return "ZX7B";
  if (strcmp(marker, "zELFz0") == 0)
    return "ZX0";
  if (strcmp(marker, "zELFzd") == 0)
    return "ZSTD";
  if (strcmp(marker, "zELFbz") == 0)
    return "BriefLZ";
  if (strcmp(marker, "zELFex") == 0)
    return "Exomizer";
  if (strcmp(marker, "zELFpp") == 0)
    return "PowerPacker";
  if (strcmp(marker, "zELFsn") == 0)
    return "Snappy";
  if (strcmp(marker, "zELFdz") == 0)
    return "Doboz";
  if (strcmp(marker, "zELFqz") == 0)
    return "QuickLZ";
  if (strcmp(marker, "zELFlv") == 0)
    return "LZAV";
  if (strcmp(marker, "zELFla") == 0)
    return "LZMA";
  if (strcmp(marker, "zELFsh") == 0)
    return "Shrinkler";
  if (strcmp(marker, "zELFsc") == 0)
    return "StoneCracker";
  if (strcmp(marker, "zELFls") == 0)
    return "LZSA2";
  if (strcmp(marker, "zELFde") == 0)
    return "Density";
  if (strcmp(marker, "zELFlz") == 0)
    return "LZHAM";
  if (strcmp(marker, "zELFrn") == 0)
    return "RNC";
  if (strcmp(marker, "zELFse") == 0)
    return "LZFSE";
  if (strcmp(marker, "zELFcs") == 0)
    return "CSC";
  if (strcmp(marker, "zELFnz") == 0)
    return "Nanozip";
  return "Unknown";
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

  const uint32_t crc32 = crc32_ieee(input_data, input_size);

  char *base_dup_name = NULL;
  const char *stored_name = "stdin";
  if (input_path && strcmp(input_path, "-") != 0) {
    base_dup_name = strdup(input_path);
    if (!base_dup_name) {
      free(input_data);
      free(compressed_data);
      return 1;
    }
    stored_name = basename(base_dup_name);
    if (!stored_name || !*stored_name)
      stored_name = "archive";
  }
  size_t stored_name_len = strlen(stored_name);
  if (stored_name_len > 0xFFFFu) {
    free(base_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  const char *marker = marker_for_codec(codec);
  const size_t header_size = 6 + 4 + 4 + 8 + 4 + 4 + 2 + stored_name_len;
  const size_t total_size = header_size + (size_t)compressed_size;
  unsigned char *final_data = (unsigned char *)malloc(total_size);
  if (!final_data) {
    free(base_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  unsigned char *p = final_data;
  memcpy(p, marker, 6);
  p += 6;
  {
    const uint32_t ver = (uint32_t)ZLF_VERSION;
    const uint32_t flags = 0;
    const uint16_t nlen = (uint16_t)stored_name_len;
    memcpy(p, &ver, 4);
    p += 4;
    memcpy(p, &flags, 4);
    p += 4;
    memcpy(p, &input_size, 8);
    p += 8;
    memcpy(p, &compressed_size, 4);
    p += 4;
    memcpy(p, &crc32, 4);
    p += 4;
    memcpy(p, &nlen, 2);
    p += 2;
    memcpy(p, stored_name, stored_name_len);
    p += stored_name_len;
  }
  memcpy(p, compressed_data, (size_t)compressed_size);

  char final_out[4096];
  if (output_path) {
    if (strcmp(output_path, "-") == 0) {
      strncpy(final_out, "-", sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    } else {
      struct stat st;
      if (stat(output_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        const char *base = stored_name;
        if (!base || !*base)
          base = "archive";
        snprintf(final_out, sizeof(final_out), "%s/%s.zlf", output_path, base);
      } else {
        strncpy(final_out, output_path, sizeof(final_out) - 1);
        final_out[sizeof(final_out) - 1] = '\0';
      }
    }
  } else {
    const char *base = stored_name;
    if (!base || !*base)
      base = "archive";
    snprintf(final_out, sizeof(final_out), "./%s.zlf", base);
  }

  if (write_file(final_out, final_data, total_size) != 0) {
    fprintf(stderr, "Error: Cannot write output %s\n", final_out);
    if (strcmp(final_out, "-") != 0)
      (void)unlink(final_out);
    free(final_data);
    free(base_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  fprintf(logf, "[%s%s%s] Written to %s\n", PK_OK, PK_SYM_OK, PK_RES,
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
  free(base_dup_name);
  return 0;
}

int archive_tar_dir(const char *dir_path, const char *output_path,
                    const char *codec) {
  FILE *logf = stdout;
  if (output_path && strcmp(output_path, "-") == 0)
    logf = stderr;

  struct stat st;
  if (!dir_path || stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "Error: Invalid directory %s\n",
            dir_path ? dir_path : "(null)");
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

  char *dir_dup_name = dup_trim_trailing_slashes(dir_path);
  if (!dir_dup_name) {
    free(input_data);
    return 1;
  }
  const char *stored_name = basename(dir_dup_name);
  if (!stored_name || !*stored_name)
    stored_name = "archive";
  size_t stored_name_len = strlen(stored_name);
  if (stored_name_len > 0xFFFFu) {
    free(dir_dup_name);
    free(input_data);
    return 1;
  }

  const uint32_t crc32 = crc32_ieee(input_data, input_size);

  fprintf(logf, "[%s%s%s] Archiving %s (tar) with %s...\n", PK_INFO,
          PK_SYM_INFO, PK_RES, dir_path, codec);
  fprintf(logf, "[%s%s%s] Input size: %zu bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, input_size);

  if (compress_data_with_codec(codec, input_data, input_size, &compressed_data,
                               &compressed_size) != 0) {
    fprintf(stderr, "Error: Compression failed\n");
    free(dir_dup_name);
    free(input_data);
    return 1;
  }

  const char *marker = marker_for_codec(codec);
  const size_t header_size = 6 + 4 + 4 + 8 + 4 + 4 + 2 + stored_name_len;
  const size_t total_size = header_size + (size_t)compressed_size;
  unsigned char *final_data = (unsigned char *)malloc(total_size);
  if (!final_data) {
    free(dir_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  unsigned char *p = final_data;
  memcpy(p, marker, 6);
  p += 6;
  {
    const uint32_t ver = (uint32_t)ZLF_VERSION;
    const uint32_t flags = (uint32_t)ZLF_FLAG_TAR;
    const uint16_t nlen = (uint16_t)stored_name_len;
    memcpy(p, &ver, 4);
    p += 4;
    memcpy(p, &flags, 4);
    p += 4;
    memcpy(p, &input_size, 8);
    p += 8;
    memcpy(p, &compressed_size, 4);
    p += 4;
    memcpy(p, &crc32, 4);
    p += 4;
    memcpy(p, &nlen, 2);
    p += 2;
    memcpy(p, stored_name, stored_name_len);
    p += stored_name_len;
  }
  memcpy(p, compressed_data, (size_t)compressed_size);

  char final_out[4096];
  if (output_path) {
    if (strcmp(output_path, "-") == 0) {
      strncpy(final_out, "-", sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    } else {
      struct stat st_out;
      if (stat(output_path, &st_out) == 0 && S_ISDIR(st_out.st_mode)) {
        snprintf(final_out, sizeof(final_out), "%s/%s.tar.zlf", output_path,
                 stored_name);
      } else {
        strncpy(final_out, output_path, sizeof(final_out) - 1);
        final_out[sizeof(final_out) - 1] = '\0';
      }
    }
  } else {
    snprintf(final_out, sizeof(final_out), "./%s.tar.zlf", stored_name);
  }

  if (write_file(final_out, final_data, total_size) != 0) {
    fprintf(stderr, "Error: Cannot write output %s\n", final_out);
    if (strcmp(final_out, "-") != 0)
      (void)unlink(final_out);
    free(final_data);
    free(dir_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  fprintf(logf, "[%s%s%s] Written to %s\n", PK_OK, PK_SYM_OK, PK_RES,
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
  free(dir_dup_name);
  return 0;
}

int archive_sfx_file(const char *input_path, const char *output_path,
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

  fprintf(logf, "[%s%s%s] Archiving %s (SFX) with %s...\n", PK_INFO,
          PK_SYM_INFO, PK_RES, input_path, codec);
  fprintf(logf, "[%s%s%s] Input size: %zu bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, input_size);

  if (compress_data_with_codec(codec, input_data, input_size, &compressed_data,
                               &compressed_size) != 0) {
    fprintf(stderr, "Error: Compression failed\n");
    free(input_data);
    return 1;
  }

  const uint32_t crc32 = crc32_ieee(input_data, input_size);

  char *base_dup_name = NULL;
  const char *stored_name = "stdin";
  if (input_path && strcmp(input_path, "-") != 0) {
    base_dup_name = strdup(input_path);
    if (!base_dup_name) {
      free(input_data);
      free(compressed_data);
      return 1;
    }
    stored_name = basename(base_dup_name);
    if (!stored_name || !*stored_name)
      stored_name = "archive";
  }
  size_t stored_name_len = strlen(stored_name);
  if (stored_name_len > 0xFFFFu) {
    free(base_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  const char *marker = marker_for_codec(codec);
  const size_t header_size = 6 + 4 + 4 + 8 + 4 + 4 + 2 + stored_name_len;
  const size_t payload_size = header_size + (size_t)compressed_size;
  unsigned char *payload = (unsigned char *)malloc(payload_size);
  if (!payload) {
    free(base_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  unsigned char *p = payload;
  memcpy(p, marker, 6);
  p += 6;
  {
    const uint32_t ver = (uint32_t)ZLF_VERSION;
    const uint32_t flags = 0;
    const uint16_t nlen = (uint16_t)stored_name_len;
    memcpy(p, &ver, 4);
    p += 4;
    memcpy(p, &flags, 4);
    p += 4;
    memcpy(p, &input_size, 8);
    p += 8;
    memcpy(p, &compressed_size, 4);
    p += 4;
    memcpy(p, &crc32, 4);
    p += 4;
    memcpy(p, &nlen, 2);
    p += 2;
    memcpy(p, stored_name, stored_name_len);
    p += stored_name_len;
  }
  memcpy(p, compressed_data, (size_t)compressed_size);

  const unsigned char *stub_s = NULL;
  const unsigned char *stub_e = NULL;
  select_embedded_sfx_stub(codec, &stub_s, &stub_e);
  if (!stub_s || !stub_e || stub_e <= stub_s) {
    fprintf(stderr, "Error: No embedded SFX stub for codec %s\n", codec);
    free(payload);
    free(base_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }
  const size_t stub_sz = (size_t)(stub_e - stub_s);

  char final_out[4096];
  if (output_path) {
    if (strcmp(output_path, "-") == 0) {
      strncpy(final_out, "-", sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    } else {
      struct stat st;
      if (stat(output_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        const char *base = stored_name;
        if (!base || !*base)
          base = "archive";
        snprintf(final_out, sizeof(final_out), "%s/%s.sfx", output_path, base);
      } else {
        strncpy(final_out, output_path, sizeof(final_out) - 1);
        final_out[sizeof(final_out) - 1] = '\0';
      }
    }
  } else {
    const char *base = stored_name;
    if (!base || !*base)
      base = "archive";
    snprintf(final_out, sizeof(final_out), "./%s.sfx", base);
  }

  if (write_sfx_executable(final_out, stub_s, stub_sz, payload, payload_size,
                           0) != 0) {
    fprintf(stderr, "Error: Cannot write SFX output %s\n", final_out);
    if (strcmp(final_out, "-") != 0)
      (void)unlink(final_out);
    free(payload);
    free(base_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  fprintf(logf, "[%s%s%s] SFX written to %s\n", PK_OK, PK_SYM_OK, PK_RES,
          final_out);
  fprintf(logf, "[%s%s%s] Compressed size: %d bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, compressed_size);
  fprintf(logf, "[%s%s%s] Ratio: %.1f%%\n", PK_INFO, PK_SYM_INFO, PK_RES,
          100.0 * compressed_size / input_size);
  free(payload);
  free(base_dup_name);
  free(input_data);
  free(compressed_data);
  return 0;
}

int archive_sfx_tar_dir(const char *dir_path, const char *output_path,
                        const char *codec) {
  FILE *logf = stdout;
  if (output_path && strcmp(output_path, "-") == 0)
    logf = stderr;

  struct stat st;
  if (!dir_path || stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "Error: Invalid directory %s\n",
            dir_path ? dir_path : "(null)");
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

  char *dir_dup_name = dup_trim_trailing_slashes(dir_path);
  if (!dir_dup_name) {
    free(input_data);
    return 1;
  }
  const char *stored_name = basename(dir_dup_name);
  if (!stored_name || !*stored_name)
    stored_name = "archive";
  size_t stored_name_len = strlen(stored_name);
  if (stored_name_len > 0xFFFFu) {
    free(dir_dup_name);
    free(input_data);
    return 1;
  }

  const uint32_t crc32 = crc32_ieee(input_data, input_size);

  fprintf(logf, "[%s%s%s] Archiving %s (tar SFX) with %s...\n", PK_INFO,
          PK_SYM_INFO, PK_RES, dir_path, codec);
  fprintf(logf, "[%s%s%s] Input size: %zu bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, input_size);

  if (compress_data_with_codec(codec, input_data, input_size, &compressed_data,
                               &compressed_size) != 0) {
    fprintf(stderr, "Error: Compression failed\n");
    free(dir_dup_name);
    free(input_data);
    return 1;
  }

  const char *marker = marker_for_codec(codec);
  const size_t header_size = 6 + 4 + 4 + 8 + 4 + 4 + 2 + stored_name_len;
  const size_t payload_size = header_size + (size_t)compressed_size;
  unsigned char *payload = (unsigned char *)malloc(payload_size);
  if (!payload) {
    free(dir_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  unsigned char *p = payload;
  memcpy(p, marker, 6);
  p += 6;
  {
    const uint32_t ver = (uint32_t)ZLF_VERSION;
    const uint32_t flags = (uint32_t)ZLF_FLAG_TAR;
    const uint16_t nlen = (uint16_t)stored_name_len;
    memcpy(p, &ver, 4);
    p += 4;
    memcpy(p, &flags, 4);
    p += 4;
    memcpy(p, &input_size, 8);
    p += 8;
    memcpy(p, &compressed_size, 4);
    p += 4;
    memcpy(p, &crc32, 4);
    p += 4;
    memcpy(p, &nlen, 2);
    p += 2;
    memcpy(p, stored_name, stored_name_len);
    p += stored_name_len;
  }
  memcpy(p, compressed_data, (size_t)compressed_size);

  const unsigned char *stub_s = NULL;
  const unsigned char *stub_e = NULL;
  select_embedded_sfx_stub(codec, &stub_s, &stub_e);
  if (!stub_s || !stub_e || stub_e <= stub_s) {
    fprintf(stderr, "Error: No embedded SFX stub for codec %s\n", codec);
    free(payload);
    free(dir_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }
  const size_t stub_sz = (size_t)(stub_e - stub_s);

  char final_out[4096];
  if (output_path) {
    if (strcmp(output_path, "-") == 0) {
      strncpy(final_out, "-", sizeof(final_out) - 1);
      final_out[sizeof(final_out) - 1] = '\0';
    } else {
      struct stat st_out;
      if (stat(output_path, &st_out) == 0 && S_ISDIR(st_out.st_mode)) {
        snprintf(final_out, sizeof(final_out), "%s/%s.tar.sfx", output_path,
                 stored_name);
      } else {
        strncpy(final_out, output_path, sizeof(final_out) - 1);
        final_out[sizeof(final_out) - 1] = '\0';
      }
    }
  } else {
    snprintf(final_out, sizeof(final_out), "./%s.tar.sfx", stored_name);
  }

  if (write_sfx_executable(final_out, stub_s, stub_sz, payload, payload_size,
                           ZLF_FLAG_TAR) != 0) {
    fprintf(stderr, "Error: Cannot write SFX output %s\n", final_out);
    if (strcmp(final_out, "-") != 0)
      (void)unlink(final_out);
    free(payload);
    free(dir_dup_name);
    free(input_data);
    free(compressed_data);
    return 1;
  }

  fprintf(logf, "[%s%s%s] SFX written to %s\n", PK_OK, PK_SYM_OK, PK_RES,
          final_out);
  fprintf(logf, "[%s%s%s] Compressed size: %d bytes\n", PK_INFO, PK_SYM_INFO,
          PK_RES, compressed_size);
  fprintf(logf, "[%s%s%s] Ratio: %.1f%%\n", PK_INFO, PK_SYM_INFO, PK_RES,
          100.0 * compressed_size / input_size);
  free(payload);
  free(dir_dup_name);
  free(input_data);
  free(compressed_data);
  return 0;
}

int archive_list(const char *input_path) {
  FILE *out = g_ui_stream ? g_ui_stream : stdout;
  size_t file_size = 0;
  unsigned char *file_data = read_file(input_path, &file_size);
  if (!file_data) {
    fprintf(stderr, "Error: Cannot read input file %s\n",
            input_path ? input_path : "(null)");
    return 1;
  }

  const unsigned char *zlf = file_data;
  size_t zlf_size = file_size;
  int is_sfx = 0;

  struct {
    uint64_t magic;
    uint32_t ver;
    uint32_t flags;
    uint64_t payload_off;
    uint64_t payload_size;
  } footer;

  if (file_size >= sizeof(footer)) {
    memcpy(&footer, file_data + (file_size - sizeof(footer)), sizeof(footer));
    if (footer.magic == SFX_FOOTER_MAGIC0 && footer.ver == SFX_FOOTER_VER) {
      if (footer.payload_off <= (uint64_t)file_size &&
          footer.payload_size <= (uint64_t)file_size &&
          footer.payload_off + footer.payload_size <=
              (uint64_t)file_size - (uint64_t)sizeof(footer)) {
        zlf = file_data + (size_t)footer.payload_off;
        zlf_size = (size_t)footer.payload_size;
        is_sfx = 1;
      }
    }
  }

  if (zlf_size < (6 + 4 + 4 + 8 + 4 + 4 + 2)) {
    fprintf(stderr, "Error: File too small to be a valid archive\n");
    free(file_data);
    return 1;
  }

  const unsigned char *p = zlf;
  char marker[7];
  memcpy(marker, p, 6);
  marker[6] = '\0';
  p += 6;

  uint32_t ver;
  memcpy(&ver, p, 4);
  p += 4;
  if (ver != ZLF_VERSION) {
    fprintf(stderr, "Error: Unsupported archive format\n");
    free(file_data);
    return 1;
  }

  uint32_t flags;
  memcpy(&flags, p, 4);
  p += 4;

  uint64_t orig_size;
  memcpy(&orig_size, p, 8);
  p += 8;

  uint32_t comp_size;
  memcpy(&comp_size, p, 4);
  p += 4;

  uint32_t crc32;
  memcpy(&crc32, p, 4);
  p += 4;

  uint16_t name_len;
  memcpy(&name_len, p, 2);
  p += 2;

  if ((size_t)(p - zlf) + (size_t)name_len > zlf_size) {
    fprintf(stderr, "Error: Truncated archive header\n");
    free(file_data);
    return 1;
  }
  if ((size_t)(p - zlf) + (size_t)name_len + (size_t)comp_size > zlf_size) {
    fprintf(stderr, "Error: Truncated archive payload\n");
    free(file_data);
    return 1;
  }

  char name_buf[4096];
  name_buf[0] = '\0';
  if (name_len) {
    size_t n = (size_t)name_len;
    if (n >= sizeof(name_buf))
      n = sizeof(name_buf) - 1;
    memcpy(name_buf, p, n);
    name_buf[n] = '\0';
  }

  const int is_tar = (flags & ZLF_FLAG_TAR) ? 1 : 0;

  fprintf(out, "[%s%s%s] Archive info:\n", PK_ARROW, PK_SYM_ARROW, PK_RES);
  fprintf(out, "[%s%s%s] Container: %s\n", PK_INFO, PK_SYM_INFO, PK_RES,
          is_sfx ? "ZLF-SFX" : "ZLF");
  fprintf(out, "[%s%s%s] Type: %s\n", PK_INFO, PK_SYM_INFO, PK_RES,
          is_tar ? "tar" : "single");
  fprintf(out, "[%s%s%s] Codec: %s\n", PK_INFO, PK_SYM_INFO, PK_RES,
          codec_name_for_marker(marker));
  fprintf(out, "[%s%s%s] Stored name: %s\n", PK_INFO, PK_SYM_INFO, PK_RES,
          name_buf[0] ? name_buf : "(none)");
  fprintf(out, "[%s%s%s] Uncompressed size: %llu bytes\n", PK_INFO,
          PK_SYM_INFO, PK_RES, (unsigned long long)orig_size);
  fprintf(out, "[%s%s%s] CRC32: 0x%08x\n", PK_INFO, PK_SYM_INFO, PK_RES,
          (unsigned)crc32);

  free(file_data);
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

  if (file_size < (6 + 4 + 4 + 8 + 4 + 4 + 2)) {
    fprintf(stderr, "Error: File too small to be a valid archive\n");
    free(file_data);
    return 1;
  }

  unsigned char *p = file_data;
  char marker[7];
  memcpy(marker, p, 6);
  marker[6] = '\0';
  p += 6;

  uint32_t ver;
  memcpy(&ver, p, 4);
  p += 4;
  if (ver != ZLF_VERSION) {
    fprintf(stderr, "Error: Unsupported archive format\n");
    free(file_data);
    return 1;
  }

  uint32_t flags;
  memcpy(&flags, p, 4);
  p += 4;

  size_t orig_size;
  memcpy(&orig_size, p, sizeof(orig_size));
  p += 8;
  int comp_size;
  memcpy(&comp_size, p, sizeof(comp_size));
  p += 4;

  uint32_t expected_crc32;
  memcpy(&expected_crc32, p, 4);
  p += 4;

  uint16_t name_len;
  memcpy(&name_len, p, 2);
  p += 2;

  if (name_len > 0) {
    if ((size_t)(p - file_data) + (size_t)name_len > file_size) {
      fprintf(stderr, "Error: Truncated archive\n");
      free(file_data);
      return 1;
    }
  }

  char stored_name_buf[4096];
  stored_name_buf[0] = '\0';
  if (name_len > 0) {
    size_t n = (size_t)name_len;
    if (n >= sizeof(stored_name_buf))
      n = sizeof(stored_name_buf) - 1;
    memcpy(stored_name_buf, p, n);
    stored_name_buf[n] = '\0';
    p += (size_t)name_len;
  }

  if (comp_size < 0) {
    fprintf(stderr, "Error: Invalid compressed size\n");
    free(file_data);
    return 1;
  }

  if ((size_t)(p - file_data) + (size_t)comp_size > file_size) {
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

  if ((size_t)ret != orig_size) {
    fprintf(stderr, "Error: Decompression size mismatch\n");
    free(decompressed);
    free(file_data);
    return 1;
  }

  if (crc32_ieee(decompressed, orig_size) != expected_crc32) {
    fprintf(stderr, "Error: CRC mismatch\n");
    free(decompressed);
    free(file_data);
    return 1;
  }

  int want_untar = (flags & ZLF_FLAG_TAR) ? 1 : 0;
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
      struct stat st_dir;
      if (stat(output_path, &st_dir) == 0) {
        if (!S_ISDIR(st_dir.st_mode)) {
          fprintf(stderr, "Error: Output path is not a directory: %s\n",
                  output_path);
          free(decompressed);
          free(file_data);
          return 1;
        }
      } else {
        if (errno != ENOENT) {
          fprintf(stderr, "Error: Cannot stat output path %s: %s\n", output_path,
                  strerror(errno));
          free(decompressed);
          free(file_data);
          return 1;
        }
      }

      if (ensure_dir_exists(output_path) != 0) {
        fprintf(stderr, "Error: Cannot create output directory %s: %s\n",
                output_path, strerror(errno));
        free(decompressed);
        free(file_data);
        return 1;
      }

      if (stored_name_buf[0] != '\0') {
        snprintf(out_dir, sizeof(out_dir), "%s/%s", output_path, stored_name_buf);
      } else {
        strncpy(out_dir, output_path, sizeof(out_dir) - 1);
        out_dir[sizeof(out_dir) - 1] = '\0';
      }
    } else {
      if (stored_name_buf[0] != '\0') {
        snprintf(out_dir, sizeof(out_dir), "./%s", stored_name_buf);
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
        if (stored_name_buf[0] != '\0') {
          snprintf(final_out, sizeof(final_out), "%s/%s", output_path,
                   stored_name_buf);
        } else {
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
        }
      } else {
        strncpy(final_out, output_path, sizeof(final_out) - 1);
        final_out[sizeof(final_out) - 1] = '\0';
      }
    }
  } else {
    if (stored_name_buf[0] != '\0') {
      snprintf(final_out, sizeof(final_out), "./%s", stored_name_buf);
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
  }

  if (write_file(final_out, decompressed, orig_size) != 0) {
    fprintf(stderr, "Error: Cannot write output file %s\n", final_out);
    if (strcmp(final_out, "-") != 0)
      (void)unlink(final_out);
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
