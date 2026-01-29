#include <unistd.h>
#include <string.h>
#include <stdint.h>

enum { PROBE_RODATA_SZ = (2u << 20) };
enum { PROBE_BSS_SZ = (8u << 20) };

__attribute__((used))
static const unsigned char probe_rodata[PROBE_RODATA_SZ] = {0xA5};

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

static void write_str2(const char *a, const char *b) {
  if (a)
    (void)write(1, a, (int)strlen(a));
  if (b)
    (void)write(1, b, (int)strlen(b));
}

int main(int argc, char **argv) {
  if (argc >= 2) {
    const char *arg = argv[1];
    if (streq(arg, "--help") || streq(arg, "-h")) {
      write_str("dynexec_probe - small dynamic ELF test binary\n\n");
      write_str("Usage:\n");
      write_str("  dynexec_probe --version\n");
      write_str("  dynexec_probe --selftest\n");
      return 0;
    }
    if (streq(arg, "--version") || streq(arg, "-V")) {
      write_str("dynexec_probe 1\n");
      return 0;
    }
    if (streq(arg, "--selftest")) {
      uint64_t x = 0x9e3779b97f4a7c15ull;
      for (uint64_t i = 0; i < 2000000ull; i++) {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        x *= 0x2545F4914F6CDD1Dull;
      }

      for (uint64_t i = 0; i < (uint64_t)PROBE_RODATA_SZ; i += 4096ull) {
        x += (uint64_t)probe_rodata[i];
      }
      for (uint64_t i = 0; i < (uint64_t)PROBE_BSS_SZ; i += 4096ull) {
        probe_bss[i] = (unsigned char)(x >> (i & 7u));
        x ^= (uint64_t)probe_bss[i];
      }
      if (x == 0)
        write_str2("FAIL ", "0\n");
      else
        write_str("OK\n");
      return (x == 0) ? 1 : 0;
    }
    write_str2("Unknown argument: ", arg);
    write_str("\n");
    return 2;
  }

  write_str("OK\n");
  return 0;
}
