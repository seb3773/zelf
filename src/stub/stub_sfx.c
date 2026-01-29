#include "codec_select.h"
#include "codec_marker.h"
#include "stub_defs.h"
#include "stub_syscalls.h"
#include "stub_utils.h"
#include <stddef.h>
#include <stdint.h>

#define SFX_FOOTER_MAGIC0 0x00584653464c457au /* "zELFSFX\0" little-endian */
#define SFX_FOOTER_VER 1u

#define ZLF_VERSION 1u
#define ZLF_FLAG_TAR 1u

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define O_WRONLY 1
#define O_CREAT 64
#define O_EXCL 128
#define O_TRUNC 512

#define EINVAL 22
#define ENOENT 2
#define EEXIST 17

#define SYS_mkdir 83
#define SYS_unlink 87
#define SYS_rmdir 84

static inline long z_syscall_mkdir(const char *pathname, int mode) {
  register long rax asm("rax") = SYS_mkdir;
  register long rdi asm("rdi") = (long)pathname;
  register long rsi asm("rsi") = mode;
  register long result;
  asm volatile("syscall" : "=a"(result) : "a"(rax), "D"(rdi), "S"(rsi)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_unlink(const char *pathname) {
  register long rax asm("rax") = SYS_unlink;
  register long rdi asm("rdi") = (long)pathname;
  register long result;
  asm volatile("syscall" : "=a"(result) : "a"(rax), "D"(rdi)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_rmdir(const char *pathname) {
  register long rax asm("rax") = SYS_rmdir;
  register long rdi asm("rdi") = (long)pathname;
  register long result;
  asm volatile("syscall" : "=a"(result) : "a"(rax), "D"(rdi)
               : "rcx", "r11", "memory");
  return result;
}

static inline uint32_t crc32_ieee(const void *data, size_t n) {
  static const uint32_t table[256] = {
      0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u,
      0x706AF48Fu, 0xE963A535u, 0x9E6495A3u, 0x0EDB8832u, 0x79DCB8A4u,
      0xE0D5E91Eu, 0x97D2D988u, 0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u,
      0x90BF1D91u, 0x1DB71064u, 0x6AB020F2u, 0xF3B97148u, 0x84BE41DEu,
      0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u, 0x136C9856u,
      0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u,
      0xFA0F3D63u, 0x8D080DF5u, 0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u,
      0xA2677172u, 0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu,
      0x35B5A8FAu, 0x42B2986Cu, 0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u,
      0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u, 0x26D930ACu, 0x51DE003Au,
      0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u, 0xCFBA9599u,
      0xB8BDA50Fu, 0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u,
      0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du, 0x76DC4190u,
      0x01DB7106u, 0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu,
      0x9FBFE4A5u, 0xE8B8D433u, 0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu,
      0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du, 0x91646C97u, 0xE6635C01u,
      0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu, 0x6C0695EDu,
      0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u, 0x65B0D9C6u, 0x12B7E950u,
      0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u,
      0xFBD44C65u, 0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u,
      0x4ADFA541u, 0x3DD895D7u, 0xA4D1C46Du, 0xD3D6F4FBu, 0x4369E96Au,
      0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u, 0x44042D73u, 0x33031DE5u,
      0xAA0A4C5Fu, 0xDD0D7CC9u, 0x5005713Cu, 0x270241AAu, 0xBE0B1010u,
      0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu,
      0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u,
      0x2EB40D81u, 0xB7BD5C3Bu, 0xC0BA6CADu, 0xEDB88320u, 0x9ABFB3B6u,
      0x03B6E20Cu, 0x74B1D29Au, 0xEAD54739u, 0x9DD277AFu, 0x04DB2615u,
      0x73DC1683u, 0xE3630B12u, 0x94643B84u, 0x0D6D6A3Eu, 0x7A6A5AA8u,
      0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u, 0xF00F9344u,
      0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu,
      0x196C3671u, 0x6E6B06E7u, 0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au,
      0x67DD4ACCu, 0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u,
      0xD6D6A3E8u, 0xA1D1937Eu, 0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u,
      0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu, 0xD80D2BDAu, 0xAF0A1B4Cu,
      0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u, 0x316E8EEFu,
      0x4669BE79u, 0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u,
      0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu, 0xC5BA3BBEu,
      0xB2BD0B28u, 0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u,
      0x2CD99E8Bu, 0x5BDEAE1Du, 0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu,
      0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu, 0x72076785u, 0x05005713u,
      0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u, 0x92D28E9Bu,
      0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u, 0x86D3D2D4u, 0xF1D4E242u,
      0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u,
      0x18B74777u, 0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu,
      0x8F659EFFu, 0xF862AE69u, 0x616BFFD3u, 0x166CCF45u, 0xA00AE278u,
      0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u, 0xA7672661u, 0xD06016F7u,
      0x4969474Du, 0x3E6E77DBu, 0xAED16A4Au, 0xD9D65ADCu, 0x40DF0B66u,
      0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
      0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u,
      0xCDD70693u, 0x54DE5729u, 0x23D967BFu, 0xB3667A2Eu, 0xC4614AB8u,
      0x5D681B02u, 0x2A6F2B94u, 0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu,
      0x2D02EF8Du};
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i)
    crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
  return ~crc;
}

static int write_all(int fd, const void *buf, size_t n) {
  const uint8_t *p = (const uint8_t *)buf;
  size_t left = n;
  while (left) {
    long w = z_syscall_write(fd, p, left);
    if (w == 0)
      return -1;
    if (w < 0) {
      if (w == -4)
        continue;
      return -1;
    }
    p += (size_t)w;
    left -= (size_t)w;
  }
  return 0;
}

static void write_cstr(int fd, const char *s) {
  if (!s)
    return;
  (void)write_all(fd, s, (size_t)simple_strlen(s));
}

static int ensure_dir_exists_p(const char *path);

static int ensure_parent_dir_exists_p(const char *path) {
  if (!path || !*path)
    return -1;
  char tmp[4096];
  size_t n = (size_t)simple_strlen(path);
  if (n >= sizeof(tmp))
    return -1;
  simple_memcpy(tmp, path, n + 1);

  size_t last_slash = (size_t)-1;
  for (size_t i = 0; i < n; ++i) {
    if (tmp[i] == '/')
      last_slash = i;
  }
  if (last_slash == (size_t)-1)
    return 0;
  if (last_slash == 0)
    return 0;
  tmp[last_slash] = '\0';
  return ensure_dir_exists_p(tmp);
}

static int read_line(char *buf, size_t cap) {
  if (!buf || cap == 0)
    return -1;
  size_t len = 0;
  while (len + 1 < cap) {
    char c;
    long r = z_syscall_read(STDIN_FILENO, &c, 1);
    if (r == 0)
      break;
    if (r < 0) {
      if (r == -4)
        continue;
      return -1;
    }
    if (c == '\n')
      break;
    if (c == '\r')
      continue;
    buf[len++] = c;
  }
  buf[len] = '\0';
  return 0;
}

static int is_dotdot_segment(const char *s, size_t n) {
  return (n == 2 && s[0] == '.' && s[1] == '.');
}

static int sanitize_basename(const char *in, char *out, size_t out_cap) {
  if (!out || out_cap == 0)
    return -1;
  if (!in || !*in) {
    out[0] = 'a';
    out[1] = 'r';
    out[2] = 'c';
    out[3] = 'h';
    out[4] = 'i';
    out[5] = 'v';
    out[6] = 'e';
    out[7] = '\0';
    return 0;
  }
  const char *p = in;
  const char *last = in;
  for (; *p; ++p)
    if (*p == '/')
      last = p + 1;
  size_t n = (size_t)(p - last);
  if (n == 0)
    return -1;
  if (n == 1 && last[0] == '.')
    return -1;
  if (n == 2 && last[0] == '.' && last[1] == '.')
    return -1;
  if (n >= out_cap)
    n = out_cap - 1;
  simple_memcpy(out, last, n);
  out[n] = '\0';
  return 0;
}

static int join2(const char *a, const char *b, char *out, size_t out_cap) {
  size_t na = (size_t)simple_strlen(a);
  size_t nb = (size_t)simple_strlen(b);
  if (na + 1 + nb + 1 > out_cap)
    return -1;
  simple_memcpy(out, a, na);
  out[na] = '/';
  simple_memcpy(out + na + 1, b, nb);
  out[na + 1 + nb] = '\0';
  return 0;
}

static int ensure_dir_exists_p(const char *path) {
  if (!path || !*path)
    return -1;
  char tmp[4096];
  size_t n = (size_t)simple_strlen(path);
  if (n >= sizeof(tmp))
    return -1;
  simple_memcpy(tmp, path, n + 1);

  size_t i = 0;
  if (tmp[0] == '/' && tmp[1] == '\0')
    return 0;

  for (i = 1; i <= n; ++i) {
    if (tmp[i] == '/' || tmp[i] == '\0') {
      char saved = tmp[i];
      tmp[i] = '\0';
      long rc = z_syscall_mkdir(tmp, 0755);
      if (rc < 0 && rc != -EEXIST) {
        tmp[i] = saved;
        return -1;
      }
      tmp[i] = saved;
    }
  }
  return 0;
}

static int ask_overwrite_policy(int *policy_out) {
  if (!policy_out)
    return -1;
  const char msg[] =
      "Path already exists. Choose: (o)verwrite all / (s)kip all / (a)bort: ";
  (void)write_all(STDOUT_FILENO, msg, sizeof(msg) - 1);
  char line[32];
  if (read_line(line, sizeof(line)) < 0)
    return -1;
  char c = line[0];
  if (c == 'o' || c == 'O') {
    *policy_out = 1;
    return 0;
  }
  if (c == 's' || c == 'S') {
    *policy_out = 2;
    return 0;
  }
  if (c == 'a' || c == 'A') {
    *policy_out = 3;
    return 0;
  }
  return -1;
}

static int parse_tar_octal_u64(const char *p, size_t n, uint64_t *out) {
  if (!p || !out)
    return -1;
  uint64_t v = 0;
  size_t i = 0;
  while (i < n && (p[i] == ' ' || p[i] == '\0'))
    ++i;
  for (; i < n; ++i) {
    char c = p[i];
    if (c == '\0' || c == ' ')
      break;
    if (c < '0' || c > '7')
      return -1;
    v = (v << 3) + (uint64_t)(c - '0');
  }
  *out = v;
  return 0;
}

static int tar_path_is_safe(const char *name) {
  if (!name || !*name)
    return 0;
  if (name[0] == '/')
    return 0;
  const char *p = name;
  while (*p) {
    const char *seg = p;
    while (*p && *p != '/')
      ++p;
    size_t sn = (size_t)(p - seg);
    if (sn == 0)
      return 0;
    if (sn == 1 && seg[0] == '.')
      return 0;
    if (is_dotdot_segment(seg, sn))
      return 0;
    if (*p == '/')
      ++p;
  }
  return 1;
}

static int extract_tar_to_dir(const uint8_t *tar, size_t tar_size,
                              const char *out_root, int *overwrite_policy) {
  size_t off = 0;
  int policy = overwrite_policy ? *overwrite_policy : 0;

  while (off + 1024 <= tar_size) {
    const uint8_t *hdr = tar + off;
    int all_zero = 1;
    for (size_t i = 0; i < 512; ++i) {
      if (hdr[i] != 0) {
        all_zero = 0;
        break;
      }
    }
    if (all_zero)
      break;

    char name[256];
    simple_memset(name, 0, sizeof(name));
    {
      size_t nn = 100;
      if (nn >= sizeof(name))
        nn = sizeof(name) - 1;
      simple_memcpy(name, hdr, nn);
      name[nn] = '\0';
    }

    if (!tar_path_is_safe(name))
      return -1;

    char full[4096];
    if ((size_t)simple_strlen(out_root) + 1 + (size_t)simple_strlen(name) + 1 >=
        sizeof(full))
      return -1;
    {
      size_t nr = (size_t)simple_strlen(out_root);
      simple_memcpy(full, out_root, nr);
      full[nr] = '/';
      size_t nn = (size_t)simple_strlen(name);
      simple_memcpy(full + nr + 1, name, nn);
      full[nr + 1 + nn] = '\0';
    }

    uint64_t fsize = 0;
    if (parse_tar_octal_u64((const char *)(hdr + 124), 12, &fsize) < 0)
      return -1;

    char typeflag = (char)hdr[156];
    if (typeflag == 0)
      typeflag = '0';

    if (typeflag == '5') {
      if (ensure_dir_exists_p(full) != 0)
        return -1;
    } else if (typeflag == '0') {
      if (ensure_parent_dir_exists_p(full) != 0)
        return -1;

      long fd = z_syscall_open(full, O_WRONLY | O_CREAT | O_EXCL, 0644);
      if (fd < 0 && fd == -EEXIST) {
        if (policy == 0) {
          if (ask_overwrite_policy(&policy) != 0)
            return -1;
          if (overwrite_policy)
            *overwrite_policy = policy;
        }
        if (policy == 2)
          goto skip_data;
        if (policy == 3)
          return -1;
        fd = z_syscall_open(full, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      }
      if (fd < 0)
        return -1;

      size_t data_off = off + 512;
      if (data_off + (size_t)fsize > tar_size) {
        (void)z_syscall_close((int)fd);
        (void)z_syscall_unlink(full);
        return -1;
      }
      if (write_all((int)fd, tar + data_off, (size_t)fsize) != 0) {
        (void)z_syscall_close((int)fd);
        (void)z_syscall_unlink(full);
        return -1;
      }
      (void)z_syscall_close((int)fd);

    skip_data:
      ;
    } else {
      return -1;
    }

    size_t data_sz = (size_t)fsize;
    size_t padded = (data_sz + 511u) & ~511u;
    off += 512 + padded;
  }
  return 0;
}

static int do_extract_single(const uint8_t *data, size_t n, const char *out_path,
                             int *overwrite_policy) {
  int policy = overwrite_policy ? *overwrite_policy : 0;

  long fd = z_syscall_open(out_path, O_WRONLY | O_CREAT | O_EXCL, 0755);
  if (fd < 0 && fd == -EEXIST) {
    if (policy == 0) {
      if (ask_overwrite_policy(&policy) != 0)
        return -1;
      if (overwrite_policy)
        *overwrite_policy = policy;
    }
    if (policy == 2)
      return 0;
    if (policy == 3)
      return -1;
    fd = z_syscall_open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
  }
  if (fd < 0)
    return -1;

  if (write_all((int)fd, data, n) != 0) {
    (void)z_syscall_close((int)fd);
    (void)z_syscall_unlink(out_path);
    return -1;
  }
  (void)z_syscall_close((int)fd);
  return 0;
}

long elfz_sfx_entry(void) {

  long fd = z_syscall_open("/proc/self/exe", O_RDONLY, 0);
  if (fd < 0)
    z_syscall_exit(1);

  struct stat st;
  if (z_syscall_fstat((int)fd, &st) < 0) {
    (void)z_syscall_close((int)fd);
    z_syscall_exit(1);
  }

  uint64_t fsz = (uint64_t)st.st_size;
  struct {
    uint64_t magic;
    uint32_t ver;
    uint32_t flags;
    uint64_t payload_off;
    uint64_t payload_size;
  } footer;

  if (fsz < sizeof(footer)) {
    (void)z_syscall_close((int)fd);
    z_syscall_exit(1);
  }

  long rr = z_syscall_pread((int)fd, &footer, sizeof(footer),
                            (off_t)(fsz - (uint64_t)sizeof(footer)));
  if (rr != (long)sizeof(footer)) {
    (void)z_syscall_close((int)fd);
    z_syscall_exit(1);
  }

  if (footer.magic != SFX_FOOTER_MAGIC0 || footer.ver != SFX_FOOTER_VER) {
    (void)z_syscall_close((int)fd);
    z_syscall_exit(1);
  }

  if (footer.payload_off + footer.payload_size > fsz || footer.payload_size < 32) {
    (void)z_syscall_close((int)fd);
    return 1;
  }

  uint64_t map_off = footer.payload_off & ~0xFFFu;
  uint64_t delta = footer.payload_off - map_off;
  uint64_t need = footer.payload_size + delta;
  size_t map_sz = (size_t)((need + 0xFFFu) & ~(uint64_t)0xFFFu);
  if (map_sz == 0)
    map_sz = 4096u;

  void *map0 = (void *)z_syscall_mmap(NULL, map_sz, PROT_READ, MAP_PRIVATE,
                                     (int)fd, (off_t)map_off);
  if ((long)map0 < 0) {
    (void)z_syscall_close((int)fd);
    return 1;
  }

  const uint8_t *base = (const uint8_t *)map0 + (size_t)delta;
  void *map = (void *)base;
  if ((long)map < 0) {
    (void)z_syscall_close((int)fd);
    return 1;
  }

  const uint8_t *p = base;
  char marker[7];
  simple_memcpy(marker, p, 6);
  marker[6] = '\0';
  p += 6;

  if (marker[0] != COMP_MARKER[0] || marker[1] != COMP_MARKER[1] ||
      marker[2] != COMP_MARKER[2] || marker[3] != COMP_MARKER[3] ||
      marker[4] != COMP_MARKER[4] || marker[5] != COMP_MARKER[5]) {
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  uint32_t zver;
  simple_memcpy(&zver, p, 4);
  p += 4;
  if (zver != ZLF_VERSION) {
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  uint32_t zflags;
  simple_memcpy(&zflags, p, 4);
  p += 4;

  uint64_t orig_size;
  simple_memcpy(&orig_size, p, 8);
  p += 8;

  uint32_t comp_size;
  simple_memcpy(&comp_size, p, 4);
  p += 4;

  uint32_t expected_crc;
  simple_memcpy(&expected_crc, p, 4);
  p += 4;

  uint16_t name_len;
  simple_memcpy(&name_len, p, 2);
  p += 2;

  if ((size_t)name_len + (size_t)(p - (const uint8_t *)map) >
      (size_t)footer.payload_size) {
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  char stored_name_raw[4096];
  stored_name_raw[0] = '\0';
  if (name_len) {
    size_t n = name_len;
    if (n >= sizeof(stored_name_raw))
      n = sizeof(stored_name_raw) - 1;
    simple_memcpy(stored_name_raw, p, n);
    stored_name_raw[n] = '\0';
    p += (size_t)name_len;
  }

  if ((size_t)(p - (const uint8_t *)map) + (size_t)comp_size >
      (size_t)footer.payload_size) {
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  const uint8_t *comp = p;

  size_t out_map_sz = (size_t)((orig_size + 0xFFFu) & ~(size_t)0xFFFu);
  if (out_map_sz == 0)
    out_map_sz = 4096u;

  void *outbuf = (void *)z_syscall_mmap(NULL, out_map_sz, PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)outbuf < 0) {
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  int dec = lz4_decompress((const char *)comp, (char *)outbuf, (int)comp_size,
                           (int)orig_size);
  if (dec < 0 || (uint64_t)dec != orig_size) {
    (void)z_syscall_munmap(outbuf, out_map_sz);
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  if (crc32_ieee(outbuf, (size_t)orig_size) != expected_crc) {
    (void)z_syscall_munmap(outbuf, out_map_sz);
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  char stored_name[256];
  if (sanitize_basename(stored_name_raw, stored_name, sizeof(stored_name)) != 0) {
    stored_name[0] = 'a';
    stored_name[1] = 'r';
    stored_name[2] = 'c';
    stored_name[3] = 'h';
    stored_name[4] = 'i';
    stored_name[5] = 'v';
    stored_name[6] = 'e';
    stored_name[7] = '\0';
  }

  const char prompt[] =
      "This is an auto extractible archive, provide a path for extraction or [enter] to extract in the current folder: ";
  (void)write_all(STDOUT_FILENO, prompt, sizeof(prompt) - 1);
  char dest_dir[4096];
  if (read_line(dest_dir, sizeof(dest_dir)) < 0) {
    (void)z_syscall_munmap(outbuf, out_map_sz);
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }
  if (dest_dir[0] == '\0') {
    dest_dir[0] = '.';
    dest_dir[1] = '\0';
  }

  char root[4096];
  if (join2(dest_dir, stored_name, root, sizeof(root)) != 0) {
    (void)z_syscall_munmap(outbuf, out_map_sz);
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  int overwrite_policy = 0;

  int is_tar = (zflags & ZLF_FLAG_TAR) ? 1 : 0;
  if (footer.flags & ZLF_FLAG_TAR)
    is_tar = 1;

  if (is_tar) {
    write_cstr(STDOUT_FILENO, "\nExtracting...\n");
    if (ensure_dir_exists_p(root) != 0) {
      (void)z_syscall_munmap(outbuf, out_map_sz);
      (void)z_syscall_munmap(map0, map_sz);
      (void)z_syscall_close((int)fd);
      return 1;
    }

    int rc = extract_tar_to_dir((const uint8_t *)outbuf, (size_t)orig_size, root,
                               &overwrite_policy);
    (void)rc;
    if (rc == 0) {
      write_cstr(STDOUT_FILENO, "Archive extracted to ");
      write_cstr(STDOUT_FILENO, root);
      write_cstr(STDOUT_FILENO, "\n");
    }
    (void)z_syscall_munmap(outbuf, out_map_sz);
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return (rc == 0 ? 0 : 1);
  }

  if (ensure_dir_exists_p(dest_dir) != 0) {
    (void)z_syscall_munmap(outbuf, out_map_sz);
    (void)z_syscall_munmap(map0, map_sz);
    (void)z_syscall_close((int)fd);
    return 1;
  }

  write_cstr(STDOUT_FILENO, "\nExtracting...\n");
  int rc = do_extract_single((const uint8_t *)outbuf, (size_t)orig_size, root,
                             &overwrite_policy);
  if (rc == 0) {
    write_cstr(STDOUT_FILENO, "Archive extracted to ");
    write_cstr(STDOUT_FILENO, root);
    write_cstr(STDOUT_FILENO, "\n");
  }
  (void)z_syscall_munmap(outbuf, out_map_sz);
  (void)z_syscall_munmap(map0, map_sz);
  (void)z_syscall_close((int)fd);
  return (rc == 0 ? 0 : 1);
}
