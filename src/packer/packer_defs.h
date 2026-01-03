#ifndef PACKER_DEFS_H
#define PACKER_DEFS_H

#define _GNU_SOURCE
#include <elf.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Global verbose flag
extern int g_verbose;
// Global flag to disable progress display (set by --no-progress)
extern int g_no_progress;
#define VPRINTF(...)                                                           \
  do {                                                                         \
    if (g_verbose)                                                             \
      printf(__VA_ARGS__);                                                     \
  } while (0)

#define FINAL_BIN "packed_binary"
#define ALIGN 0x1000

// Reuse segment info outside of main for feature scanning
typedef struct SegmentInfo {
  Elf64_Addr vaddr;
  Elf64_Off offset;
  Elf64_Xword filesz;
  Elf64_Xword memsz; // include mem size to derive .bss
  Elf64_Word flags;
  const char *name;
} SegmentInfo;

// Executable filter selection
typedef enum {
  EXE_FILTER_AUTO = 0,
  EXE_FILTER_KANZIEXE = 1,
  EXE_FILTER_BCJ = 2,
  EXE_FILTER_NONE = 3
} exe_filter_t;

// Global filter selection
extern exe_filter_t g_exe_filter;

#endif // PACKER_DEFS_H
