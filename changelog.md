# Changelog

## 0.91

- Added full ELF64 AArch64 (arm64) support (both packer host build and packing/unpacking arm64 targets).
- Multi-arch build model: host-specific packer can pack foreign targets when the corresponding prebuilt assets are present:
  - `build/stubs_x86_64/`, `build/stubs_arm64/`
  - `build/stage0_x86_64/`, `build/stage0_arm64/`
- `make clean` is multi-arch safe (does not delete foreign arch assets that cannot be regenerated on the current host).
- Added/expanded archive self-extracting modes (`--archive-sfx`, `--archive-tar-sfx`) and improved archive handling.
- Corrected and documented the supported codec count to 22, with the full list.
- CLI improvements:
  - Print detected input ELF machine type (x86_64 / AArch64) after the "Packing ELF" line.
  - Improved `--version` output with host architecture prefix and supported packing target detection.
- Updated documentation to reflect AArch64 + multi-arch + SFX support and the codec list.
  - Improved information displayed during build.


## 0.82

- Added openSUSE support to the dependency installer (`tools/install_dependencies.sh`) via `zypper`.
- Improved build chain compatibility on openSUSE toolchains by enabling PIE for stubs when required (`-fpie`).
   (Note: this may produce slightly larger stubs, typically a few hundred bytes. A build on Debian remains preferable when you want the smallest possible stubs)
- Fixed unpack/decompression handling for stage0-wrapped stubs (ZSTD and Density dynamic).
- Added an explicit 1-byte stage0 header flags field used to propagate the selected EXE filter (BCJ/KanziEXE/none) without relying on heuristics.
- Fixed dynamic loader handoff on recent glibc/ld-linux by updating AUXV entries to match the decompressed in-memory ELF mapping (notably `AT_PHDR` and `AT_ENTRY`).
- Removed the ZSTD-only `memfd_create+execveat` fast-path from static stubs (execution now always continues from the on-disk packed binary).
- Added self-extracting (SFX) archive modes (`--archive-sfx` and `--archive-tar-sfx`).
- Added a preflight RAM / `RLIMIT_AS` check for LZHAM and abort early with an explicit error message when memory is insufficient (keeps existing LZHAM compression settings unchanged).
- Removed strict-aliasing/unaligned-access-unsafe loads/stores in stage0 wrapper patching and unpack/parsing paths (replaced with `memcpy` based accesses where needed).
