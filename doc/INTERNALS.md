# zELF packer internals: stub + payload injection

This document focuses on how the **packer** builds the final packed ELF file on disk: where the decompression stub is placed, how the packed payload is appended, which ELF headers/segments are created, what padding/alignment rules are used, and how **stage0** changes the layout.

For the runtime side (how the stub finds the payload, maps memory, relocates PIE, invokes ld.so, etc.), see `doc/STUB_LOADER_INTERNALS.md`.

## Scope and terminology

- **stub**: the small self-contained loader/decompressor executed first.
- **packed block / payload**: a marker + header + compressed data appended to the file.
- **dynamic target**: an executable that uses the dynamic loader (`PT_INTERP` present in the original), packed output uses `ET_DYN`.
- **static target**: a fully static binary (`PT_INTERP` absent), packed output uses `ET_EXEC`.
- **stage0 wrapper**: for some codecs, the “stub bytes on disk” are not the final stub, but a small stage0 + an NRV stream which decompresses stage1 stub into memory.

## Where the authoritative implementation lives

- `src/packer/elf_builder.c:write_packed_binary()` is the authoritative builder for packed ELF binaries.
- `src/packer/elf_utils.c` contains helper routines used by the builder (alignment, `.elfz_params` patching helpers).
- SFX writing is handled separately in `src/packer/archive_mode.c` (`write_sfx_executable()`); it follows similar ideas but is not the same layout.

## Design choice: no PT_NOTE conversion

Some packers inject payloads by converting a `PT_NOTE` into a `PT_LOAD` or by growing an existing segment. zELF does **not** rely on that mechanism.

Instead, zELF writes a **new minimal ELF**:
- It emits a fresh `Elf64_Ehdr` + a minimal set of `Elf64_Phdr`.
- It places the stub bytes at a known file offset.
- It appends a packed data block which the stub locates by scanning for a marker.

This approach keeps the on-disk representation predictable and avoids subtle interactions with original binaries’ segment layouts.

## Packed ELF layout (dynamic binaries)

Implemented in `write_packed_binary()` when `has_pt_interp != 0`.

### File layout

The packed ELF is written with:

- `[Elf64_Ehdr + Phdrs]`
- `[padding to 0x100]`
- `[stub bytes]`
- `[packed block]` (immediately after stub bytes)

The entry point is set to execute inside the mapped `C_TEXT` segment (see below). If the stub is placed after the 0x100 padding, entry becomes `0x5000 + 0x100`.

### Program headers (3 segments)

`e_phnum = 3`:

- **Segment 0: C_BASE (RW or RWX depending case)**
  - `p_type = PT_LOAD`
  - `p_offset = 0`
  - `p_vaddr = 0x0`
  - `p_filesz = 0x100 + stub_sz` (common case)
  - `p_memsz = p_filesz`

- **Segment 1: C_TEXT (RX, or RWX for stage0 codecs)**
  - `p_type = PT_LOAD`
  - `p_offset = 0` (when stub is within the first page layout) or `stub_off`
  - `p_vaddr = 0x5000`
  - `p_filesz = ctext_filesz` (includes stub bytes + packed block)
  - `p_memsz = ctext_memsz`

- **Segment 2: PT_GNU_STACK**

### Stage0 impact (dynamic)

For `zstd` and for `density` **when dynamic**, the file stub bytes are:

- `[stage0][hdr][NRV-compressed-stage1-stub]`

Key consequence: stage0 writes the decompressed stage1 stub **in-memory**, so the `C_TEXT` segment must reserve additional memory:

- `p_memsz` is increased to at least `0x100 + stub_memsz_effective`.

At the same time:

- `p_filesz` must remain consistent with the actual file size to avoid runtime SIGBUS on mapped pages.

In this configuration, `.elfz_params` patching inside `elf_builder.c` is **skipped** because modifying the on-disk bytes would corrupt the NRV stream. The stage1 stub is patched **before** NRV compression in `zelf_packer.c`.

## Packed ELF layout (static binaries)

Implemented in `write_packed_binary()` when `has_pt_interp == 0`.

### File layout

The packed ELF is written with:

- `[Elf64_Ehdr + 2*Phdr]`
- `[padding to stub_off]`
- `[stub bytes]`
- `[padding to packed_off (ALIGN=0x1000)]`
- `[packed block]`

Entry point is `e_entry = stub_vaddr` (absolute).

### Program headers (2 segments)

`e_phnum = 2`:

- **Segment 0: stub**
  - `p_type = PT_LOAD`
  - `p_offset = stub_off`
  - `p_vaddr = stub_vaddr`
  - `p_filesz = stub_sz`
  - `p_memsz = stub_memsz_effective`
  - `p_flags = PF_X|PF_R` (or also `PF_W` for zstd stage0 usage)
  - `p_align = ALIGN (0x1000)`

- **Segment 1: packed data block**
  - `p_type = PT_LOAD`
  - `p_offset = packed_off`
  - `p_vaddr = packed_vaddr`
  - `p_filesz = p_memsz_val`
  - `p_memsz = p_memsz_val`
  - `p_flags = PF_R`
  - `p_align = ALIGN (0x1000)`

### Stage0 impact (static)

Static behavior differs by codec:

- `zstd` uses stage0 and therefore the stub segment is marked writable (`PF_W|PF_R|PF_X`).
- `density` static does **not** use stage0 in this builder (see comment in `elf_builder.c`).

Unlike the dynamic stage0 case, static non-stage0 stubs can be patched in `elf_builder.c` via `.elfz_params` patching.

## Alignment and padding rules

- The global constant `ALIGN` is `0x1000` (`src/packer/packer_defs.h`).
- Dynamic mode uses a fixed “small header” placement and pads to `0x100` before stub bytes.
- Static mode uses explicit `stub_off` and `packed_off` (with alignment constraints), and may insert large padding between stub and packed block.

### Offset recomputation in static mode

`write_packed_binary()` contains a safeguard:

- It computes `pad3 = packed_off - current_pos`.
- If `pad3` becomes unexpectedly huge (> 1,000,000) and `current_pos` has surpassed the expected `packed_off`, it recomputes:
  - `packed_off = align_up(current_pos, ALIGN)`
  - `packed_vaddr = stub_vaddr + packed_off - stub_off`
  - then it rewrites the packed segment program header in-place.

If `.elfz_params` patching is in use (non-stage0, static), it also **re-patches** the stub with the corrected `packed_vaddr` and rewrites the stub bytes at `stub_off`.

This avoids producing an ELF whose PHDRs would point to a packed block offset that is not the one actually used.

## Packed block (payload) format

The packed data block is created at the end of `write_packed_binary()` and appended after padding.

Common layout (conceptual):

- `marker` (6 bytes)
- `original_size` (size_t)
- `entry_offset` (uint64_t)
- `compressed_size` (int)
- optionally `filtered_size` (int)
- optionally density KanziEXE extra tail (tag + 40 bytes)
- compressed stream bytes

Whether `filtered_size` is present depends on the “legacy layout” decision (`legacy_bcj_layout`), which depends on:

- target machine (`EM_X86_64` vs `EM_AARCH64`)
- codec
- filter mode (`EXE_FILTER_BCJ`, `EXE_FILTER_NONE`, `EXE_FILTER_KANZIEXE`)

The runtime stub locates the packed block by scanning memory for the marker; this is one of the reasons the builder can keep the layout simple and does not require a PT_NOTE trick.

## SFX (self-extracting) note

SFX executables are not built by `elf_builder.c`.

`archive_mode.c:write_sfx_executable()` writes a minimal `ET_DYN` file with:

- `stub_file_off = 0x1000`
- `stub_vaddr = 0x5000`
- `e_phnum = 2`:
  - a `PT_LOAD` for the stub
  - a `PT_GNU_STACK`

The payload is appended after stub bytes, followed by an explicit footer containing payload offset/size.

## Stage0: why it exists and why it affects patching

Stage0 exists to handle cases where the “real” stage1 stub is large but compresses well.

Because stage1 stub is stored on disk as an NRV stream, any in-place patching of parameters inside that byte stream would corrupt it.

Therefore:

- stage1 stub parameter patching must happen **before** NRV compression (packer-side), and `elf_builder.c` must treat the resulting on-disk bytes as immutable.

## Status

- **Created / present**: ELF rebuild based on minimal PHDRs, staged stub handling, payload appended with marker.
- **Not used currently**: PT_NOTE conversion.
