#ifndef STUB_SYSCALLS_H
#define STUB_SYSCALLS_H

#include "stub_defs.h"

// Syscalls inline
static inline long z_syscall_mmap(void *addr, size_t length, int prot,
                                  int flags, int fd, off_t offset) {
  register long rax asm("rax") = SYS_mmap;
  register long rdi asm("rdi") = (long)addr;
  register long rsi asm("rsi") = (long)length;
  register long rdx asm("rdx") = (long)prot;
  register long r10 asm("r10") = (long)flags;
  register long r8 asm("r8") = (long)fd;
  register long r9 asm("r9") = (long)offset;

  asm volatile("syscall"
               : "=a"(rax)
               : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8),
                 "r"(r9)
               : "memory", "rcx", "r11");
  return rax;
}

static inline long z_syscall_ioctl(int fd, unsigned long req, void *arg) {
  register long rax asm("rax") = SYS_ioctl;
  register long rdi asm("rdi") = (long)fd;
  register long rsi asm("rsi") = (long)req;
  register long rdx asm("rdx") = (long)arg;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_mprotect(void *addr, size_t len, int prot) {
  register long rax asm("rax") = SYS_mprotect;
  register long rdi asm("rdi") = (long)addr;
  register long rsi asm("rsi") = (long)len;
  register long rdx asm("rdx") = (long)prot;

  asm volatile("syscall"
               : "=a"(rax)
               : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx)
               : "memory", "rcx", "r11");
  return rax;
}

static inline void z_syscall_exit(int status) {
  register long rax asm("rax") = SYS_exit;
  register long rdi asm("rdi") = (long)status;

  asm volatile("syscall" : : "a"(rax), "D"(rdi) : "memory", "rcx", "r11");
  __builtin_unreachable();
}

static inline long z_syscall_read(int fd, void *buf, size_t count) {
  register long rax asm("rax") = SYS_read;
  register long rdi asm("rdi") = (long)fd;
  register long rsi asm("rsi") = (long)buf;
  register long rdx asm("rdx") = (long)count;

  asm volatile("syscall"
               : "=a"(rax)
               : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx)
               : "memory", "rcx", "r11");
  return rax;
}

static inline long z_syscall_write(int fd, const void *buf, size_t count) {
  register long rax asm("rax") = SYS_write;
  register long rdi asm("rdi") = (long)fd;
  register long rsi asm("rsi") = (long)buf;
  register long rdx asm("rdx") = (long)count;

  asm volatile("syscall"
               : "=a"(rax)
               : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx)
               : "memory", "rcx", "r11");
  return rax;
}

static inline long z_syscall_open(const char *pathname, int flags, int mode) {
  register long rax asm("rax") = SYS_open;
  register long rdi asm("rdi") = (long)pathname;
  register long rsi asm("rsi") = flags;
  register long rdx asm("rdx") = mode;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_close(int fd) {
  register long rax asm("rax") = SYS_close;
  register long rdi asm("rdi") = fd;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(rax), "D"(rdi)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_pread(int fd, void *buf, size_t count,
                                   off_t offset) {
  register long rax asm("rax") = 17; // __NR_pread64
  register long rdi asm("rdi") = fd;
  register long rsi asm("rsi") = (long)buf;
  register long rdx asm("rdx") = count;
  register long r10 asm("r10") = offset;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "r"(rax), "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_execve(const char *filename, char *const argv[],
                                    char *const envp[]) {
  register long rax asm("rax") = 59; // SYS_execve
  register long rdi asm("rdi") = (long)filename;
  register long rsi asm("rsi") = (long)argv;
  register long rdx asm("rdx") = (long)envp;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "r"(rax), "r"(rdi), "r"(rsi), "r"(rdx)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_fstat(int fd, void *statbuf) {
  register long rax asm("rax") = SYS_fstat;
  register long rdi asm("rdi") = fd;
  register long rsi asm("rsi") = (long)statbuf;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(rax), "D"(rdi), "S"(rsi)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_munmap(void *addr, size_t length) {
  register long rax asm("rax") = SYS_munmap;
  register long rdi asm("rdi") = (long)addr;
  register long rsi asm("rsi") = length;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "r"(rax), "r"(rdi), "r"(rsi)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_brk(void *addr) {
  register long rax asm("rax") = 12; // SYS_brk
  register long rdi asm("rdi") = (long)addr;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(rax), "D"(rdi)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_readlink(const char *pathname, char *buf,
                                      size_t bufsiz) {
  register long rax asm("rax") = 89; // SYS_readlink
  register long rdi asm("rdi") = (long)pathname;
  register long rsi asm("rsi") = (long)buf;
  register long rdx asm("rdx") = bufsiz;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "a"(rax), "D"(rdi), "S"(rsi), "d"(rdx)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_memfd_create(const char *name,
                                          unsigned int flags) {
  register long rax asm("rax") = SYS_memfd_create;
  register long rdi asm("rdi") = (long)name;
  register long rsi asm("rsi") = (long)flags;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "r"(rax), "r"(rdi), "r"(rsi)
               : "rcx", "r11", "memory");
  return result;
}

static inline long z_syscall_execveat(int dirfd, const char *pathname,
                                      char *const argv[], char *const envp[],
                                      int flags) {
  register long rax asm("rax") = SYS_execveat;
  register long rdi asm("rdi") = (long)dirfd;
  register long rsi asm("rsi") = (long)pathname;
  register long rdx asm("rdx") = (long)argv;
  register long r10 asm("r10") = (long)envp;
  register long r8 asm("r8") = (long)flags;
  register long result;
  asm volatile("syscall"
               : "=a"(result)
               : "r"(rax), "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8)
               : "rcx", "r11", "memory");
  return result;
}

#endif // STUB_SYSCALLS_H
