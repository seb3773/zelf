#ifndef STUB_DEFS_H
#define STUB_DEFS_H

#include <stddef.h>
#include <stdint.h>

// off_t type for nostdlib
typedef long off_t;

// Memory protections (mirrors sys/mman.h)
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define PROT_NONE 0x0

// mmap flags
#define MAP_PRIVATE 2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 32
#define MAP_STACK 0x20000

// nostdlib syscall numbers
#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_fstat 5
#define SYS_mmap 9
#define SYS_mprotect 10
#define SYS_munmap 11
#define SYS_exit 60
#define SYS_ioctl 16
#define SYS_memfd_create 319
#define SYS_execveat 322

// Syscall constants
#define O_RDONLY 0
#define AT_EMPTY_PATH 0x1000
#define MFD_CLOEXEC 0x0001

// Minimal TTY ioctls to toggle echo
#define TCGETS 0x5401
#define TCSETS 0x5402
#define Z_ECHO 0x00000008

// Structures ELF
typedef struct {
  unsigned char e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
} Elf64_Phdr;

typedef struct {
  int64_t d_tag;
  union {
    uint64_t d_val;
    uint64_t d_ptr;
  } d_un;
} Elf64_Dyn;

typedef struct {
  uint64_t r_offset;
  uint64_t r_info;
  int64_t r_addend;
} Elf64_Rela;

typedef struct {
  uint64_t a_type;
  union {
    uint64_t a_val;
  } a_un;
} Elf64_auxv_t;

// stat structure for nostdlib
struct stat {
  uint64_t st_dev;
  uint64_t st_ino;
  uint64_t st_nlink;
  uint32_t st_mode;
  uint32_t st_uid;
  uint32_t st_gid;
  uint32_t __pad0;
  uint64_t st_rdev;
  int64_t st_size;
  int64_t st_blksize;
  int64_t st_blocks;
  uint64_t st_atime;
  uint64_t st_mtime;
  uint64_t st_ctime;
};

// Defines ELF
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_PHDR 6

#define PF_R 4
#define PF_W 2
#define PF_X 1

#define AT_NULL 0
#define AT_IGNORE 1
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_BASE 7
#define AT_ENTRY 9
#define AT_PLATFORM 15
#define AT_RANDOM 25
#define AT_EXECFN 31

#define DT_NULL 0
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_INIT 12
#define DT_FINI 13
#define DT_JMPREL 0x17
#define DT_PLTRELSZ 0x2
#define DT_PLTGOT 3
#define DT_RELR 0x24
#define DT_RELRSZ 0x23
#define DT_RELRENT 0x25

#define R_X86_64_RELATIVE 8
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_IRELATIVE 37

#endif // STUB_DEFS_H
