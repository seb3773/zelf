#include <unistd.h>
#include <string.h>
#include <stdint.h>

enum { PROBE_RODATA_SZ = (1u << 20) };
enum { PROBE_BSS_SZ = (4u << 20) };

__attribute__((used))
static const unsigned char probe_rodata[PROBE_RODATA_SZ] = {0x5A};

__attribute__((used))
static unsigned char probe_bss[PROBE_BSS_SZ];

static int streq(const char *a, const char *b) {
  return (a && b && strcmp(a, b) == 0);
}

static void write_str(const char *s) {
  if (!s)
    return;
  (void)write(1, s, (int)strlen(s));
}

int main(int argc, char **argv) {
  if (argc >= 2) {
    const char *arg = argv[1];
    if (streq(arg, "--version") || streq(arg, "-V")) {
      write_str("simple_static 1\n");
      return 0;
    }
    if (streq(arg, "--selftest")) {
      uint64_t x = 0x243f6a8885a308d3ull;
      for (uint64_t i = 0; i < 1000000ull; i++) {
        x ^= x >> 13;
        x *= 0xff51afd7ed558ccdull;
        x ^= x >> 33;
      }
      for (uint64_t i = 0; i < (uint64_t)PROBE_RODATA_SZ; i += 4096ull)
        x += (uint64_t)probe_rodata[i];
      for (uint64_t i = 0; i < (uint64_t)PROBE_BSS_SZ; i += 4096ull) {
        probe_bss[i] = (unsigned char)(x >> (i & 7u));
        x ^= (uint64_t)probe_bss[i];
      }
      write_str((x == 0) ? "FAIL\n" : "OK\n");
      return (x == 0) ? 1 : 0;
    }
    write_str("OK\n");
    return 0;
  }

  write_str("OK\n");
  return 0;
}
