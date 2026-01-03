#!/usr/bin/env python3
import os

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
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp']
    },
    'lzav': {
        'sources': ['src/decompressors/lzav/lzav_decompress.c'],
        'defines': ['CODEC_LZAV'],
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp']
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
        'extra_includes': ['src/compressors/qlz', 'src/compressors/pp']
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
        'sources': ['src/decompressors/shrinkler/shrinkler_decompress.c', '$(BUILD_DIR)/shrinkler_decompress_x64.o'],
        'defines': ['CODEC_SHRINKLER'],
        'extra_includes': []
    },
    'lzma': {
        'sources': ['src/decompressors/lzma/minlzma_decompress.c', 'src/decompressors/lzma/minlzma_core.c'],
        'defines': ['CODEC_LZMA', '_LZMA_SIZE_OPT'],
        'extra_includes': ['src/compressors/lzma/C', 'src/compressors/qlz', 'src/compressors/pp']
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
        'extra_includes': []
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
        'defines': [],
        'sources': [],
        'extra_includes': []
    },
    'bcj': {
        'suffix_elf': '_bcj',
        'suffix_bin': 'bcj',
        'defines': ['ELFZ_FILTER_BCJ'],
        'sources': ['src/filters/bcj_x86_min.c'],
        'extra_includes': []
    }
}

MODES = {
    'dynamic': {
        'core_src': 'src/stub/stub_dynamic.c', # Base source, will be suffixed _bcj in some cases in original makefile but here we control defines
        # Actually original makefile used stub_dynamic_bcj.c for BCJ. Let's see if we can unify.
        # stub_dynamic_bcj.c is likely just stub_dynamic.c with some defines or small changes.
        # Let's check if we can just use defines.
        # Reading stub_main_dynamic.c, it uses ELFZ_FILTER_BCJ.
        # So we probably just need stub_dynamic.c for all, assuming stub_dynamic_bcj.c was just a wrapper or if we pass the define.
        # But wait, makefile explicitly used src/stub/stub_dynamic_bcj.c.
        # If stub_dynamic.c includes stub_main_dynamic.c, and stub_dynamic_bcj.c also includes it (or similar),
        # then passing -DELFZ_FILTER_BCJ to stub_dynamic.c should work IF stub_dynamic.c supports it.
        # Let's assume for now we replicate the filename logic to be safe.
        'extra_objs': ['src/stub/mini_mem.S']
    },
    'static': {
        'core_src': 'src/stub/stub_static.c',
        'extra_objs': ['$(BUILD_DIR)/static_final_noreturn.o', 'src/stub/mini_mem.S']
    }
}

def generate_makefile():
    print("# Auto-generated stubs makefile")
    print("# Generated by tools/gen_stubs_mk.py")
    print("")
   
    all_embed_objs = []
    # Iterate over all combinations
    for algo_name, algo_conf in ALGOS.items():
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
                   
                    target_bin = f"$(BUILD_DIR)/stubs/stub_{algo_name}_{mode_name}{suffix_part}{pwd_suffix_bin}.bin"
                   
                    # OBJ: embed_stub_{algo}_{mode}_{variant_bin}{pwd_suffix}.o
                    target_obj = f"$(BUILD_DIR)/embed_stub_{algo_name}_{mode_name}{suffix_part}{pwd_suffix_bin}.o"
                   
                    all_embed_objs.append(target_obj)
                   
                    # Build dependencies and command
                    deps = [
                        "$(BUILD_DIR)/start.o",
                        "$(BUILD_DIR)/elfz_handoff_gate.o",
                        "$(BUILD_DIR)/elfz_handoff_sequence.o"
                    ]
                   
                    # Core Stub Source
                    # Check if special BCJ source file is needed (legacy makefile used stub_dynamic_bcj.c)
                    # We will try to map: if variant==bcj, append _bcj to core source basename
                    core_src = mode_conf['core_src']
                    if variant_name == 'bcj':
                        # src/stub/stub_dynamic.c -> src/stub/stub_dynamic_bcj.c
                        core_src = core_src.replace(".c", "_bcj.c")
                   
                    deps.append(core_src)
                   
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
                    # Note: we filter out .ld file from compilation sources, it is passed to linker via -T
                    src_files = [d for d in deps if d.endswith('.c') or d.endswith('.S') or d.endswith('.s') or d.endswith('.o')]
                    src_files_str = " ".join(src_files)
                   
                    print(f"\t@$(CC) -fuse-ld=gold {defines_str} -nostdlib -static -Os -flto -ffunction-sections -fdata-sections -fvisibility=hidden -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fno-builtin -fomit-frame-pointer -falign-functions=1 -falign-jumps=1 -falign-loops=1 -falign-labels=1 -fmerge-all-constants -fno-ident {cflags} {includes_str} -o $@ {src_files_str} -Wl,-e,_start,-T,$(STUB_SRC)/stub.ld -Wl,-z,noexecstack -Wl,--build-id=none -Wl,--relax -Wl,--gc-sections -Wl,-O1,--strip-all {ldflags}")
                    print("")
                    # Rule for BIN
                    # Use a pattern rule or specific rule? Specific is safer to avoid conflicts
                    print(f"{target_bin}: {target_elf} | $(BUILD_DIR)/stubs")
                    print(f'\t@printf "\\033[37;44m⚡ Generating BIN for {algo_name.upper()} ({mode_name}, {variant_name}, pwd={pwd})...\\033[0m\\n"')
                    print(f"\t@objcopy -O binary -j .text -j .rodata $< $@")
                    print("")
                    # Rule for OBJ
                    print(f"{target_obj}: {target_bin} | $(BUILD_DIR)")
                    print(f'\t@printf "\\033[37;44m→→ Embedding BIN into OBJ for {algo_name.upper()} ({mode_name}, {variant_name}, pwd={pwd})...\\033[0m\\n"')
                    print(f"\t@$(LD) -r -b binary -o $@ $<")
                    print("")

    # Output aggregate variable
    print("ALL_STUB_OBJS = \\")
    for obj in all_embed_objs:
        print(f"\t{obj} \\")
    print("")

if __name__ == "__main__":
    generate_makefile()
