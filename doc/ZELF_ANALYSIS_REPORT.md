# ZELF Technical Analysis Report

## 1. Project Overview

### 1.1 General Description

**ZELF** (stylized **zELF**) is an ELF x86_64 packer for Linux. Its modular architecture supports 16 compression algorithms, two executable filters (BCJ and KanziEXE), and an automatic filter selection system based on decision trees distilled from Machine Learning models.

**ZELF is a packing/compression tool only.** It is not meant for encryption, obfuscation, or antivirus evasion. The password option only restricts execution; it does not hide the binary contents.

### 1.2 Key Features

| Feature | Description |
|---------|-------------|
| **Multi-codec compression** | LZ4, LZMA, ZSTD, Apultra, ZX7B, ZX0, BriefLZ, Exomizer, PowerPacker, Snappy, Doboz, QuickLZ, LZAV, Shrinkler, StoneCracker, LZSA2 |
| **EXE filters** | BCJ (Branch-Call-Jump), KanziEXE (from the Kanzi compressor) |
| **Automatic selection** | Per-codec decision trees, ~93–97% accuracy on a 2000+ binary corpus |
| **ELF types supported** | Static binaries, dynamic/PIE, dynamically linked |
| **Password protection** | FNV-1a hashing with random salt |
| **Archive mode** | Arbitrary file compression (`.zlf` format) |
| **Unpacking** | Offline extraction of packed binaries and archives |

### 1.3 Global Pipeline

**Packing pipeline:**
```mermaid
graph LR
    A[Original ELF] --> B[Strip sstrip-like]
    B --> C[Select Filter AUTO/BCJ/KanziEXE]
    C --> D[Apply Filter]
    D --> E[Compress (Codec)]
    E --> F[Embed in Stub]
    F --> G[Final Packed ELF]
```

**When running the packed binary:**
```mermaid
graph LR
    H[Packed ELF] --> I[_start -> stub]
    I --> J[Find zELFxx marker]
    J --> K[Decompress in memory]
    K --> L[Unfilter BCJ/KanziEXE]
    L --> M[Map PT_LOAD segments]
    M --> N[Relocate if PIE]
    N --> O[Load ld.so if dynamic]
    O --> P[Patch AUXV]
    P --> Q[Jump to original Entry Point]
```

---

## 2. Detailed Behavior

### 2.1 Packer execution flow

#### CLI argument parsing
- Codec selection: `-lz4`, `-apultra`, `-zx7b`, etc.
- Filter choice: `--exe-filter=auto|kanziexe|bcj|none`
- Options: `--password`, `--no-strip`, `--archive`, `--unpack`, `--output`, `--nobackup`, `--verbose`.

#### Reading and analyzing the input ELF
1. **mmap() read-only** of the file.
2. **Parse ELF header** (`Elf64_Ehdr`) and program headers (`Elf64_Phdr`).
3. **Type detection**: presence of `PT_INTERP` → dynamic, otherwise static.
4. **Extract** `PT_LOAD` segments.
5. **Identify** the `.text` segment for ML analysis.

#### In-memory stripping
Similar to `sstrip`: remove non-essential sections (section table, debug) and trim trailing zeros to reduce size before compression.

#### Automatic filter selection
Uses **distilled decision trees** to choose between BCJ and KanziEXE. Extracted features include per-segment entropy, JMP/CALL opcode counts (`e8`, `e9`, `eb`), and RIP-relative estimates.

#### Data compression
`compress_data_with_codec` dispatches to the proper compressor (LZ4, ZSTD, etc.) with progress callbacks.

#### Building the packed ELF
The final ELF includes:
- **ELF header** configured for the stub.
- **Program Headers**: `PT_LOAD` for stub + compressed data.
- **Parameter block** `+zELF-PR` patched with version and virtual offsets.
- **Stub binary** chosen for the (codec × filter × static/dynamic × password) combo.
- **Marker + header** for compressed data (original size, offsets, implicit checks).

### 2.2 Stub/loader description

The stub is compiled with `-nostdlib` and uses direct syscalls (inline asm) to avoid libc dependencies.

#### Architecture
- **stub_dynamic.c**: for PIE/dynamic binaries.
- **stub_static.c**: for static binaries.
- **Entry point `_start`**: saves initial context (registers, stack pointer), prepares the stub stack, and calls the wrapper main.

#### Locating compressed data
- **Direct mode**: non-PIE/static binaries (known addresses), read from `.rodata` / `.elfz_params`.
- **Scan mode (PIE)**: the stub searches for the 6-byte codec marker in memory and validates metadata.
  - For most codecs this is a small fixed scan window near `_start` (keeps stubs smaller).
  - For **ZSTD** a more robust scan may use `/proc/self/maps` to scan the full VMA.

#### Decompression and unfiltering
- Allocate memory via `mmap`.
- Call the codec decompressor (inline, no libc).
- Apply inverse filter (BCJ decode in-place or KanziEXE decode).

#### Mapping and relocation
1. **Mapping**: map `PT_LOAD` segments to their target virtual addresses.
2. **Relocation (PIE)**:
   - Apply `DT_RELA` relocations (R_X86_64_RELATIVE).
   - Patch GOT via `DT_PLTGOT`.
   - RW heuristic (last resort) for atypical segments.

#### Loading ld.so (dynamic only)
- Open and map the interpreter (`/lib64/ld-linux-x86-64.so.2`).
- Create a “hatch” (syscall trampoline) required by ld.so to restore registers.
- Patch the Auxiliary Vector (AUXV) on the stack (`AT_PHDR`, `AT_ENTRY`, `AT_BASE`) to mimic a direct launch of the original binary.

---

## 3. Notable Aspects

### 3.1 ML prediction
Decision trees trained on a large corpus (~2000 binaries) to predict BCJ vs KanziEXE efficiency. Models are exported as C headers (`*_predict_dt.h`) for ultra-fast packing-time inference without Python.

### 3.2 Modular stub architecture
All stub combinations (Codecs × Filters × Types × Options) are generated at build time (`gen_stubs_mk.py`), yielding ~128 variants optimized with `-Os`, `-flto`, `-nostdlib`.

### 3.3 Password protection
Uses salted/obfuscated FNV-1a 64-bit hashing. At runtime, the stub reads the password from `/dev/tty` (echo off) to unlock execution.

---

## 4. Code Structure

### 4.1 Directory layout
```
ELFZ/
├── src/
│   ├── packer/          # Main code (Host)
│   │   ├── zelf_packer.c, compression.c, elf_builder.c...
│   ├── stub/            # Loader code (Target, nostdlib)
│   │   ├── stub_static.c, stub_dynamic.c, syscalls...
│   ├── compressors/     # Compressor sources
│   ├── filters/         # BCJ, KanziEXE
│   └── tools/           # Analysis utilities
├── tools/               # Build/ML scripts
├── doc/                 # Documentation
└── build/               # Artifacts
```

### 4.2 Main modules
- **Packer**: orchestration, CLI parsing, compression management.
- **Stub**: minimal runtime, memory handling, syscalls, ELF loading.
- **ML**: training scripts and prediction headers.

---

## 5. Technical Aspects and Error Codes

### 5.1 Error handling (Stub)
The stub exits with specific codes on failure:
- `1`: Generic error.
- `3`: `mmap` failure.
- `4`: Marker not found (corrupt or missing data).
- `5`: Decompression failure.
- `6`: Invalid ELF after decompression.
- `11/12`: Parameter/password block error.
- `65`: Parameters not patched.
- `66/67`: Marker/Magic mismatch.
- `90+`: Sanity-check (bounds) errors.

### 5.2 Performance
- **Packer**: uses `mmap` for I/O, streaming for ZSTD, LTO for linking.
- **Stub**: size-optimized (`-Os`), no complex dynamic allocation, direct syscalls.

