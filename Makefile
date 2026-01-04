############################################################
# LZMA + BCJ stubs (legacy minimal BCJ core + out-of-line decoder)
############################################################
## Moved below variable definitions to ensure $(BUILD_DIR) is defined at parse time

CC = gcc
CXX = g++
LD = ld
NASM = nasm

# Static build toggle (usage: make STATIC=1)
STATIC ?= 0

# Profiling build toggle (usage: make PROFILE=1)
PROFILE ?= 0

# Parallelism for stubs 
STUBS_J ?= 1

# Common flags for small tools
TOOLS_CFLAGS = -O2 -std=c99 -Wall -Wextra -Wshadow -march=x86-64 -fomit-frame-pointer -fstrict-aliasing -fno-asynchronous-unwind-tables
TOOLS_CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -Wshadow -march=x86-64 -fomit-frame-pointer -fstrict-aliasing -fno-asynchronous-unwind-tables
TOOLS_LDFLAGS =
ifeq ($(STATIC),1)
 TOOLS_LDFLAGS += -static -static-libgcc -static-libstdc++
endif

# Flags prod taille (stubs)
# Ensure 16-byte stack alignment for SSE (movaps) even if caller misaligned
STUB_PROD_CFLAGS = -fstrict-aliasing -march=x86-64 -mstackrealign -fno-plt -fno-math-errno -ffast-math -flto -I $(SRC_DIR)/filters -I $(SRC_DIR)/decompressors/lzav -I $(SRC_DIR)/decompressors/zx7b -I $(SRC_DIR)/decompressors/snappy -I $(SRC_DIR)/decompressors/quicklz -I $(SRC_DIR)/decompressors/exo -I $(SRC_DIR)/decompressors/doboz -I $(SRC_DIR)/decompressors/pp -I $(SRC_DIR)/decompressors/lzma -I $(SRC_DIR)/decompressors/zstd -I $(SRC_DIR)/decompressors/zstd/third_party/muzscat -I $(SRC_DIR)/decompressors/apultra -I $(SRC_DIR)/decompressors/lz4 -I $(SRC_DIR)/decompressors/shrinkler -I $(SRC_DIR)/decompressors/zx0 -I $(SRC_DIR)/decompressors/brieflz -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/decompressors/stonecracker -I $(SRC_DIR)/decompressors/lzsa/libs/decompression -I $(SRC_DIR)/decompressors/density -I $(SRC_DIR)/decompressors/lzham -I $(SRC_DIR)/decompressors/rnc
STUB_PROD_LDFLAGS = -Wl,--as-needed -Wl,--icf=all -Wl,--gc-sections -Wl,--relax -Wl,--build-id=none -Wl,-O1,--strip-all

# Répertoires
BUILD_DIR = build
SRC_DIR = src
STUB_SRC = $(SRC_DIR)/stub
PACKER_SRC = $(SRC_DIR)/packer
FILTERS_DIR = $(SRC_DIR)/filters
DECOMP_DIR = $(SRC_DIR)/decompressors
DECOMP_LZAV_DIR = $(DECOMP_DIR)/lzav
DECOMP_ZX7B_DIR = $(DECOMP_DIR)/zx7b
DECOMP_SNAPPY_DIR = $(DECOMP_DIR)/snappy
DECOMP_QLZ_DIR = $(DECOMP_DIR)/quicklz
DECOMP_EXO_DIR = $(DECOMP_DIR)/exo
DECOMP_DOBOZ_DIR = $(DECOMP_DIR)/doboz
DECOMP_PP_DIR = $(DECOMP_DIR)/pp
DECOMP_LZMA_DIR = $(DECOMP_DIR)/lzma
DECOMP_ZSTD_DIR = $(DECOMP_DIR)/zstd
DECOMP_APULTRA_DIR = $(DECOMP_DIR)/apultra
DECOMP_LZ4_DIR = $(DECOMP_DIR)/lz4
DECOMP_SHRINKLER_DIR = $(DECOMP_DIR)/shrinkler
DECOMP_ZX0_DIR = $(DECOMP_DIR)/zx0
DECOMP_BRIEFLZ_DIR = $(DECOMP_DIR)/brieflz
DECOMP_LZHAM_DIR = $(DECOMP_DIR)/lzham

LZSA_SRC = \
	$(SRC_DIR)/compressors/lzsa/libs/compression/lzsa2_compress.c \
	$(SRC_DIR)/compressors/lzsa/compressor/shrink_inmem.c \
	$(SRC_DIR)/compressors/lzsa/compressor/shrink_block_v1.c \
	$(SRC_DIR)/compressors/lzsa/compressor/shrink_block_v2.c \
	$(SRC_DIR)/compressors/lzsa/compressor/shrink_context.c \
	$(SRC_DIR)/compressors/lzsa/compressor/matchfinder.c \
	$(SRC_DIR)/compressors/lzsa/compressor/frame.c \
	$(SRC_DIR)/compressors/lzsa/compressor/libdivsufsort/lib/divsufsort.c \
	$(SRC_DIR)/compressors/lzsa/compressor/libdivsufsort/lib/divsufsort_utils.c \
	$(SRC_DIR)/compressors/lzsa/compressor/libdivsufsort/lib/sssort.c \
	$(SRC_DIR)/compressors/lzsa/compressor/libdivsufsort/lib/trsort.c

LZHAM_VENDOR_SRC = \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamcomp/lzham_lzbase.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamcomp/lzham_lzcomp.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamcomp/lzham_lzcomp_internal.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamcomp/lzham_lzcomp_state.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamcomp/lzham_match_accel.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamcomp/lzham_pthreads_threading.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_assert.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_checksum.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_huffman_codes.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_lzdecomp.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_lzdecompbase.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_mem.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_platform.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_prefix_coding.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_symbol_codec.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_timer.cpp \
	$(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp/lzham_vector.cpp

# Fichiers
EXO_SRC = \
	$(SRC_DIR)/compressors/exo/buf.c \
	$(SRC_DIR)/compressors/exo/buf_io.c \
	$(SRC_DIR)/compressors/exo/log.c \
	$(SRC_DIR)/compressors/exo/vec.c \
	$(SRC_DIR)/compressors/exo/match.c \
	$(SRC_DIR)/compressors/exo/search.c \
	$(SRC_DIR)/compressors/exo/optimal.c \
	$(SRC_DIR)/compressors/exo/output.c \
	$(SRC_DIR)/compressors/exo/exo_util.c \
	$(SRC_DIR)/compressors/exo/exo_helper.c \
	$(SRC_DIR)/compressors/exo/chunkpool.c \
	$(SRC_DIR)/compressors/exo/progress.c \
	$(SRC_DIR)/compressors/exo/radix.c \
	$(SRC_DIR)/compressors/exo/getflag.c \
	$(SRC_DIR)/compressors/exo/exo_bridge.c \
	$(SRC_DIR)/compressors/exo/exodec.c

CSC_COMP_SRC = \
	$(SRC_DIR)/compressors/csc/csc_compressor.cpp \
	$(SRC_DIR)/compressors/csc/csc_enc.cpp \
	$(SRC_DIR)/compressors/csc/csc_coder.cpp \
	$(SRC_DIR)/compressors/csc/csc_filters.cpp \
	$(SRC_DIR)/compressors/csc/csc_lz.cpp \
	$(SRC_DIR)/compressors/csc/csc_memio.cpp \
	$(SRC_DIR)/compressors/csc/csc_mf.cpp \
	$(SRC_DIR)/compressors/csc/csc_model.cpp \
	$(SRC_DIR)/compressors/csc/csc_profiler.cpp \
	$(SRC_DIR)/compressors/csc/csc_analyzer.cpp \
	$(SRC_DIR)/compressors/csc/csc_default_alloc.cpp \
	$(SRC_DIR)/compressors/csc/csc_encoder_main.cpp

CSC_COMP_OBJS = $(patsubst $(SRC_DIR)/compressors/csc/%.cpp,$(BUILD_DIR)/csc_%.o,$(CSC_COMP_SRC))

PACKER_C = $(PACKER_SRC)/zelf_packer.c \
	$(PACKER_SRC)/elf_utils.c \
	$(PACKER_SRC)/stub_selection.c \
	$(PACKER_SRC)/filter_selection.c \
	$(PACKER_SRC)/compression.c \
	$(PACKER_SRC)/elf_builder.c \
	$(PACKER_SRC)/archive_mode.c \
	$(PACKER_SRC)/depacker.c \
	$(SRC_DIR)/compressors/rnc/rnc_compress_wrapper.c \
	$(SRC_DIR)/decompressors/csc/csc_dec.c \
	$(SRC_DIR)/decompressors/csc/csc_filters_dec.c \
	$(SRC_DIR)/decompressors/rnc/rnc_minidec.c \
	$(SRC_DIR)/decompressors/nz1/nz1_decompressor.c \
	$(SRC_DIR)/decompressors/lzfse/lzfse_decode_base.c \
	$(SRC_DIR)/decompressors/lzfse/lzfse_decompress.c \
	$(SRC_DIR)/decompressors/lzfse/lzfse_fse_stub.c \
	$(SRC_DIR)/decompressors/lzfse/lzvn_decode_base.c \
	$(SRC_DIR)/decompressors/lz4/codec_lz4.c \
	$(SRC_DIR)/decompressors/apultra/apultra_decompress.c \
	$(SRC_DIR)/decompressors/zx7b/zx7b_decompress.c \
	$(SRC_DIR)/decompressors/zx0/zx0_decompress.c \
	$(SRC_DIR)/decompressors/brieflz/brieflz_decompress.c \
	$(SRC_DIR)/decompressors/brieflz/depack.c \
	$(SRC_DIR)/decompressors/exo/exo_decompress.c \
	$(SRC_DIR)/decompressors/pp/powerpacker_decompress.c \
	$(SRC_DIR)/decompressors/snappy/snappy_decompress.c \
	$(SRC_DIR)/decompressors/doboz/doboz_decompress.c \
	$(SRC_DIR)/decompressors/lzav/lzav_decompress.c \
	$(SRC_DIR)/decompressors/lzma/minlzma_decompress.c \
	$(SRC_DIR)/decompressors/lzma/minlzma_core.c \
	$(SRC_DIR)/decompressors/shrinkler/shrinkler_decompress.c \
	$(SRC_DIR)/decompressors/stonecracker/sc_decompress.c \
	$(SRC_DIR)/decompressors/lzsa/libs/decompression/lzsa2_decompress.c \
	$(SRC_DIR)/decompressors/lzsa/decompressor/expand_inmem.c \
	$(SRC_DIR)/decompressors/lzsa/decompressor/expand_block_v1.c \
	$(SRC_DIR)/decompressors/lzsa/decompressor/expand_block_v2.c \
	$(SRC_DIR)/decompressors/lzsa/decompressor/expand_context.c \
	$(SRC_DIR)/compressors/lz4/lz4.c \
	$(SRC_DIR)/compressors/lz4/lz4hc.c \
	$(SRC_DIR)/compressors/zx7b/zx7b_lib.c \
	$(SRC_DIR)/compressors/zx7b/zx7b_compress.c \
	$(SRC_DIR)/compressors/snappy/snappy_compress.c \
	$(SRC_DIR)/compressors/doboz/doboz_compress_wrapper.cpp \
	$(SRC_DIR)/compressors/doboz/Compressor.cpp \
	$(SRC_DIR)/compressors/doboz/Dictionary.cpp \
	$(SRC_DIR)/compressors/lzav/lzav_compress.c \
	$(SRC_DIR)/compressors/nz1/nz1_compressor.c \
	$(SRC_DIR)/compressors/apultra/shrink.c \
	$(SRC_DIR)/compressors/apultra/matchfinder.c \
	$(SRC_DIR)/compressors/apultra/libdivsufsort/lib/divsufsort.c \
	$(SRC_DIR)/compressors/apultra/libdivsufsort/lib/divsufsort_utils.c \
	$(SRC_DIR)/compressors/apultra/libdivsufsort/lib/sssort.c \
	$(SRC_DIR)/compressors/apultra/libdivsufsort/lib/trsort.c \
	$(SRC_DIR)/compressors/qlz/quicklz.c \
	$(SRC_DIR)/compressors/pp/pp_compress.c \
    $(SRC_DIR)/compressors/pp/pp_encoder.c \
	$(SRC_DIR)/compressors/pp/pp_ref_encoder.c \
	$(EXO_SRC) \
	$(LZSA_SRC) \
	$(LZHAM_VENDOR_SRC) \
	$(SRC_DIR)/compressors/lzma/C/LzmaLib.c \
	$(SRC_DIR)/compressors/lzma/C/LzmaEnc.c \
	$(SRC_DIR)/compressors/lzma/C/LzmaDec.c \
	$(SRC_DIR)/compressors/lzma/C/LzFind.c \
	$(SRC_DIR)/compressors/lzma/C/Alloc.c \
	$(SRC_DIR)/compressors/lzma/C/7zCrc.c \
	$(SRC_DIR)/compressors/lzma/C/7zCrcOpt.c \
	$(SRC_DIR)/compressors/lzma/C/CpuArch.c \
	$(SRC_DIR)/filters/kanzi_exe_encode_c.cpp \
	$(SRC_DIR)/filters/kanzi_exe_encode.cpp \
	$(SRC_DIR)/compressors/zstd/zstd-in.c \
    $(SRC_DIR)/compressors/zx0/zx0_compress_wrapper.c \
    $(SRC_DIR)/compressors/zx0/compress.c \
    $(SRC_DIR)/compressors/zx0/optimize.c \
    $(SRC_DIR)/compressors/zx0/memory.c \
    $(SRC_DIR)/compressors/lzfse/lzfse_compress.c \
    $(SRC_DIR)/compressors/lzfse/lzfse_encode_base.c \
    $(SRC_DIR)/compressors/lzfse/lzfse_fse.c \
    $(SRC_DIR)/compressors/lzfse/lzvn_encode_base.c \
    $(SRC_DIR)/compressors/brieflz/blz_compress_wrapper.c \
    $(SRC_DIR)/compressors/brieflz/brieflz.c \
    $(SRC_DIR)/compressors/stonecracker/sc_compress.c \
    $(SRC_DIR)/compressors/stonecracker/bitstream.c \
    $(SRC_DIR)/filters/bcj_x86_enc.c \
    $(SRC_DIR)/others/help_display.c \
    $(SRC_DIR)/others/table_display.c \
    $(SRC_DIR)/compressors/density/density_compress_wrapper.c \
    $(SRC_DIR)/compressors/density/buffer.c \
    $(SRC_DIR)/compressors/density/stream.c \
    $(SRC_DIR)/compressors/density/memory_teleport.c \
    $(SRC_DIR)/compressors/density/memory_location.c \
    $(SRC_DIR)/compressors/density/main_encode.c \
    $(SRC_DIR)/compressors/density/block_encode.c \
    $(SRC_DIR)/compressors/density/kernel_lion_encode.c \
    $(SRC_DIR)/compressors/density/kernel_lion_dictionary.c \
    $(SRC_DIR)/compressors/density/kernel_lion_form_model.c \
    $(SRC_DIR)/compressors/density/main_header.c \
    $(SRC_DIR)/compressors/density/main_footer.c \
    $(SRC_DIR)/compressors/density/block_header.c \
    $(SRC_DIR)/compressors/density/block_footer.c \
    $(SRC_DIR)/compressors/density/block_mode_marker.c \
    $(SRC_DIR)/compressors/density/globals.c \
    $(SRC_DIR)/decompressors/density/main_decode.c \
    $(SRC_DIR)/decompressors/density/block_decode.c \
    $(SRC_DIR)/decompressors/density/kernel_lion_decode.c \
    $(SRC_DIR)/decompressors/density/density_minidec.c \
    $(SRC_DIR)/compressors/lzham/lzham_compressor.c \
    $(SRC_DIR)/compressors/lzham/lzham_comp_shim.cpp \
    $(SRC_DIR)/decompressors/lzham/lzhamd_decompress.c \
    $(SRC_DIR)/decompressors/lzham/lzhamd_huffman.c \
    $(SRC_DIR)/decompressors/lzham/lzhamd_models.c \
    $(SRC_DIR)/decompressors/lzham/lzhamd_symbol_codec.c \
    $(SRC_DIR)/decompressors/lzham/lzhamd_tables.c

SHRINKLER_COMP_SRC = $(SRC_DIR)/compressors/shrinkler/shrinkler_comp.cpp
SHRINKLER_COMP_OBJ = $(BUILD_DIR)/shrinkler_comp_fast.o

ifeq ($(DEBUG),1)
 SHRINKLER_COMP_CXXFLAGS = $(PACKER_CFLAGS) -std=c++17
else
 SHRINKLER_COMP_CXXFLAGS = $(PACKER_CFLAGS) -O3 -std=c++17
endif

PACKER_BIN = $(BUILD_DIR)/zelf
PACKED_BIN = $(BUILD_DIR)/packed_binary
STUB_BINS_DIR = $(BUILD_DIR)
STAGE0_ELF = $(BUILD_DIR)/stage0_nrv2b_le32.elf
STAGE0_BIN = $(BUILD_DIR)/stage0_nrv2b_le32.bin
STAGE0_OFFSETS = $(BUILD_DIR)/stage0_nrv2b_le32_offsets.h
STAGE0_EMBED_O = $(BUILD_DIR)/stage0_nrv2b_le32_bin.o
BUILD_INFO_H = $(BUILD_DIR)/build_info.h

TOOLS_DIR = $(SRC_DIR)/tools
TOOLS_BIN_DIR = $(BUILD_DIR)/tools
TOOLS_BINARIES = $(TOOLS_BIN_DIR)/elfz_probe $(TOOLS_BIN_DIR)/filtered_probe

# Vendored UCL (NRV2B-99 compressor) is used only by the packer (hosted tool).
# UCL is sensitive to strict-aliasing with aggressive optimization; compile it
# with -fno-strict-aliasing to keep its runtime self-checks (ucl_init) valid.
UCL_VENDOR_CFLAGS = $(PACKER_CFLAGS) -fno-strict-aliasing -fno-lto \
	-DSIZEOF_SIZE_T=8 -DSIZEOF_CHAR_P=8 -DSIZEOF_PTRDIFF_T=8 \
	-DSIZE_T_MAX=18446744073709551615ULL
UCL_VENDOR_SRC = \
	$(SRC_DIR)/tools/ucl/vendor/src/ucl_init.c \
	$(SRC_DIR)/tools/ucl/vendor/src/ucl_util.c \
	$(SRC_DIR)/tools/ucl/vendor/src/ucl_ptr.c \
	$(SRC_DIR)/tools/ucl/vendor/src/ucl_crc.c \
	$(SRC_DIR)/tools/ucl/vendor/src/ucl_str.c \
	$(SRC_DIR)/tools/ucl/vendor/src/alloc.c \
	$(SRC_DIR)/tools/ucl/vendor/src/io.c \
	$(SRC_DIR)/tools/ucl/vendor/src/n2b_99.c
UCL_VENDOR_OBJS = \
	$(BUILD_DIR)/ucl_init.o \
	$(BUILD_DIR)/ucl_util.o \
	$(BUILD_DIR)/ucl_ptr.o \
	$(BUILD_DIR)/ucl_crc.o \
	$(BUILD_DIR)/ucl_str.o \
	$(BUILD_DIR)/ucl_alloc.o \
	$(BUILD_DIR)/ucl_io.o \
	$(BUILD_DIR)/ucl_n2b_99.o

ifeq ($(DEBUG),1)
 STUB_CFLAGS = -c -O0 -g -DDEBUG=1 -nostdlib -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fno-builtin
 PACKER_CFLAGS = -O0 -D_7ZIP_ST -DQLZ_COMPRESSION_LEVEL=3 -DQLZ_STREAMING_BUFFER=0 -I $(SRC_DIR)/compressors/apultra -I $(SRC_DIR)/compressors/apultra/libdivsufsort/include -I $(SRC_DIR)/compressors/lzma/C -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/zstd/lib -I $(SRC_DIR)/compressors/zstd/lib/common -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/qlz -I $(SRC_DIR)/compressors/pp -I $(SRC_DIR)/compressors/lzav -I $(SRC_DIR)/compressors/snappy -I $(SRC_DIR)/compressors/zx7b -I $(SRC_DIR)/compressors/zx0 -I $(SRC_DIR)/compressors/doboz -I $(SRC_DIR)/compressors/exo -I $(SRC_DIR)/compressors/shrinkler -I $(SRC_DIR)/compressors/shrinkler/ref -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/compressors/stonecracker -I $(SRC_DIR)/compressors/density -I $(SRC_DIR)/decompressors/density -I $(SRC_DIR)/others -I $(SRC_DIR)/filters -I $(SRC_DIR)/compressors/lzham -I $(SRC_DIR)/compressors/lzham/vendor/lzhamcomp -I $(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp
else
 STUB_CFLAGS = -c -O2 -nostdlib -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-builtin
 # Aggressive size optimization for packer binary
PACKER_CFLAGS = -Os -flto=9 -fuse-ld=gold -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fomit-frame-pointer -ffunction-sections -fdata-sections -fvisibility=hidden -fno-plt -fmerge-all-constants -fno-ident -fno-exceptions \
 	-D_7ZIP_ST -DQLZ_COMPRESSION_LEVEL=3 -DQLZ_STREAMING_BUFFER=0 \
 	-I $(SRC_DIR)/compressors/apultra -I $(SRC_DIR)/compressors/apultra/libdivsufsort/include -I $(SRC_DIR)/compressors/lzma/C -I $(SRC_DIR)/compressors/zstd/lib -I $(SRC_DIR)/compressors/zstd/lib/common -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/qlz -I $(SRC_DIR)/compressors/pp -I $(SRC_DIR)/compressors/lzav -I $(SRC_DIR)/compressors/snappy -I $(SRC_DIR)/compressors/zx7b -I $(SRC_DIR)/compressors/zx0 -I $(SRC_DIR)/compressors/doboz -I $(SRC_DIR)/compressors/exo -I $(SRC_DIR)/compressors/shrinkler -I $(SRC_DIR)/compressors/shrinkler/ref -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/compressors/stonecracker -I $(SRC_DIR)/compressors/lzsa/libs/compression -I $(SRC_DIR)/compressors/lzsa/compressor -I $(SRC_DIR)/compressors/lzsa/compressor/libdivsufsort/include -I $(SRC_DIR)/compressors/density -I $(SRC_DIR)/decompressors/density -I $(SRC_DIR)/others -I $(SRC_DIR)/filters -I $(SRC_DIR)/compressors/lzham -I $(SRC_DIR)/compressors/lzham/vendor/lzhamcomp -I $(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp \
	-I $(SRC_DIR)/compressors/rnc -I $(SRC_DIR)/decompressors/zstd -I $(SRC_DIR)/tools/ucl/vendor/include -I $(SRC_DIR)/tools/ucl/vendor/src
endif
 # Linker flags tuned for size (keep functionality)
 PACKER_LDFLAGS = -lm -lstdc++ \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,--as-needed -Wl,-O1,--icf=all,--compress-debug-sections=zlib -Wl,--relax -Wl,--strip-all -s

 ifeq ($(PROFILE),1)
  PACKER_CFLAGS := $(filter-out -flto=% -fomit-frame-pointer,$(PACKER_CFLAGS))
  PACKER_CFLAGS += -g -fno-omit-frame-pointer -fno-optimize-sibling-calls
  # Use explicit linker flags in profile builds to avoid accidental stripping
  # or malformed option lists after filtering.
  PACKER_LDFLAGS = -lm -lstdc++ \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,--as-needed \
	-Wl,-O1,--icf=all,--compress-debug-sections=zlib -Wl,--relax
 endif

# If requested, build packer and selected tools fully static
ifeq ($(STATIC),1)
 PACKER_LDFLAGS += -static -static-libgcc -static-libstdc++
 TOOLS_LDFLAGS += -static -static-libgcc -static-libstdc++
endif

# Cible par défaut
.PHONY: all static packer
all: stubs tools packer
	@printf "\033[37;43m✔ Compilation terminée\033[0m\n"

# Convenience alias to build static packer/tools/zelf
static:
	$(MAKE) STATIC=1 all
	@printf "\033[37;43m✔ Compilation terminée\033[0m\n"

# Compiler le stub
$(BUILD_DIR)/start.o: $(STUB_SRC)/start.S | $(BUILD_DIR)
	@printf "\033[37;44m░▒▓ Compiling start.S...\033[0m\n"
	@$(CC) -c $(STUB_SRC)/start.S -o $(BUILD_DIR)/start.o

$(BUILD_DIR)/start_sfx.o: $(STUB_SRC)/start_sfx.S | $(BUILD_DIR)
	@printf "\033[37;44m░▒▓ Compiling start_sfx.S...\033[0m\n"
	@$(CC) -c $(STUB_SRC)/start_sfx.S -o $(BUILD_DIR)/start_sfx.o

$(BUILD_DIR)/elfz_handoff_gate.o: $(STUB_SRC)/elfz_handoff_gate.S | $(BUILD_DIR)
	@printf "\033[37;44m░▒▓ Compiling elfz_handoff_gate.S...\033[0m\n"
	@$(CC) -c $(STUB_SRC)/elfz_handoff_gate.S -o $(BUILD_DIR)/elfz_handoff_gate.o

$(BUILD_DIR)/elfz_handoff_sequence.o: $(STUB_SRC)/elfz_handoff_sequence.S | $(BUILD_DIR)
	@printf "\033[37;44m░▒▓ Compiling elfz_handoff_sequence.S...\033[0m\n"
	@$(CC) -c $(STUB_SRC)/elfz_handoff_sequence.S -o $(BUILD_DIR)/elfz_handoff_sequence.o

# Assemble Shrinkler ASM decompressor
$(BUILD_DIR)/shrinkler_decompress_x64.o: $(DECOMP_SHRINKLER_DIR)/shrinkler_decompress_x64.asm | $(BUILD_DIR)
	@printf "\033[37;44m❯  Assembling Shrinkler x64 decompressor (NASM)...\033[0m\n"
	@$(NASM) -f elf64 -O2 -o $@ $<

$(SHRINKLER_COMP_OBJ): $(SHRINKLER_COMP_SRC) | $(BUILD_DIR)
	@printf "\033[37;44m░▒▓ Compiling Shrinkler (speed flags)...\033[0m\n"
	@$(CXX) $(SHRINKLER_COMP_CXXFLAGS) -c $< -o $@

# --- GENERATION DES STUBS ---
# Inclure le makefile généré
-include $(BUILD_DIR)/stubs.mk

# Règle pour générer stubs.mk s'il n'existe pas ou si le script change
$(BUILD_DIR)/stubs.mk: tools/gen_stubs_mk.py | $(BUILD_DIR)
	@printf "\033[37;45m⚙ Generating $(BUILD_DIR)/stubs.mk...\033[0m\n"
	@python3 tools/gen_stubs_mk.py > $@

# Cible stubs qui force la génération
.PHONY: stubs
stubs: $(BUILD_DIR)/stubs.mk
	@$(MAKE) -f Makefile actual_stubs -j$(STUBS_J)

.PHONY: actual_stubs
actual_stubs: $(ALL_STUB_OBJS)

$(BUILD_DIR)/static_final_noreturn.o: $(STUB_SRC)/static_final_noreturn.S | $(BUILD_DIR)
	@printf "\033[37;44m░▒▓ Compiling static_final_noreturn.o...\033[0m\n"
	@$(CC) -c $(STUB_SRC)/static_final_noreturn.S -o $(BUILD_DIR)/static_final_noreturn.o

$(BUILD_DIR)/zstd_minidec.o: $(SRC_DIR)/decompressors/zstd/zstd_minidec.c | $(BUILD_DIR)
	@printf "\033[37;44m░▒▓ Compiling zstd_minidec.c...\033[0m\n"
	@$(CC) -I $(SRC_DIR)/decompressors/zstd/third_party/muzscat $(PACKER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/csc_%.o: $(SRC_DIR)/compressors/csc/%.cpp | $(BUILD_DIR)
	@$(CXX) $(filter-out -fno-exceptions -flto=%,$(PACKER_CFLAGS)) -fno-lto -fexceptions -std=c++17 -D_7Z_TYPES_ -Wno-format \
		-I $(SRC_DIR)/compressors/csc -c $< -o $@

$(BUILD_DIR)/ucl_init.o: $(SRC_DIR)/tools/ucl/vendor/src/ucl_init.c | $(BUILD_DIR)
	@$(CC) $(UCL_VENDOR_CFLAGS) -c $< -o $@
$(BUILD_DIR)/ucl_util.o: $(SRC_DIR)/tools/ucl/vendor/src/ucl_util.c | $(BUILD_DIR)
	@$(CC) $(UCL_VENDOR_CFLAGS) -c $< -o $@
$(BUILD_DIR)/ucl_ptr.o: $(SRC_DIR)/tools/ucl/vendor/src/ucl_ptr.c | $(BUILD_DIR)
	@$(CC) $(UCL_VENDOR_CFLAGS) -c $< -o $@
$(BUILD_DIR)/ucl_crc.o: $(SRC_DIR)/tools/ucl/vendor/src/ucl_crc.c | $(BUILD_DIR)
	@$(CC) $(UCL_VENDOR_CFLAGS) -c $< -o $@
$(BUILD_DIR)/ucl_str.o: $(SRC_DIR)/tools/ucl/vendor/src/ucl_str.c | $(BUILD_DIR)
	@$(CC) $(UCL_VENDOR_CFLAGS) -c $< -o $@
$(BUILD_DIR)/ucl_alloc.o: $(SRC_DIR)/tools/ucl/vendor/src/alloc.c | $(BUILD_DIR)
	@$(CC) $(UCL_VENDOR_CFLAGS) -c $< -o $@
$(BUILD_DIR)/ucl_io.o: $(SRC_DIR)/tools/ucl/vendor/src/io.c | $(BUILD_DIR)
	@$(CC) $(UCL_VENDOR_CFLAGS) -c $< -o $@
$(BUILD_DIR)/ucl_n2b_99.o: $(SRC_DIR)/tools/ucl/vendor/src/n2b_99.c $(SRC_DIR)/tools/ucl/vendor/src/n2_99.ch | $(BUILD_DIR)
	@$(CC) $(UCL_VENDOR_CFLAGS) -c $< -o $@

$(STAGE0_ELF): $(STUB_SRC)/stage0_nrv2b_le32.S $(STUB_SRC)/stage0.ld $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.c $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.h | $(BUILD_DIR)
	@printf "\033[37;44m░▒▓ Building stage0 NRV2B LE32 ELF...\033[0m\n"
	@$(CC) -Os -ffreestanding -fno-plt -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -nostdlib -static \
		-I $(SRC_DIR)/tools/ucl \
		-Wl,-T,$(STUB_SRC)/stage0.ld -Wl,-e,stage0_nrv2b_entry -Wl,--build-id=none -Wl,-z,noexecstack \
		$(STUB_SRC)/stage0_nrv2b_le32.S $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.c -o $@

$(STAGE0_BIN): $(STAGE0_ELF)
	@printf "\033[37;44m❯ Objcopy stage0 -> bin...\033[0m\n"
	@objcopy -O binary -j .text -j .rodata $< $@

$(STAGE0_OFFSETS): $(STAGE0_ELF)
	@printf "\033[37;44m❯ Export stage0 header offsets...\033[0m\n"
	@nm -S $< | perl -ne 'BEGIN{$$b=$$u=$$c=$$d=$$bo=undef;} if(/stage0_nrv2b_entry/){($$a)=split; $$b=hex($$a);} if(/stage0_hdr_ulen/){($$a)=split; $$u=hex($$a);} if(/stage0_hdr_clen/){($$a)=split; $$c=hex($$a);} if(/stage0_hdr_dst/){($$a)=split; $$d=hex($$a);} if(/stage0_hdr_blob/){($$a)=split; $$bo=hex($$a);} END{die "stage0_nrv2b_entry not found\n" unless defined $$b; print "#define STAGE0_HDR_ULEN_OFF 0x", sprintf("%x",$$u-$$b), "\n"; print "#define STAGE0_HDR_CLEN_OFF 0x", sprintf("%x",$$c-$$b), "\n"; print "#define STAGE0_HDR_DST_OFF  0x", sprintf("%x",$$d-$$b), "\n"; print "#define STAGE0_HDR_BLOB_OFF 0x", sprintf("%x",$$bo-$$b), "\n";}' > $@

$(STAGE0_EMBED_O): $(STAGE0_BIN) | $(BUILD_DIR)
	@printf "\033[37;44m→→ Embedding stage0 bin...\033[0m\n"
	@ld -r -b binary $< -o $@


$(PACKER_BIN): $(BUILD_INFO_H) $(ALL_STUB_OBJS) $(PACKER_C) $(CSC_COMP_OBJS) $(SHRINKLER_COMP_OBJ) $(BUILD_DIR)/zstd_minidec.o $(BUILD_DIR)/shrinkler_decompress_x64.o $(STAGE0_EMBED_O) $(STAGE0_OFFSETS) $(UCL_VENDOR_OBJS) | $(BUILD_DIR)
	@$(CC) $(PACKER_CFLAGS) -I $(BUILD_DIR) $(PACKER_C) $(CSC_COMP_OBJS) $(SHRINKLER_COMP_OBJ) $(ALL_STUB_OBJS) $(BUILD_DIR)/zstd_minidec.o $(BUILD_DIR)/shrinkler_decompress_x64.o $(STAGE0_EMBED_O) \
		$(UCL_VENDOR_OBJS) \
		-o $(PACKER_BIN) $(PACKER_LDFLAGS)
	@if [ "$(PROFILE)" != "1" ]; then \
	  command -v sstrip >/dev/null 2>&1 && sstrip $(PACKER_BIN) || true; \
	  printf "\033[37;44m❯ Post-build: purge .o et symlinks dans $(BUILD_DIR)\033[0m\n"; \
	  rm -f $(BUILD_DIR)/*.o; \
	  rm -f $(BUILD_DIR)/*.elf; \
	  rm -f $(BUILD_DIR)/stub_*_*.bin; \
	  rm -f $(BUILD_DIR)/stubs.mk; \
	  rm -f $(BUILD_DIR)/stage0_nrv2b_le32*; \
	fi

$(BUILD_INFO_H): | $(BUILD_DIR)
	@dt=$$(date '+%Y-%m-%d %H:%M:%S'); \
	 os=$$(uname -sr 2>/dev/null || echo unknown); \
	 cpu=$$(uname -m 2>/dev/null || echo unknown); \
	 model=$$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2- | sed 's/^ *//'); \
	 if [ -n "$$model" ]; then cpu="$$cpu / $$model"; fi; \
	 if [ "$(STATIC)" = "1" ]; then link=Static; else link=Dynamic; fi; \
	 { \
		echo '#ifndef ZELF_BUILD_INFO_H'; \
		echo '#define ZELF_BUILD_INFO_H'; \
		echo '#define ZELF_BUILD_DATETIME "'"$$dt"'"'; \
		echo '#define ZELF_BUILD_HOST_OS "'"$$os"'"'; \
		echo '#define ZELF_BUILD_HOST_CPU "'"$$cpu"'"'; \
		echo '#define ZELF_BUILD_LINKAGE "'"$$link"'"'; \
		echo '#endif'; \
	 } > $@.tmp; \
	 mv -f $@.tmp $@

# Build only the packer.
packer: stubs tools | $(BUILD_DIR)
	@printf "\033[37;45m░▒▓ Building packer binary...\033[0m\n"
	@$(MAKE) $(PACKER_BIN)

# Build the Kanzi .text benchmark tool
.PHONY: kanzi_text_bench
kanzi_text_bench: $(BUILD_DIR) $(FILTERS_DIR)/kanzi_exe_encode.cpp $(FILTERS_DIR)/kanzi_exe_encode_c.cpp $(SRC_DIR)/tools/kanzi_text_bench.c
	@printf "\033[37;44m░▒▓ Building kanzi_text_bench...\033[0m\n"
	@$(CC) -O2 -DNDEBUG \
		-I $(SRC_DIR)/filters \
		-I $(SRC_DIR)/compressors/lz4 \
		-I $(SRC_DIR)/compressors/zstd/lib -I $(SRC_DIR)/compressors/zstd/lib/common \
		-I $(SRC_DIR)/compressors/lzav \
		-I $(SRC_DIR)/others \
		$(FILTERS_DIR)/kanzi_exe_encode_c.cpp $(FILTERS_DIR)/kanzi_exe_encode.cpp \
		$(SRC_DIR)/compressors/lz4/lz4.c $(SRC_DIR)/compressors/lz4/lz4hc.c \
		$(SRC_DIR)/compressors/zstd/zstd-in.c \
		$(SRC_DIR)/filters/bcj_x86_enc.c \
		$(SRC_DIR)/compressors/lzav/lzav_compress.c \
		$(SRC_DIR)/tools/kanzi_text_bench.c -o $(BUILD_DIR)/kanzi_text_bench -lm -lstdc++

clean:
	@printf "\033[37;44m❯ Cleaning...\033[0m\n"
	rm -f $(BUILD_DIR)/start.o $(BUILD_DIR)/elfz_handoff_gate.o $(BUILD_DIR)/elfz_handoff_sequence.o $(BUILD_DIR)/upx_final.o $(BUILD_DIR)/upx_final_sequence.o
	rm -f $(BUILD_DIR)/stubs.mk
	rm -f $(BUILD_DIR)/*.elf
	rm -f $(BUILD_DIR)/*.o
	rm -f $(BUILD_DIR)/stubs/*.bin $(BUILD_DIR)/stub_*_*.bin
	rm -f $(BUILD_DIR)/embed_stub_*.o
	rm -f $(BUILD_DIR)/stage0_nrv2b_le32*.*
	rm -f $(STUB_SRC)/stub_vars.h.bak
	rm -f $(BUILD_DIR)/zelf

.PHONY: distclean
distclean: clean
	@printf "\033[37;43m✔ Cleaning complete\033[0m\n"

# Créer les répertoires si nécessaire
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/stubs: | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/stubs


# Interactive menu target (runs tools/make_menu.sh)
.PHONY: menu
menu:
	@bash tools/make_menu.sh

.PHONY: tools
tools: $(TOOLS_BINARIES)
	@printf "\033[37;43m✔ Tools built\033[0m\n"

FORCE:

$(TOOLS_BINARIES): FORCE

$(TOOLS_BIN_DIR):
	mkdir -p $(TOOLS_BIN_DIR)

$(TOOLS_BIN_DIR)/elfz_probe: $(TOOLS_DIR)/elfz_probe.c | $(TOOLS_BIN_DIR)
	@$(CC) $(TOOLS_CFLAGS) $(TOOLS_LDFLAGS) $< -lm -o $@

$(TOOLS_BIN_DIR)/filtered_probe: $(TOOLS_DIR)/filtered_probe.c | $(TOOLS_BIN_DIR)
	@$(CXX) $(TOOLS_CXXFLAGS) \
		-I $(SRC_DIR)/filters -I $(SRC_DIR)/compressors/lz4 \
		$(TOOLS_DIR)/filtered_probe.c \
		$(SRC_DIR)/filters/bcj_x86_enc.c \
		$(SRC_DIR)/filters/kanzi_exe_encode_c.cpp \
		$(SRC_DIR)/filters/kanzi_exe_encode.cpp \
		$(SRC_DIR)/compressors/lz4/lz4.c \
		$(SRC_DIR)/compressors/lz4/lz4hc.c \
		$(TOOLS_LDFLAGS) -lm -o $@

.PHONY: deb
deb: packer
	@STATIC=$(STATIC) bash tools/create_deb.sh

.PHONY: tar
tar: packer
	@STATIC=$(STATIC) bash tools/create_tar.sh

.PHONY: install_dependencies
install_dependencies:
	@bash tools/install_dependencies.sh
