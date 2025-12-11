# zELF — x86_64 ELF packer

zELF is an ELF64 packer/modifier for Linux x86_64, inspired by UPX but with a modular structure: 16 compression codecs, the ability to integrate your own codecs, two executable filters (BCJ, KanziEXE) and automatic selection via ML models. It handles static and dynamic/PIE binaries, provides a `.zlf` archive mode, and a `-best` mode that explores multiple combinations to maximize ratio.

## Key features
- Multi-codec compression: LZ4, LZMA, ZSTD, Apultra, ZX7B, ZX0, BriefLZ, Exomizer, PowerPacker, Snappy, Doboz, QuickLZ, LZAV, Shrinkler, StoneCracker, LZSA2.
- EXE filters: BCJ and KanziEXE, automatic selection via trained decision trees (default mode `auto`).
- ELF support: static and dynamic/PIE binaries, strict header and segment validation.
- `-best` mode: picks 3 predicted codecs based on binary size, tests filters `bcj → kanzi → none`, keeps the smallest.
- Archive mode `.zlf` and unpack (`--archive`, `--unpack`).
- Password option (FNV-1a hash obfuscated in the stub).
- Compact nostdlib stubs (1.5–22 KB), repacked into the final binary.
- Analysis tools (elfz_probe, filtered_probe) in `build/tools/`.

## Platform and limits
- Targets Linux x86_64, ELF64 only. ELF 32-bit is not supported.
- Please note that zELF is not designed for code obfuscation or concealment. All source code is openly available, and even the more exotic compression format can be integrated and parsed by any analysis tool thanks to the provided sources. The goal of this packer is binary size reduction and efficient runtime unpacking—not evasion of antivirus software or distribution of malicious code. It is intended for legitimate use cases such as embedded systems, demos, general binary optimization or educational purposes like studying ELF structure and advanced packing techniques.

## Build
You can use the interactive menu to configure and launch the build:
```bash
make menu
```
Or use the manual commands below:

### Install dependencies
On Debian/Ubuntu:
```bash
make install_dependencies
```
For other distributions, install equivalents of: gcc/g++/make/nasm/pkg-config, zlib-dev, lz4-dev, zstd-dev, python3/pip, dpkg-deb, dialog/whiptail. For static builds, static libs (libstdc++/glibc) must be available on the system.

### Build commands
```bash
# Full build (stubs, tools, packer)
make
# Static build
make STATIC=1
# Parallelize stub generation (default 2)
make STUBS_J=4 stubs
# Tools only
make tools
# Packages
make deb        # produces a .deb, suffix _static if STATIC=1
make tar        # produces a .tar.gz, suffix _static if STATIC=1
```
The final packer binary is `build/zelf`.

## Quick usage
```bash
./build/zelf [options] <elf_binary>
```
Main user options:
- Codec selection: `-lz4` (default), `-lzma`, `-zstd`, `-apultra`, `-zx7b`, `-zx0`, `-blz`, `-exo`, `-pp`, `-snappy`, `-doboz`, `-qlz`, `-lzav`, `-shr` (shrinkler), `-stcr` (stonecracker), `-lzsa`.
- EXE filter: `--exe-filter=auto|bcj|kanziexe|none` (ignored in `-best`, order forced bcj→kanzi→none).
- `-best` mode: tries 3 predicted codecs + filters, keeps the best.
- Strip: `--no-strip` disables super-strip before compression.
- Password: `--password` (prompts for password, random salt).
- Output: `--output <file|dir>`, `--nobackup` to avoid `.bak` when overwriting.
- Archives: `--archive` (creates `.zlf`), `--unpack` (unpacks packed ELF or `.zlf` archive).
- Display: `--verbose`, `--no-progress`, `--no-colors`.

### Examples
```bash
# Simple pack (lz4 default), overwrites original after .bak backup beside it
./build/zelf /usr/bin/ls

# Pack with ZSTD + explicit BCJ filter
./build/zelf -zstd --exe-filter=bcj /usr/bin/ls --output /tmp/ls_packed

# -best mode (multi codec/filter tries), explicit output
./build/zelf -best /usr/bin/true --output /tmp/true_best

# Archive any file
./build/zelf -zstd --archive some_data --output archive.zlf

# Unpack a .zlf archive
./build/zelf --unpack archive.zlf --output restored_data

# Restore a packed binary (in-place or to a new file)
./build/zelf --unpack /tmp/ls_packed --output /tmp/ls_original
```

## `-best` mode (details)
- Selects 3 codecs based on size (built-in table).
- Tests filters `bcj`, `kanzi`, `none` for each codec (9 attempts total).
- Keeps only the smallest binary.
- `--exe-filter` is ignored in this mode.

## Developer docs
For exhaustive architecture and development details:
- **[README_dev.md](README_dev.md)**: Comprehensive developer guide (Architecture, Codecs, ML, Stubs).

Other reference documents:
- `doc/ZELF_ANALYSIS_REPORT.md`: Technical analysis report (Architecture & Analysis).
- `doc/STUB_LOADER_INTERNALS.md`: Stub/loader low-level details.
- `doc/Predictor_Integration_and_Guide.md`: ML pipeline and predictors.
- `doc/ANALYSE_SYSTEME_STATS_ML.md`: Statistical analyses.

## License
Main license: TFYW (The Fuck You Want), subject to licenses of bundled components (see codec/filter sources).
