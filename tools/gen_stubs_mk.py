#!/usr/bin/env python3
import os

def _parse_algos_env():
    v = os.environ.get('ELFZ_STUB_ALGOS', '').strip()
    if not v:
        return None
    if v.lower() == 'all':
        return None
    parts = []
    for tok in v.replace(',', ' ').split():
        t = tok.strip()
        if t:
            parts.append(t)
    return set(parts) if parts else None

# Configuration definitions
ALGOS = {
    'lz4': {
        'sources': ['src/decompressors/lz4/codec_lz4.c'],
        'defines': [], # LZ4 is default
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp'] # Kept from original makefile
    },
    'rnc': {
        'sources': ['src/decompressors/rnc/rnc_minidec.c'],
        'defines': ['CODEC_RNC'],
        'extra_includes': ['src/decompressors/rnc', 'src/compressors/qlz', 'src/compressors/pp']
    },
    'apultra': {
        'sources': ['src/decompressors/apultra/apultra_decompress.c'],
        'defines': ['CODEC_APULTRA'],
        'extra_includes': ['src/decompressors/apultra', 'src/compressors/qlz', 'src/compressors/pp']
    },
    'lzav': {
        'sources': ['src/decompressors/lzav/lzav_decompress.c'],
        'defines': ['CODEC_LZAV'],
        'extra_includes': ['src/decompressors/lzav', 'src/compressors/qlz', 'src/compressors/pp']
    },
    'zstd': {
        'sources': ['src/decompressors/zstd/zstd_minidec.c'],
        'defines': ['CODEC_ZSTD'],
        'extra_includes': ['src/decompressors/zstd/third_party/muzscat', 'src/compressors/qlz', 'src/compressors/pp']
    },
    'blz': {
        'sources': ['src/decompressors/brieflz/brieflz_decompress.c', 'src/decompressors/brieflz/depack.c'],
        'defines': ['CODEC_BRIEFLZ'],
        'extra_includes': []
    },
    'doboz': {
        'sources': ['src/decompressors/doboz/doboz_decompress.c'],
        'defines': ['CODEC_DOBOZ'],
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp']
    },
    'exo': {
        'sources': ['src/decompressors/exo/exo_decompress.c'],
        'defines': ['CODEC_EXO'],
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp']
    },
    'pp': {
        'sources': ['src/decompressors/pp/powerpacker_decompress.c'],
        'defines': ['CODEC_PP'],
        'extra_includes': ['src/decompressors/pp', 'src/compressors/qlz', 'src/compressors/pp']
    },
    'qlz': {
        'sources': ['src/decompressors/quicklz/quicklz.c'],
        'defines': ['CODEC_QLZ'],
        'extra_includes': ['src/compressors/qlz']
    },
    'snappy': {
        'sources': ['src/decompressors/snappy/snappy_decompress.c'],
        'defines': ['CODEC_SNAPPY'],
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp']
    },
    'shrinkler': {
        'sources': ['src/decompressors/shrinkler/shrinkler_decompress.c', '$(SHRINKLER_STUB_ASM_OBJ)'],
        'defines': ['CODEC_SHRINKLER'],
        'extra_includes': ['src/decompressors/shrinkler']
    },
    'lzma': {
        'sources': ['src/decompressors/lzma/minlzma_decompress.c', 'src/decompressors/lzma/minlzma_core.c'],
        'defines': ['CODEC_LZMA', '_LZMA_SIZE_OPT'],
        'extra_includes': ['src/decompressors/lzma', 'src/compressors/lzma/C', 'src/compressors/qlz', 'src/compressors/pp']
    },
    'zx7b': {
        'sources': ['src/decompressors/zx7b/zx7b_decompress.c'],
        'defines': ['CODEC_ZX7B'],
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp']
    },
    'zx0': {
        'sources': ['src/decompressors/zx0/zx0_decompress.c'],
        'defines': ['CODEC_ZX0'],
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp']
    },
    'density': {
        'sources': [
            'src/decompressors/density/density_minidec.c',
            'src/decompressors/density/buffer.c',
            'src/decompressors/density/stream.c',
            'src/decompressors/density/memory_teleport.c',
            'src/decompressors/density/memory_location.c',
            'src/decompressors/density/main_decode.c',
            'src/decompressors/density/block_decode.c',
            'src/decompressors/density/kernel_lion_decode.c',
            'src/decompressors/density/kernel_lion_dictionary.c',
            'src/decompressors/density/kernel_lion_form_model.c',
            'src/decompressors/density/main_header.c',
            'src/decompressors/density/main_footer.c',
            'src/decompressors/density/block_header.c',
            'src/decompressors/density/block_footer.c',
            'src/decompressors/density/block_mode_marker.c',
            'src/decompressors/density/globals.c'
        ],
        'defines': ['CODEC_DENSITY'],
        'extra_includes': ['src/decompressors/density']
    },
    'sc': {
        'sources': ['src/decompressors/stonecracker/sc_decompress.c'],
        'defines': ['CODEC_SC'],
        'extra_includes': ['src/decompressors/stonecracker']
    },
    'lzsa': {
        'sources': [
            'src/decompressors/lzsa/libs/decompression/lzsa2_decompress.c',
            'src/decompressors/lzsa/decompressor/expand_inmem.c',
            'src/decompressors/lzsa/decompressor/expand_block_v1.c',
            'src/decompressors/lzsa/decompressor/expand_block_v2.c',
            'src/decompressors/lzsa/decompressor/expand_context.c',
            'src/decompressors/lzsa/upstream/frame.c'
        ],
        'defines': ['CODEC_LZSA'],
        'extra_includes': [
             'src/decompressors/lzsa/upstream',
             'src/decompressors/lzsa/upstream/libdivsufsort/include'
        ]
    },
    'lzham': {
        'sources': [
            'src/decompressors/lzham/lzhamd_decompress.c',
            'src/decompressors/lzham/lzhamd_huffman.c',
            'src/decompressors/lzham/lzhamd_models.c',
            'src/decompressors/lzham/lzhamd_symbol_codec.c',
            'src/decompressors/lzham/lzhamd_tables.c'
        ],
        'defines': ['CODEC_LZHAM'],
        'extra_includes': ['src/decompressors/lzham']
    },
    'lzfse': {
        'sources': [
            'src/decompressors/lzfse/lzfse_decode_base.c',
            'src/decompressors/lzfse/lzfse_decompress.c',
            'src/decompressors/lzfse/lzfse_fse_stub.c',
            'src/decompressors/lzfse/lzvn_decode_base.c'
        ],
        'defines': ['CODEC_LZFSE', 'NDEBUG'],
        'extra_includes': ['src/decompressors/lzfse', 'src/compressors/qlz', 'src/compressors/pp']
    },
    'csc': {
        'sources': [
            'src/decompressors/csc/csc_dec.c',
            'src/decompressors/csc/csc_filters_dec.c'
        ],
        'defines': ['CODEC_CSC'],
        'extra_includes': ['src/decompressors/csc']
    },
    'nz1': {
        'sources': [
            'src/decompressors/nz1/nz1_decompressor.c'
        ],
        'defines': ['CODEC_NZ1'],
        'extra_includes': ['src/decompressors/nz1']
    }
}

VARIANTS = {
    'standard': {
        'suffix_elf': '',
        'suffix_bin': 'kexe', # Kanzi EXE / No Filter
        'defines': ['ELFZ_FILTER_KANZIEXE'],
        'sources': [],
        'extra_includes': []
    },
    'bcj': {
        'suffix_elf': '_bcj',
        'suffix_bin': 'bcj',
        'defines': ['ELFZ_FILTER_BCJ'],
        'sources': ['$(STUB_BCJ_MIN_SRC)'],
        'extra_includes': []
    }
}

MODES = {
    'dynamic': {
        'core_src': '$(STUB_SRC)/stub_dynamic.c',
        'extra_objs': ['$(STUB_SRC)/mini_mem.S']
    },
    'dynexec': {
        'core_src': '$(STUB_SRC)/stub_dynexec.c',
        'extra_objs': ['$(STUB_SRC)/mini_mem.S']
    },
    'static': {
        'core_src': '$(STUB_SRC)/stub_static.c',
        'extra_objs': ['$(BUILD_DIR)/static_final_noreturn.o', '$(STUB_SRC)/mini_mem.S']
    }
}

def generate_makefile():
    print("# Auto-generated stubs makefile")
    print("# Generated by tools/gen_stubs_mk.py")
    print("")
   
    allow_algos = _parse_algos_env()
    stub_bin_files_all = []

    stub_bin_files = []
    all_bins = []
    all_sfx_bins = []
    # Iterate over all combinations
    for algo_name, algo_conf in ALGOS.items():
        for variant_name, variant_conf in VARIANTS.items():
            for mode_name, mode_conf in MODES.items():
                for pwd in [False, True]:
                    pwd_suffix_bin = "_pwd" if pwd else ""
                    variant_bin = variant_conf['suffix_bin']
                    suffix_part = f"_{variant_bin}"
                    bin_file = f"stub_{algo_name}_{mode_name}{suffix_part}{pwd_suffix_bin}.bin"
                    stub_bin_files_all.append(bin_file)

        if allow_algos is not None and algo_name not in allow_algos:
            continue
        for variant_name, variant_conf in VARIANTS.items():
            for mode_name, mode_conf in MODES.items():
                for pwd in [False, True]:
                   
                    # Construct target names
                    # ELF: stub_pre_{mode}_{algo}{variant_suffix}{pwd_suffix}.elf
                    # e.g. stub_pre_dynamic_lz4.elf or stub_pre_dynamic_lz4_bcj_pwd.elf
                   
                    pwd_suffix_elf = "_pwd" if pwd else ""
                    variant_suffix_elf = variant_conf['suffix_elf']
                   
                    target_elf = f"$(BUILD_DIR)/stub_pre_{mode_name}_{algo_name}{variant_suffix_elf}{pwd_suffix_elf}.elf"
                   
                    # BIN: stub_{algo}_{mode}_{variant_bin}{pwd_suffix}.bin
                    # e.g. stub_lz4_dynamic_kexe.bin
                    pwd_suffix_bin = "_pwd" if pwd else ""
                    variant_bin = variant_conf['suffix_bin']
                   
                    # Construct suffix part
                    suffix_part = f"_{variant_bin}"

                    bin_file = f"stub_{algo_name}_{mode_name}{suffix_part}{pwd_suffix_bin}.bin"
                    target_bin = f"$(STUBS_HOST_DIR)/{bin_file}"

                    stub_bin_files.append(bin_file)
                    all_bins.append(target_bin)
                   
                    # Build dependencies and command
                    deps = ["$(STUB_LINK_OBJS)"]
                   
                    # Core Stub Source
                    # Check if special BCJ source file is needed (legacy makefile used stub_dynamic_bcj.c)
                    # We will try to map: if variant==bcj, append _bcj to core source basename
                    core_src = mode_conf['core_src']
                    if variant_name == 'bcj':
                        # src/stub/stub_dynamic.c -> src/stub/stub_dynamic_bcj.c
                        core_src = core_src.replace(".c", "_bcj.c")
                   
                    deps.append(core_src)

                    deps.append("$(STUB_SRC)/stub_loader.h")
                    deps.append("$(STUB_SRC)/stub_syscalls.h")
                    deps.append("$(STUB_SRC)/stub_utils.h")
                    deps.append("$(STUB_SRC)/stub_defs.h")
                    deps.append("$(STUB_SRC)/stub_elf.h")
                   
                    # Algo sources
                    deps.extend(algo_conf['sources'])
                   
                    # Variant sources (filters)
                    deps.extend(variant_conf['sources'])
                   
                    # Mode extra objs
                    deps.extend(mode_conf['extra_objs'])
                   
                    # Linker script
                    deps.append("$(STUB_SRC)/stub.ld")
                   
                    # Flags
                    defines = []
                    defines.extend(algo_conf['defines'])
                    defines.extend(variant_conf['defines'])
                    if pwd:
                        defines.append("ELFZ_STUB_PASSWORD")
                       
                    includes = []
                    includes.append("-I $(STUB_SRC)")
                    for inc in algo_conf['extra_includes']:
                        includes.append(f"-I {inc}")
                   
                    cflags = "$(STUB_PROD_CFLAGS)"
                    ldflags = "$(STUB_PROD_LDFLAGS)"
                   
                    # Rule for ELF
                    print(f"{target_elf}: {' '.join(deps)} | $(BUILD_DIR)")
                    print(f'\t@printf "\\033[37;48;5;19m○─○ Linking stub {algo_name.upper()} ({mode_name}, {variant_name}, pwd={pwd})...\\033[0m\\n"')
                   
                    defines_str = " ".join([f"-D{d}" for d in defines])
                    includes_str = " ".join(includes)
                   
                    # GCC command
                    # Note: stub.ld is passed via -T; everything else is provided as inputs.
                    link_inputs = [d for d in deps if d != "$(STUB_SRC)/stub.ld"]
                    src_files_str = " ".join(link_inputs)
                   
                    print(f"\t@$(CC) $(FUSE_LD_FLAG) {defines_str} -nostdlib -static -Os $(STUB_LTO_FLAG) -ffunction-sections -fdata-sections -fvisibility=hidden -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fno-builtin -fomit-frame-pointer -falign-functions=1 -falign-jumps=1 -falign-loops=1 -falign-labels=1 -fmerge-all-constants -fno-ident {cflags} {includes_str} -o $@ {src_files_str} -Wl,-e,_start,-T,$(STUB_SRC)/stub.ld -Wl,-z,noexecstack -Wl,--build-id=none -Wl,--relax -Wl,--gc-sections -Wl,-O1,--strip-all {ldflags}")
                    print("")
                    # Rule for BIN
                    # Use a pattern rule or specific rule? Specific is safer to avoid conflicts
                    print(f"{target_bin}: {target_elf} | $(STUBS_HOST_DIR)")
                    print(f'\t@printf "\\033[37;44m⚡ Generating BIN for {algo_name.upper()} ({mode_name}, {variant_name}, pwd={pwd})...\\033[0m\\n"')
                    print(f"\t@objcopy -O binary -j .text -j .rodata $(if $(filter arm64,$(HOST_ARCH)),-j .got -j .got.plt,) $< $@")
                    print("")

    # SFX stubs (extract-only self-extracting archives)
    for algo_name, algo_conf in ALGOS.items():
        bin_file = f"stub_sfx_{algo_name}.bin"
        stub_bin_files_all.append(bin_file)
        stub_bin_files.append(bin_file)

    print("ifeq ($(STUB_ENABLE_SFX),1)")
    for algo_name, algo_conf in ALGOS.items():
        target_elf = f"$(BUILD_DIR)/stub_sfx_{algo_name}.elf"
        bin_file = f"stub_sfx_{algo_name}.bin"
        target_bin = f"$(STUBS_HOST_DIR)/{bin_file}"
        all_sfx_bins.append(target_bin)

        print(f"{target_elf}: $(BUILD_DIR)/start_sfx.o $(STUB_SRC)/stub_sfx.c {' '.join(algo_conf['sources'])} $(STUB_SRC)/mini_mem.S $(STUB_SRC)/stub.ld | $(BUILD_DIR)")
        print(f'\t@printf "\\033[37;48;5;19m○─○ Linking SFX stub {algo_name.upper()}...\\033[0m\\n"')

        defines = []
        defines.extend(algo_conf['defines'])
        defines_str = " ".join([f"-D{d}" for d in defines])

        includes = []
        includes.append("-I $(STUB_SRC)")
        for inc in algo_conf['extra_includes']:
            includes.append(f"-I {inc}")
        includes_str = " ".join(includes)

        cflags = "$(STUB_PROD_CFLAGS)"
        ldflags = "$(STUB_PROD_LDFLAGS)"

        src_files = [
            "$(BUILD_DIR)/start_sfx.o",
            "$(STUB_SRC)/stub_sfx.c",
            *algo_conf['sources'],
            "$(STUB_SRC)/mini_mem.S",
        ]
        src_files_str = " ".join(src_files)

        print(f"\t@$(CC) $(FUSE_LD_FLAG) {defines_str} -nostdlib -static -Os $(STUB_LTO_FLAG) -ffunction-sections -fdata-sections -fvisibility=hidden -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fno-builtin -fomit-frame-pointer -falign-functions=1 -falign-jumps=1 -falign-loops=1 -falign-labels=1 -fmerge-all-constants -fno-ident {cflags} {includes_str} -o $@ {src_files_str} -Wl,-e,_start,-T,$(STUB_SRC)/stub.ld -Wl,-z,noexecstack -Wl,--build-id=none -Wl,--relax -Wl,--gc-sections -Wl,-O1,--strip-all {ldflags}")
        print("")

        print(f"{target_bin}: {target_elf} | $(STUBS_HOST_DIR)")
        print(f'\t@printf "\\033[37;44m⚡ Generating SFX BIN for {algo_name.upper()}...\\033[0m\\n"')
        print(f"\t@objcopy -O binary -j .text -j .rodata $(if $(filter arm64,$(HOST_ARCH)),-j .got -j .got.plt,) $< $@")
        print("")
    print("endif")

    # Output aggregate variables
    print("STUB_BIN_FILES_ALL = \\")
    for f in stub_bin_files_all:
        print(f"\t{f} \\")
    print("")

    print("STUB_BIN_FILES = \\")
    for f in stub_bin_files:
        print(f"\t{f} \\")
    print("")

    print("ALL_STUB_BINS = \\")
    for b in all_bins:
        print(f"\t{b} \\")
    print("")

    print("ifeq ($(STUB_ENABLE_SFX),1)")
    print("ALL_STUB_BINS += \\")
    for b in all_sfx_bins:
        print(f"\t{b} \\")
    print("")
    print("endif")
    print("")

if __name__ == "__main__":
    generate_makefile()
