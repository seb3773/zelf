# Compressed Decompressor (Two-Stage Stub) — Developer Guide

This document describes the **two-stage stub** mechanism used by zELF to reduce the on-disk size of **large codec stubs** (e.g. Zstandard, Density). The idea is to ship a tiny **stage-0** loader that **decompresses the real codec stub** at runtime, then transfers control to it.

In short:

- **Stage-0**: very small, position-independent, minimal dependencies.
- **Stage-1 (real stub)**: the regular codec stub (static or dynamic), as currently generated/embedded by the project.
- **Payload**: the packed block (`marker + metadata + compressed_data`) that stage-1 will decode and execute.

The current codebase implements this mechanism **for `zstd`** (both static and dynamic) and **for `density`** (dynamic only). This guide explains how it works and how to wire it for another codec if needed.

> **Note on Density**: Density uses stage0 only for **dynamic binaries**. For static binaries, density does NOT use stage0 because the density stub (~25KB) doesn't compress well enough with NRV2B to offset the stage0 overhead. ZSTD benefits from stage0 because its stub compresses much better with NRV2B. See the "Density: Why No Stage0 for Static Binaries" section below for details.

---

## Goals and non-goals

### Goals
- Reduce the **file size** of large decompression stubs while keeping runtime behavior identical.
- Keep stage-0 as small as possible (C first; assembly if size-critical).
- Keep the mechanism **codec-scoped** (do not break other codecs).
- Preserve the existing packer/stub pipeline (only wrap the stub after final patching).

### Non-goals
- A universal multi-codec stage-0 (not necessary; can be per-codec).
- A portable implementation across OS/arch (target is Linux ELF64 x86_64 and AArch64).

---

## Terminology

- **Packed block**: the appended data region containing marker + metadata + the codec-compressed payload.
- **Stage-0**: small loader that expands the stage-1 stub.
- **Stage-1**: the real codec stub (the usual zELF stub), which then decodes the packed block and executes the original ELF.
- **Wrapper**: the file bytes for the stub segment after applying two-stage compression: `[stage0][header][compressed_stage1]`.

---

## Why this exists (when to use it)

Use a two-stage stub when:
- The codec’s decompressor stub is **large** (tens of KB) and dominates the final packed binary size.
- The codec stub compresses well with a secondary fast compressor (NRV/UCL-like), yielding meaningful size savings.

Do **not** use it when:
- The codec stub is already tiny.
- The secondary compression ratio is poor or stage-0 becomes too large.
- You cannot satisfy runtime constraints (memory, permissions, segment layout).

---

## Current implementation overview (zstd + density)

The current pipeline for the wrapped codec is:

1. Select the embedded stage-1 stub (the normal codec stub).
2. Apply final patching to the stage-1 stub (addresses, flags, etc.).
3. Compress the patched stage-1 stub using **NRV2B LE32** (UCL).
4. Create the wrapper blob:
   - `stage0_nrv2b_le32.bin` (PIC stage-0 entry)
   - stage-0 header (patched offsets/sizes)
   - NRV2B-compressed stage-1 stub bytes
5. Build the final packed ELF with proper `PT_LOAD` sizing so that:
   - wrapper bytes exist in the file (`p_filesz`)
   - enough memory exists for stage-0 to write the decompressed stage-1 stub (`p_memsz`)
6. Runtime:
   - stage-0 decodes the NRV stream into memory
   - stage-0 jumps to the decompressed stage-1 stub
   - stage-1 decodes the packed block and launches the original program

Key code locations:
- **Packer wrapper creation**: `src/packer/zelf_packer.c` (`build_zstd_stage0_blob(...)` for zstd and `build_density_stage0_blob(...)` for density, same format)
- **ELF layout / PT_LOAD sizes**: `src/packer/elf_builder.c` (`write_packed_binary(...)`)
- **Stage-0 implementation**:
  - Assembly entry: `src/stub/stage0_nrv2b_le32.S` (x86_64) and `src/stub/stage0_nrv2b_le32_arm64.S` (AArch64)
  - Minimal NRV decoder in C: `src/tools/ucl/minidec_nrv2b_le32.c` and `.h`
  - Offsets headers and binary blobs (per arch):
    - `build/stage0_x86_64/stage0_nrv2b_le32_offsets_x86_64.h`, `build/stage0_x86_64/stage0_nrv2b_le32_x86_64.bin`
    - `build/stage0_arm64/stage0_nrv2b_le32_offsets_arm64.h`, `build/stage0_arm64/stage0_nrv2b_le32_arm64.bin`

---

## Wrapper format (what stage-0 expects)

The file bytes at the stub location become:

```
[ stage0 code bytes ... ]
[ stage0 header (fixed offsets, patched by packer) ]
[ NRV2B-compressed stage-1 stub bytes ... ]
```

The stage-0 header fields are patched by the packer. Conceptually:

- **`ulen`**: uncompressed stage-1 size (sometimes “cap”, see notes)
- **`clen`**: compressed stage-1 size (NRV stream length)
- **`dst_off`**: destination offset (relative to wrapper base) where stage-1 will be written
- **`blob_off`**: offset (relative to wrapper base) where compressed bytes begin
- **`flags`**: 1 byte, stage-0 metadata flags (currently used to propagate EXE filter choice)

### Stage-0 `flags` field (EXE filter propagation)

The stage-0 header contains a 1-byte `flags` field at `STAGE0_HDR_FLAGS_OFF` (currently `0x62`).

This field is written by the packer when building the wrapper and is used by the unpacker to reliably recover the selected EXE filter for stage0-wrapped stubs (ZSTD and Density dynamic).

Bit semantics match the packer `elfz_flags` encoding:

- `flags & 0x01`: BCJ filter enabled
- `flags & 0x02`: KanziEXE filter enabled

Typical values:

- `0x00`: no filter
- `0x01`: BCJ
- `0x02`: KanziEXE

### Important constraint: do not patch stage-0 wrapper contents

Once stage-1 has been NRV-compressed into the wrapper, **any later byte patching inside the wrapper may corrupt the NRV stream**.

Therefore:

- Patch `.elfz_params` and runtime flags **in stage-1 bytes before compression**.
- Do not run generic “patch stub blob” code on the wrapper.

The current code implements exactly that behavior for zstd.

In addition, because the wrapper is an outer container, stage-0 wrapped stubs must not rely on heuristic filter detection at unpack time. The filter choice is propagated via the stage-0 header `flags` field.

---

## Runtime memory layout constraints (static vs dynamic)

Two-stage decompression requires placing the decompressed stage-1 stub into memory without colliding with:
- file-backed stub bytes
- the packed block
- other mappings (especially for dynamic layout where multiple PT_LOAD map from file offset 0)

### Dynamic binaries (`ET_DYN`, `PT_INTERP` present)

Dynamic packed layout in this project maps the packed binary in a way where the packed block is effectively placed close to the stub in the file mapping.

To avoid overwriting the packed block at runtime, the destination is chosen as:

- `dst_off = align_up(filesz + packed_block_size, 16)`

This ensures:
- stage-0 file bytes are safe
- decompressed stage-1 is written **after** the packed block region in memory

### Static binaries (`ET_EXEC`, no `PT_INTERP`)

Static packed layout uses a distinct `PT_LOAD` for the packed block (at `packed_vaddr`).

If stage-0 writes the decompressed stage-1 stub into an address range that overlaps the packed segment’s pages, the write can fault with `SEGV_ACCERR` due to page permissions and/or the kernel’s mapping choice.

To avoid this, the zstd implementation uses a fixed destination offset:

- `dst_off = 0x8000` (must remain `< (packed_vaddr - stub_vaddr)`; in the current layout that gap is `0x10000`)

This keeps the decompressed stub in a stable and non-overlapping area inside the stub segment mapping.

---

## `.elfz_params` patching (critical for static)

Stage-1 stubs use `.elfz_params` to locate the packed block and compute an offset (`aslr_offset`) based on where the stub is mapped.

When stage-1 is decompressed to a **different base** than the wrapper base:

- `.elfz_params.virtual_start` must match the **decompressed stage-1 base** (static case), otherwise stage-1 will compute a wrong `aslr_offset` and look for the marker at the wrong address.

For static zstd two-stage:
- wrapper base: `stub_vaddr`
- decompressed stage-1 base: `stub_vaddr + dst_off`
- so patch:
  - `virtual_start = stub_vaddr + dst_off`
  - `packed_data_vaddr = packed_vaddr`

For dynamic binaries, `.elfz_params` may be unused/absent depending on the stub variant and runtime resolution strategy; do not rely on static assumptions there.

---

## `PT_LOAD` sizing and permissions (what the kernel must map)

Two-stage requires a `PT_LOAD` segment that covers **both**:
- the wrapper bytes present in the file (`p_filesz`)
- the destination region for the decompressed stage-1 (`p_memsz` must be large enough)

Additionally:
- stage-0 must be able to **write** into the destination range → the mapping must be writable.
- stage-0 and stage-1 must execute → mapping must be executable where code lives.

For static zstd, the first segment is configured as **RWE** to allow stage-0 writes and execution.

For `ET_EXEC`, do not violate kernel alignment constraints:
- `p_offset % p_align == p_vaddr % p_align`

This is why the wrapper’s file offset must stay page-aligned for static binaries.

---

## Packed block layout compatibility

The stage-0 wrapper must reserve memory so it does not overwrite the packed block (dynamic case).
To do that, the packer needs an estimate of `packed_block_size`.

Important:
- zstd currently uses a **legacy packed block layout** (no `filtered_size` field).
- Other codecs may use a newer layout with additional fields.

If you apply the two-stage mechanism to another codec, you must:
- define which packed block layout that codec uses at runtime
- compute `packed_block_size` accordingly

---

## How to enable two-stage compression for another codec

This section is a checklist to implement the mechanism for another codec (call it `foo`).

### 1) Decide the stage-0 decompressor algorithm

The current implementation uses:
- **NRV2B LE32** (UCL) for stage-1 compression
- minimal decoder embedded for stage-0

If you keep NRV2B:
- you can reuse the existing stage-0 (`stage0_nrv2b_le32`) as-is
- only the packer wrapper code and layout decisions change per codec

If you choose another secondary compressor:
- you must provide a minimal stage-0 decoder with the same constraints (PIC, no libc).

### 2) Add a “wrap stub” step in the packer (codec-scoped)

Implement a codec-scoped block similar to the current zstd path:

- Create a mutable copy of the stage-1 stub bytes.
- Patch `.elfz_params` and flags **into this copy**.
- Compress the patched stage-1 stub using the secondary compressor.
- Build wrapper: `[stage0][hdr][compressed_stage1]`.
- Replace the stub pointer/size passed to `write_packed_binary(...)`:
  - `stub_data = wrapper`
  - `stub_sz = wrapper_filesz`
  - `stub_memsz = wrapper_memsz` (must include destination space)

Files to change:
- `src/packer/zelf_packer.c`

### 3) Ensure `write_packed_binary()` does not corrupt the wrapper

The ELF builder typically patches `.elfz_params` and flags in the stub blob.
For a wrapped stub, this would patch bytes inside the NRV stream and break decoding.

Therefore:
- Skip `.elfz_params` patching in `elf_builder` for the wrapped codec, and do it earlier.

Files to change:
- `src/packer/elf_builder.c`

### 4) Choose destination placement rules (static vs dynamic)

You must pick `dst_off` rules that guarantee:
- no overlap with the packed block
- no overlap with other mapped segments
- the decompressed stage-1 base matches any metadata assumptions (static `.elfz_params`)

Recommended starting point:
- **Dynamic**: `dst_off = align_up(filesz + packed_block_size, 16)`
- **Static**: pick a fixed `dst_off` inside the stub segment mapping such that:
  - `dst_off >= filesz`
  - `dst_off + stage1_size` fits inside the stub segment `p_memsz`
  - `dst_off < (packed_vaddr - stub_vaddr)` (to avoid overlapping the packed PT_LOAD)

### 5) Set correct `PT_LOAD` flags

Stage-0 writes the decompressed stage-1 → the stub segment must be writable.

For safety during bring-up:
- set stub segment to **RWE** for the wrapped codec

Once stable:
- you may refine permissions (e.g. `RWX` during stage-0 then `mprotect` inside stage-1), but that requires additional runtime code.

Files to change:
- `src/packer/elf_builder.c`

### 6) Verify marker and metadata reading in stage-1

Because stage-1 is unchanged logic-wise, correctness depends on:
- `.elfz_params` pointing to correct `packed_data_vaddr` (static)
- correct runtime scan window / marker search (dynamic)
- packed block layout matching what stage-1 expects

If you “test” the two-stage mechanism on an existing codec, ensure the codec’s stub:
- can tolerate the extra indirection
- does not assume it is located at a fixed base equal to the wrapper base

### 7) Testing procedure

Minimal test flow:

- Pack a known tiny program with the codec `foo` and run it under `strace`.
- Validate:
  - no `execve(...)=EINVAL` (alignment)
  - no early `SIGSEGV` in stage-0 (permissions/overlap)
  - marker check succeeds (no “exit(66)” style failure)
  - final `execveat(memfd, ...)` handoff works and program output matches original

Useful tools:
- `readelf -l packed_binary` to verify:
  - alignment
  - `p_filesz` vs `p_memsz`
  - segment flags
- `gdb` for stage-0 faults (look for writes into non-writable or unmapped pages).

---

## Common failure modes and how to recognize them

- **`execve(...) = -1 EINVAL`**
  - Usually PT_LOAD alignment is broken for `ET_EXEC`.
  - Fix `stub_off` to be page-aligned if `stub_vaddr` is page-aligned, or adjust `p_align`.

- **Immediate `SIGSEGV` with `SEGV_ACCERR` at a destination address**
  - Stage-0 wrote into read-only pages (wrong mapping flags) or into a region mapped by another PT_LOAD.
  - Ensure stub PT_LOAD includes `PF_W` and destination does not overlap other PT_LOAD VMAs.

- **Stage-1 exits very early with an internal “marker not found” code**
  - `.elfz_params virtual_start` does not match decompressed stage-1 base (static), so `packed_data_vaddr + aslr_offset` is wrong.
  - Patch `virtual_start = decompressed_base` for static two-stage.

- **Decoder fails unpredictably**
  - Wrapper bytes were patched after compression (NRV stream corrupted).
  - Ensure `.elfz_params` patching is done before NRV compression and `elf_builder` skips wrapper patching.

---

## future improvements

- Replacing the “C first” stage-0 decoder with a smaller assembly decoder
- making the destination strategy more robust for static builds by computing a safe `dst_off` based on `packed_vaddr - stub_vaddr`, not a constant.




---

## Density: Why No Stage0 for Static Binaries

The density codec uses stage0 compression **only for dynamic binaries**, not for static binaries. This is a deliberate design decision based on empirical testing.

### The Problem

Stage0 compression works by:
1. Compressing the codec stub with NRV2B
2. Adding a small stage0 loader (~1KB)
3. At runtime, stage0 decompresses the stub before executing it

For this to reduce file size, the NRV2B-compressed stub + stage0 overhead must be smaller than the original stub.

### Why ZSTD Benefits from Stage0

The ZSTD stub (~23KB) compresses very well with NRV2B:
- Original stub: ~23KB
- NRV2B compressed: ~13KB
- Stage0 overhead: ~1KB
- **Total: ~14KB** (saves ~9KB)

### Why Density Does NOT Benefit from Stage0 (Static)

The density stub (~25KB) does not compress as well with NRV2B:
- Original stub: ~25KB
- NRV2B compressed: ~9KB
- Stage0 overhead: ~1KB
- **Total: ~10KB** (saves ~15KB in stub size)

However, the stage0 mechanism requires additional padding in the ELF layout to accommodate the decompression buffer (`stub_memsz`). For static binaries:
- `estimated_stub_sz` must be 65536 (64KB) to reserve space for decompression
- This creates ~50KB of padding between the stub and packed data
- **Net result: larger file than without stage0**

### Why Density DOES Benefit from Stage0 (Dynamic)

For dynamic binaries, the ELF layout is different:
- The stub and packed data share the same segment
- No padding is needed between them
- The decompression buffer is handled within the segment's `p_memsz`
- **Net result: smaller file with stage0**

### Implementation

In `zelf_packer.c`:
```c
} else if (strcmp(codec, "density") == 0) {
    if (has_pt_interp) {
      // Dynamic: use stage0 wrapper
      estimated_stub_sz = 65536;
      stub_off = 0x100;
    } else {
      // Static: no stage0, use stub directly
      estimated_stub_sz = 28000;
    }
  }
```

The stage0 wrap block only executes for density when `has_pt_interp` is true (dynamic binary).

### Summary

| Binary Type | ZSTD Stage0 | Density Stage0 | Reason |
|-------------|-------------|----------------|--------|
| Dynamic     | ✅ Yes      | ✅ Yes         | Both benefit from stub compression |
| Static      | ✅ Yes      | ❌ No          | Density's padding overhead exceeds compression savings |
