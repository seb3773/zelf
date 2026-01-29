#ifndef STUB_SYSCALLS_H
#define STUB_SYSCALLS_H

#include "stub_defs.h"

static inline long z_syscall_mmap(void *addr, size_t length, int prot, int flags,
                                 int fd, off_t offset) {
  register long x8 asm("x8") = SYS_mmap;
  register long x0 asm("x0") = (long)addr;
  register long x1 asm("x1") = (long)length;
  register long x2 asm("x2") = (long)prot;
  register long x3 asm("x3") = (long)flags;
  register long x4 asm("x4") = (long)fd;
  register long x5 asm("x5") = (long)offset;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_ioctl(int fd, unsigned long req, void *arg) {
  register long x8 asm("x8") = SYS_ioctl;
  register long x0 asm("x0") = (long)fd;
  register long x1 asm("x1") = (long)req;
  register long x2 asm("x2") = (long)arg;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_mprotect(void *addr, size_t len, int prot) {
  register long x8 asm("x8") = SYS_mprotect;
  register long x0 asm("x0") = (long)addr;
  register long x1 asm("x1") = (long)len;
  register long x2 asm("x2") = (long)prot;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
  return x0;
}

static inline void z_syscall_exit(int status) {
  register long x8 asm("x8") = SYS_exit;
  register long x0 asm("x0") = (long)status;

  asm volatile("svc #0" : : "r"(x0), "r"(x8) : "memory");
  __builtin_unreachable();
}

static inline long z_syscall_read(int fd, void *buf, size_t count) {
  register long x8 asm("x8") = SYS_read;
  register long x0 asm("x0") = (long)fd;
  register long x1 asm("x1") = (long)buf;
  register long x2 asm("x2") = (long)count;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_write(int fd, const void *buf, size_t count) {
  register long x8 asm("x8") = SYS_write;
  register long x0 asm("x0") = (long)fd;
  register long x1 asm("x1") = (long)buf;
  register long x2 asm("x2") = (long)count;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
  return x0;
}

// NOTE: AArch64 has no open(2) syscall; use openat(2) with AT_FDCWD.
static inline long z_syscall_open(const char *pathname, int flags, int mode) {
  register long x8 asm("x8") = SYS_openat;
  register long x0 asm("x0") = (long)AT_FDCWD;
  register long x1 asm("x1") = (long)pathname;
  register long x2 asm("x2") = (long)flags;
  register long x3 asm("x3") = (long)mode;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_close(int fd) {
  register long x8 asm("x8") = SYS_close;
  register long x0 asm("x0") = (long)fd;

  asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_pread(int fd, void *buf, size_t count,
                                  off_t offset) {
  register long x8 asm("x8") = SYS_pread64;
  register long x0 asm("x0") = (long)fd;
  register long x1 asm("x1") = (long)buf;
  register long x2 asm("x2") = (long)count;
  register long x3 asm("x3") = (long)offset;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_fstat(int fd, void *statbuf) {
  register long x8 asm("x8") = SYS_fstat;
  register long x0 asm("x0") = (long)fd;
  register long x1 asm("x1") = (long)statbuf;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_munmap(void *addr, size_t length) {
  register long x8 asm("x8") = SYS_munmap;
  register long x0 asm("x0") = (long)addr;
  register long x1 asm("x1") = (long)length;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_brk(void *addr) {
  register long x8 asm("x8") = SYS_brk;
  register long x0 asm("x0") = (long)addr;

  asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_readlink(const char *pathname, char *buf,
                                     size_t bufsiz) {
  register long x8 asm("x8") = SYS_readlinkat;
  register long x0 asm("x0") = (long)AT_FDCWD;
  register long x1 asm("x1") = (long)pathname;
  register long x2 asm("x2") = (long)buf;
  register long x3 asm("x3") = (long)bufsiz;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x8) : "memory");
  return x0;
}

static inline long z_syscall_execve(const char *filename, char *const argv[],
                                    char *const envp[]) {
  register long x8 asm("x8") = SYS_execve;
  register long x0 asm("x0") = (long)filename;
  register long x1 asm("x1") = (long)argv;
  register long x2 asm("x2") = (long)envp;

  asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
  return x0;
}

#endif // STUB_SYSCALLS_H
