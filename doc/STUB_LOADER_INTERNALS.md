# ZELF Stub/Loader Technical Notes

## 1. nostdlib Constraints

### 1.1 Runtime environment

The stub runs **without libc**:
- No CRT init (`_start` called directly by the kernel)
- No `malloc()`, `printf()`, `memcpy()` from libc
- No TLS (Thread-Local Storage)
- Stack as provided by the kernel (no pre-init)

### 1.2 Implications

| Constraint | ZELF approach |
|------------|---------------|
| No libc | Direct syscalls via inline asm |
| No allocator | `mmap(MAP_ANONYMOUS)` only |
| No printf | Debug writes via `write(1, ...)` |
| No memcpy | Inline implementation in `stub_utils.h` |
| No mutable globals | Everything on the stack or in registers |

### 1.3 Build flags

```bash
# Critical flags
-nostdlib
-fno-stack-protector
-fomit-frame-pointer
-fno-exceptions
-fno-asynchronous-unwind-tables
-fvisibility=hidden
```

---

## 2. Syscalls used

### 2.1 List

| Syscall | # | Purpose | File |
|---------|---|---------|------|
| `read` | 0 | Read password from TTY | stub_syscalls.h |
| `write` | 1 | Writes (memfd, prompts) | stub_syscalls.h |
| `open` | 2 | Open `/dev/tty`, ld.so | stub_syscalls.h |
| `close` | 3 | Close fd | stub_syscalls.h |
| `fstat` | 5 | ld.so file size | stub_syscalls.h |
| `mmap` | 9 | Allocate memory, map segments | stub_syscalls.h |
| `mprotect` | 10 | Change segment permissions | stub_syscalls.h |
| `munmap` | 11 | Free temporary memory | stub_syscalls.h |
| `ioctl` | 16 | Disable TTY echo (password) | stub_syscalls.h |
| `exit` | 60 | Exit on error | stub_syscalls.h |
| `memfd_create` | 319 | Fast-path ET_EXEC (ZSTD) | stub_syscalls.h |
| `execveat` | 322 | Fast-path ET_EXEC (ZSTD) | stub_syscalls.h |

### 2.2 Call pattern

```c
// stub_syscalls.h - inline asm wrapper
static inline long z_syscall_mmap(void *addr, size_t len, int prot,
                                  int flags, int fd, off_t off) {
    long ret;
    register long r10 asm("r10") = flags;
    register long r8 asm("r8") = fd;
    register long r9 asm("r9") = off;
    asm volatile("syscall"
        : "=a"(ret)
        : "a"(SYS_mmap), "D"(addr), "S"(len), "d"(prot),
          "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory");
    return ret;
}
```

---

## 3. Packed data lookup strategy

### 3.1 Two modes

| Mode | Condition | Method |
|------|-----------|--------|
| **Direct** | `virtual_start >= 0x10000` | Patched address in `.rodata.elfz_params` |
| **Scan** | `virtual_start < 0x10000` (PIE) | Marker search in memory (see below) |

### 3.2 Direct mode (non-PIE)

```c
// Parameters read from the packer-patched block
uint64_t params_packed_data_vaddr = *(pb + 24);
uint64_t aslr_offset = current_start_addr - virtual_start;
const char *packed_data = (params_packed_data_vaddr + aslr_offset);
```

### 3.3 VMA scan mode (PIE)

```c
// Marker scan (PIE):
// - Non-ZSTD codecs: fixed scan window near _start (keeps stubs smaller).
// - ZSTD codec: optional robust scan using /proc/self/maps to scan the full VMA.
for (unsigned char *p = search_start; p < search_end - 32; p++) {
    if (p[0]==COMP_MARKER[0] && p[1]==COMP_MARKER[1] && ...) {
        // Validate metadata
        uint64_t actual_size = *(uint64_t *)(p + 6);
        uint32_t comp_size = *(uint32_t *)(p + 22);
        
        // Validation heuristics
        if (actual_size > 1000 && actual_size < 0x20000000 &&
            comp_size > 100 && comp_size < actual_size) {
            // Codec-specific validation...
            packed_data = (const char *)p;
            break;
        }
    }
}
```

### 3.4 Codec-specific validation

| Codec | Validation |
|-------|------------|
| ZSTD | Magic `0xFD2FB528`, coherent content size |
| LZMA | Valid props byte (lc≤8, lp≤4, pb≤4), dict 4KB–256MB |
| Shrinkler | `Shri` header, coherent sizes |
| Snappy | Varint uncompressed length = actual_size |
| LZAV | Nibble format = 2 |

### 3.5 Rationale (size vs robustness)

- **Why not always `/proc/self/maps`?**: parsing `/proc/self/maps` adds ~1KB+ of code/data to every dynamic stub variant. zELF keeps this **ZSTD-only** (where the dynamic layout is more sensitive and the robustness is worth the cost) and uses a small fixed scan window for other codecs.

---

## 4. Decompression and unfiltering

### 4.1 Unified interface

All decompressors are wrapped under the same prototype via `codec_select.h`:

```c
// The name "lz4_decompress" is used for all codecs via conditional aliasing
static inline int lz4_decompress(const char *src, char *dst,
                                 int src_len, int dst_cap);
```

### 4.2 Decompression sequence

```c
// 1. Allocate destination buffer
size_t alloc_size = original_size + SAFETY_MARGIN;
void *combined_data = mmap(NULL, alloc_size, PROT_READ|PROT_WRITE,
                           MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

// 2. Decompress
int decomp_result = lz4_decompress(compressed_data, combined_data,
                                   compressed_size, alloc_size);

// 3. Validate
if (decomp_result < 0) z_syscall_exit(5);
if (decomp_result != original_size) z_syscall_exit(5);
```

### 4.3 Unfiltering

After decompression, apply the inverse executable filter:

```c
// BCJ (in-place)
bcj_x86_decode((uint8_t*)combined_data, original_size, 0);

// KanziEXE (requires separate buffer)
void *unfiltered = mmap(...);
kanzi_exe_unfilter_x86(combined_data, decomp_result,
                       unfiltered, original_size, &processed);
```

### 4.4 Filter Header Layouts

The packed data block format depends on the filter type and codec. This affects how stubs parse the header.

#### Standard layout (KanziEXE or NONE filter)

| Field | Offset | Size |
|-------|--------|------|
| COMP_MARKER | 0 | 6 bytes |
| original_size | 6 | 8 bytes |
| entry_offset | 14 | 8 bytes |
| compressed_size | 22 | 4 bytes |
| **filtered_size** | 26 | 4 bytes |
| compressed_stream | 30 | N bytes |

#### Legacy layout (BCJ filter with legacy codecs, or ZSTD/Density)

| Field | Offset | Size |
|-------|--------|------|
| COMP_MARKER | 0 | 6 bytes |
| original_size | 6 | 8 bytes |
| entry_offset | 14 | 8 bytes |
| compressed_size | 22 | 4 bytes |
| compressed_stream | 26 | N bytes |

> **Key difference**: Legacy layout omits `filtered_size` (4 bytes) because with BCJ filter, `filtered_size == original_size`.

#### `legacy_bcj_layout` flag in `elf_builder.c`

The packer uses `legacy_bcj_layout` to decide whether to write the `filtered_size` field:

```c
int legacy_bcj_layout =
    (codec == "zstd" || codec == "density")   // Always legacy for these
        ? 1
        : ((g_exe_filter == EXE_FILTER_BCJ || g_exe_filter == EXE_FILTER_NONE) &&
           is_legacy_codec(codec));           // Legacy for BCJ/NONE with legacy codecs
```

**Rules**:
- ZSTD/Density: always use legacy layout (no `filtered_size`)
- Legacy codecs (LZ4, LZMA, LZHAM, etc.) with BCJ or NONE filter: legacy layout
- Legacy codecs with KanziEXE filter: standard layout (writes `filtered_size`)
- Modern stubs (`stub_dynamic.c`): always read `filtered_size` for non-ZSTD/Density codecs

### 4.5 Decompression destination capacity (`dst_cap`)

Different codecs require different `dst_cap` values based on the filter type:

| Codec | BCJ filter | KanziEXE/NONE filter |
|-------|------------|----------------------|
| LZ4, Apultra, etc. | `alloc_size` | `alloc_size` |
| LZMA, BriefLZ | `filtered_size` | `filtered_size` |
| LZHAM | `original_size` | `filtered_size` |
| PowerPacker | `original_size` | `filtered_size` |

**Rationale**: Some decompressors (LZMA, BriefLZ, LZHAM) need the exact output size for proper termination. With BCJ, `filtered_size == original_size`, but with KanziEXE, `filtered_size` may differ slightly from `original_size`.

**Implementation in `stub_dynamic.c`**:
```c
int dst_cap = (int)alloc_size;

#if defined(CODEC_LZMA) || defined(CODEC_BRIEFLZ)
    dst_cap = (int)filtered_size;
#endif

#if defined(CODEC_LZHAM)
#if defined(ELFZ_FILTER_BCJ)
    dst_cap = (int)original_size;
#else
    dst_cap = (int)filtered_size;
#endif
#endif
```

### 4.6 Stub variants and filter defines

Stubs are compiled with filter-specific defines:

| Stub file | Define | Used for |
|-----------|--------|----------|
| `stub_dynamic.c` | (none) | KanziEXE, NONE filters |
| `stub_dynamic_bcj.c` | `ELFZ_FILTER_BCJ` | BCJ filter |
| `stub_static.c` | (none) | Static KanziEXE, NONE |
| `stub_static_bcj.c` | `ELFZ_FILTER_BCJ` | Static BCJ |

> **Note**: `stub_dynamic_bcj.c` simply includes `stub_core_dynamic.inc`, which uses the legacy header layout (no `filtered_size`).

### 4.7 ZSTD Dynamic Stub Architecture (Critical)

ZSTD dynamic stubs have a unique architecture that differs from other codecs:

#### Stub file hierarchy

```
stub_dynamic.c          → Used for KanziEXE stubs (most codecs)
stub_dynamic_bcj.c      → Used for BCJ stubs (wrapper, includes stub_core_dynamic.inc)
  └── stub_core_dynamic.inc → Core BCJ logic (legacy header parsing)
```

> **IMPORTANT**: When debugging BCJ filter issues for ZSTD, edit `stub_core_dynamic.inc`, NOT `stub_dynamic.c`. The BCJ stub uses a completely separate code path!

#### Stage0 wrapper for ZSTD

ZSTD stubs are wrapped with a stage0 NRV2B bootloader:
1. Kernel loads the packed binary
2. Stage0 runs and decompresses the real stub to `dst_off` (higher memory)
3. Stage0 jumps to the decompressed stub
4. Stub scans for packed data in memory

Due to this, **the packed data is at LOWER addresses than where the stub runs**. The ZSTD stub includes a backward VMA scan to find the marker.

#### Filter flag handling

Filter selection is communicated via `elfz_flags` in `elfz_params_block`:

| Bit | Flag | Meaning |
|-----|------|---------|
| 0 | BCJ | BCJ filter was applied |
| 1 | Kanzi | KanziEXE filter was applied |

For `filter=none`, both bits are 0.

**Critical**: The stub must read the flag from `elfz_params_block` (patched by packer), NOT from legacy packed data fields. The ZSTD BCJ path in `stub_core_dynamic.inc` was updated to use `elfz_params_block` for consistency.

```c
// Correct: Read from elfz_params_block
uint64_t params_version = *(const uint64_t *)(pb + 8);
int bcj_flag = ((params_version >> 8) & 1ULL) ? 1 : 0;  // bit0

// Incorrect (legacy): Reading from packed data
int bcj_flag = *(int *)compressed_data;  // May not match packer intent
```

### 4.8 Safety margins per codec

| Codec | Allocation margin | Rationale |
|-------|-------------------|-----------|
| LZ4/Snappy | +64 KB | Decoder edge cases |
| ZSTD | +64 MB + 128 KB | Possible block overrun |
| Shrinkler | +32 MB | Long transient matches |
| Others | +4 KB | Standard padding |

---

## 5. Memory mapping

### 5.1 Strategy

```c
// 1. Compute total size and max alignment
for (int i = 0; i < phnum; i++) {
    if (phdr[i].p_type == PT_LOAD) {
        uint64_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
        if (seg_end > total_size) total_size = seg_end;
        if (phdr[i].p_align > max_align) max_align = phdr[i].p_align;
    }
}

// 2. Reserve space with PROT_NONE
void *reserve = mmap(NULL, total_size + max_align, PROT_NONE,
                     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

// 3. Align base
uint64_t aligned_addr = (reserve + max_align - 1) & ~(max_align - 1);
coherent_base = (void *)aligned_addr;

// 4. Unmap unused head/tail
munmap(reserve, aligned_addr - reserve);
munmap(aligned_addr + total_size, ...);
```

### 5.2 Mapping PT_LOAD segments

```c
for (int i = 0; i < phnum; i++) {
    if (phdr[i].p_type != PT_LOAD) continue;
    
    // Target address
    void *target = coherent_base + (phdr[i].p_vaddr & ~(align-1));
    
    // Map with temporary PROT_WRITE
    mmap(target, map_size, prot | PROT_WRITE,
         MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
    
    // Copy contents
    memcpy(dest, combined_data + phdr[i].p_offset, phdr[i].p_filesz);
    
    // Zero BSS
    if (phdr[i].p_memsz > phdr[i].p_filesz)
        memset(dest + p_filesz, 0, p_memsz - p_filesz);
    
    // Drop PROT_WRITE if not needed
    if (!(prot & PROT_WRITE))
        mprotect(target, map_size, prot);
}
```

---

## 6. PIE relocation

### 6.1 Three-level strategy

```
Level 1: standard DT_RELA
    ↓ (if insufficient)
Level 2: patch GOT via DT_PLTGOT
    ↓ (last resort)
Level 3: RW segment heuristic scan
```

### 6.2 Level 1: DT_RELA (`stub_reloc.h:75-121`)

```c
// Find DT_RELA and DT_RELASZ in PT_DYNAMIC
Elf64_Rela *rela = ...;
size_t rela_count = relasz / sizeof(Elf64_Rela);

for (size_t i = 0; i < rela_count; i++) {
    if (ELF64_R_TYPE(rela[i].r_info) == R_X86_64_RELATIVE) {
        uint64_t *target = base + rela[i].r_offset;
        *target = base + rela[i].r_addend;
    }
}
```

### 6.3 Level 2: GOT via DT_PLTGOT (`stub_reloc.h:124-184`)

```c
// Patch GOT entries pointing to the old space
uint64_t *got = (uint64_t *)(base + dt_pltgot);
for (int i = 0; got[i] != 0 && i < MAX_GOT; i++) {
    if (got[i] < old_base + old_size) {
        got[i] += reloc_delta;
    }
}
```

### 6.4 Level 3: RW heuristic (`stub_reloc.h:192-225`)

> ⚠️ Last resort; may corrupt if detection is wrong.

```c
// Scan all RW segments for pointers in the old range
for (uint64_t *p = rw_start; p < rw_end; p++) {
    if (*p >= old_base && *p < old_base + old_size) {
        *p += reloc_delta;
    }
}
```

---

## 7. Handoff to ld.so

### 7.1 Dynamic binaries

```c
// 1. Find interpreter path from PT_INTERP
const char *interp_path = base + phdr_interp.p_offset;
// Example: "/lib64/ld-linux-x86-64.so.2"

// 2. Open and map ld.so
int fd = open(interp_path, O_RDONLY);
struct stat st; fstat(fd, &st);

// 3. Map its PT_LOAD segments
Elf64_Ehdr *ld_ehdr = mmap(...);
for (PT_LOAD in ld_ehdr) {
    mmap(interp_base + p_vaddr, ...);
}

// 4. Create the hatch (syscall trampoline)
// Sequence: syscall; pop rdx; ret (0F 05 5A C3)
void *hatch = make_hatch_x86_64(phdr, reloc, mask);
```

### 7.2 AUXV patching

The Auxiliary Vector must be updated to point to the new binary:

```c
Elf64_auxv_t *auxv = get_auxv_from_stack(original_sp);

auxv_up(auxv, AT_PHDR,  base + ehdr->e_phoff);
auxv_up(auxv, AT_PHNUM, ehdr->e_phnum);
auxv_up(auxv, AT_PHENT, ehdr->e_phentsize);
auxv_up(auxv, AT_ENTRY, base + ehdr->e_entry);
auxv_up(auxv, AT_BASE,  interp_base);  // ld.so base
```

### 7.3 Control transfer

```c
// Call elfz_fold_final_sequence (assembly)
// Args: ld.so entry, original stack, stub params, hatch
elfz_fold_final_sequence(loader_entry, original_sp,
                         ADRU, LENU, coherent_base,
                         interp_base, hatch, saved_rdx);
// Never returns — jumps to ld.so entry
```

---

## 8. Exit codes and debugging

### 8.1 Error codes

| Code | Meaning | Context |
|------|---------|---------|
| 1 | Generic error | Password, assertions |
| 3 | `mmap` failure | Memory allocation |
| 4 | Marker not found | VMA scan, header validation |
| 5 | Decompression failure | `decomp_result < 0` or size mismatch |
| 6 | Invalid ELF | Missing ELF magic, mapping failure |
| 11 | Params block missing | `+zELF-PR` magic missing (static) |
| 12 | Params block corrupt | Magic validation failed (password mode) |
| 65 | Params not patched | `virtual_start` or `packed_data` = 0 |
| 66 | Marker mismatch | Wrong COMP_MARKER (ZSTD static) |
| 67 | Invalid ZSTD magic | Magic ≠ 0xFD2FB528 |
| 88 | Hatch not created | make_hatch_x86_64 failed |
| 90 | Sanity check metadata | Sizes out of range |
| 91 | PHDR bounds check | Bad phentsize or phoff |
| 98 | Hatch overlap | Hatch in unmapped area |
| 99 | Entry overlap | Entry point in unmapped area |

### 8.2 Debugging

#### With strace

```bash
strace -f ./packed_binary 2>&1 | head -100
# Inspect mmap, mprotect, open
```

#### Inspect exit code

```bash
./packed_binary
echo "Exit code: $?"
# See the table above
```

#### Debug output (dev only)

In the stub, temporarily uncomment:

```c
// z_syscall_write(1, "[DBG] marker found\n", 19);
```

Rebuild and retest.

#### GDB (tricky without libc)

```bash
gdb ./packed_binary
break *0x401000  # Approximate _start
run
stepi
```

### 8.3 Common issues

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Exit 3 | Not enough memory | Check ulimit, RAM |
| Exit 4 | Marker not found | Corrupt binary or wrong codec |
| Exit 5 | Decompression failed | Corrupt data |
| Exit 6 | Not a valid ELF | Wrong filter applied |
| Exit 88 | No R+X segment | Atypical binary with no code |
| SIGSEGV | Relocation failed | PIE with unusual layout |

### 8.4 ZSTD-specific issues

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Exit 4 (infinite `/proc/self/maps` loop) | Backward VMA scan not implemented | Ensure `stub_dynamic.c` has backward scan for `CODEC_ZSTD` |
| SIGSEGV after library load (filter=none) | BCJ decode applied incorrectly | Check `stub_core_dynamic.inc` reads flag from `elfz_params_block`, not packed data |
| Debug prints in `stub_dynamic.c` not appearing | Wrong stub file | For BCJ stubs, edit `stub_core_dynamic.inc` instead |
| Packed size unchanged after code edits | Stubs not rebuilt | Run `make clean && make` to regenerate embedded stubs |

