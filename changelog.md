# Changelog

## Unreleased

### Build / Toolchain
- Added openSUSE support to the dependency installer (`tools/install_dependencies.sh`) via `zypper`.
- Improved build chain compatibility on openSUSE toolchains by enabling PIE for stubs when required (`-fpie`).
  - Note: this may produce slightly larger stubs (typically a few hundred bytes). A build on Debian remains preferable when you want the smallest possible stubs.

### Two-stage stubs (stage0 wrappers)
- Fixed unpack/decompression handling for stage0-wrapped stubs (ZSTD and Density dynamic).
- Added an explicit 1-byte stage0 header flags field used to propagate the selected EXE filter (BCJ/KanziEXE/none) without relying on heuristics.

### Runtime compatibility (dynamic stub)
- Fixed dynamic loader handoff on recent glibc/ld-linux by updating AUXV entries to match the decompressed in-memory ELF mapping (notably `AT_PHDR` and `AT_ENTRY`).

### Runtime behavior
- Removed the ZSTD-only `memfd_create+execveat` fast-path from static stubs (execution now always continues from the on-disk packed binary).

### Compression / Memory
- Added a preflight RAM / `RLIMIT_AS` check for LZHAM and abort early with an explicit error message when memory is insufficient (keeps existing LZHAM compression settings unchanged).

### Robustness
- Removed strict-aliasing/unaligned-access-unsafe loads/stores in stage0 wrapper patching and unpack/parsing paths (replaced with `memcpy` based accesses where needed).
