############################################################
# LZMA + BCJ stubs (legacy minimal BCJ core + out-of-line decoder)
############################################################
## Moved below variable definitions to ensure $(BUILD_DIR) is defined at parse time



CC = gcc
CXX = g++
LD = ld
NASM = nasm

FUSE_LD ?=
ifeq ($(strip $(FUSE_LD)),)
 FUSE_LD := $(shell if command -v ld.lld >/dev/null 2>&1; then echo lld; elif command -v ld.bfd >/dev/null 2>&1; then echo bfd; else echo default; fi)
endif
ifeq ($(FUSE_LD),bfd)
 FUSE_LD_FLAG = -fuse-ld=bfd
else ifeq ($(FUSE_LD),lld)
 FUSE_LD_FLAG = -fuse-ld=lld
else
 FUSE_LD_FLAG =
endif

ifeq ($(FUSE_LD),lld)
 STUB_LTO_FLAG =
 PACKER_LTO_FLAG =
else
 STUB_LTO_FLAG = -flto
 PACKER_LTO_FLAG = -flto=9
endif

ifeq ($(FUSE_LD),lld)
 ICF_LDFLAG = -Wl,--icf=all
else
 ICF_LDFLAG =
endif

# Static build toggle (usage: make STATIC=1)
STATIC ?= 0

# Profiling build toggle (usage: make PROFILE=1)
PROFILE ?= 0

# Parallelism for stubs 
STUBS_J ?= 1

PYTHON ?= python3

HOST_UNAME_M ?= $(shell uname -m 2>/dev/null || echo unknown)
ifeq ($(HOST_UNAME_M),x86_64)
 TOOLS_MARCH_CFLAG := -march=x86-64
else ifeq ($(HOST_UNAME_M),aarch64)
 TOOLS_MARCH_CFLAG := -march=armv8-a
else
 TOOLS_MARCH_CFLAG :=
endif

# Common flags for small tools
TOOLS_CFLAGS = -O2 -std=c99 -Wall -Wextra -Wshadow $(TOOLS_MARCH_CFLAG) -fomit-frame-pointer -fstrict-aliasing -fno-asynchronous-unwind-tables
TOOLS_CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -Wshadow $(TOOLS_MARCH_CFLAG) -fomit-frame-pointer -fstrict-aliasing -fno-asynchronous-unwind-tables
TOOLS_LDFLAGS =
ifeq ($(STATIC),1)
 TOOLS_LDFLAGS += -static -static-libgcc -static-libstdc++
endif

# Stub PIE policy:
# Some toolchains (observed with GCC 15+) may emit absolute-addressing sequences
# in stub code when not compiling as PIE. Since stubs are embedded as raw binary
# blobs (objcopy -O binary), relocations are not available at runtime and this
# can crash (SIGSEGV) when mapped at a randomized base.
#
# Override:
#   make STUB_FORCE_PIE=0   -> force disabled
#   make STUB_FORCE_PIE=1   -> force enabled
STUB_FORCE_PIE ?= auto
GCC_MAJOR := $(shell $(CC) -dumpversion 2>/dev/null | sed 's/\..*//' )

ifeq ($(STUB_FORCE_PIE),1)
 STUB_PIE_CFLAGS := -fpie
else ifeq ($(STUB_FORCE_PIE),0)
 STUB_PIE_CFLAGS :=
else
 # auto
 ifneq ($(GCC_MAJOR),)
  ifneq ($(shell [ $(GCC_MAJOR) -ge 15 ] 2>/dev/null && echo 1),)
   STUB_PIE_CFLAGS := -fpie
  else
   STUB_PIE_CFLAGS :=
  endif
 else
  STUB_PIE_CFLAGS :=
 endif
endif

 # Flags prod taille (stubs)
 # Ensure 16-byte stack alignment for SSE (movaps) even if caller misaligned
 ifeq ($(HOST_UNAME_M),x86_64)
  STUB_PROD_ARCH_CFLAGS = -march=x86-64 -mstackrealign
 else ifeq ($(HOST_UNAME_M),aarch64)
  STUB_PROD_ARCH_CFLAGS = -march=armv8-a
 else
  STUB_PROD_ARCH_CFLAGS =
 endif
 STUB_PROD_CFLAGS_BASE = $(STUB_PIE_CFLAGS) -fstrict-aliasing $(STUB_PROD_ARCH_CFLAGS) -fno-plt -fno-math-errno -ffast-math -I $(SRC_DIR)/filters -I $(SRC_DIR)/decompressors/lzav -I $(SRC_DIR)/decompressors/zx7b -I $(SRC_DIR)/decompressors/snappy -I $(SRC_DIR)/decompressors/quicklz -I $(SRC_DIR)/decompressors/exo -I $(SRC_DIR)/decompressors/doboz -I $(SRC_DIR)/decompressors/pp -I $(SRC_DIR)/decompressors/lzma -I $(SRC_DIR)/decompressors/zstd -I $(SRC_DIR)/decompressors/zstd/third_party/muzscat -I $(SRC_DIR)/decompressors/apultra -I $(SRC_DIR)/decompressors/lz4 -I $(SRC_DIR)/decompressors/shrinkler -I $(SRC_DIR)/decompressors/zx0 -I $(SRC_DIR)/decompressors/brieflz -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/decompressors/stonecracker -I $(SRC_DIR)/decompressors/lzsa/libs/decompression -I $(SRC_DIR)/decompressors/lzsa/decompressor -I $(SRC_DIR)/decompressors/density -I $(SRC_DIR)/decompressors/lzham -I $(SRC_DIR)/decompressors/rnc
 ifeq ($(HOST_UNAME_M),aarch64)
  STUB_PROD_CFLAGS = $(filter-out -fno-plt,$(STUB_PROD_CFLAGS_BASE))
 else
  STUB_PROD_CFLAGS = $(STUB_PROD_CFLAGS_BASE)
 endif
 STUB_PROD_LDFLAGS = -Wl,--as-needed $(ICF_LDFLAG) -Wl,--gc-sections -Wl,--relax -Wl,--build-id=none -Wl,-O1,--strip-all
 
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

 # Host/target architecture split (stubs are target-specific assets)
HOST_ARCH ?= $(shell uname -m 2>/dev/null || echo unknown)
ifeq ($(HOST_ARCH),x86_64)
 HOST_ARCH := x86_64
else ifeq ($(HOST_ARCH),aarch64)
 HOST_ARCH := arm64
else
 HOST_ARCH := $(HOST_ARCH)
endif

STUBS_X86_DIR = $(BUILD_DIR)/stubs_x86_64
STUBS_ARM64_DIR = $(BUILD_DIR)/stubs_arm64
STUBS_HOST_DIR = $(BUILD_DIR)/stubs_$(HOST_ARCH)

BUILD_ARCH_STAMP = $(BUILD_DIR)/.host_arch

ifeq ($(HOST_ARCH),x86_64)
 STUB_SRC = $(SRC_DIR)/stub
 STUB_BCJ_MIN_SRC = $(SRC_DIR)/filters/bcj_x86_min.c
 STUB_LINK_OBJS = $(BUILD_DIR)/start.o $(BUILD_DIR)/elfz_handoff_gate.o $(BUILD_DIR)/elfz_handoff_sequence.o
 STUB_ENABLE_SFX = 1
else ifeq ($(HOST_ARCH),arm64)
 STUB_SRC = $(SRC_DIR)/stubs_arm64
 STUB_BCJ_MIN_SRC = $(SRC_DIR)/filters/bcj_arm64_min.c
 STUB_LINK_OBJS = $(BUILD_DIR)/start.o $(BUILD_DIR)/elfz_handoff_sequence.o
 STUB_ENABLE_SFX = 1
else
 STUB_SRC = $(SRC_DIR)/stub
 STUB_BCJ_MIN_SRC = $(SRC_DIR)/filters/bcj_x86_min.c
 STUB_LINK_OBJS = $(BUILD_DIR)/start.o $(BUILD_DIR)/elfz_handoff_gate.o $(BUILD_DIR)/elfz_handoff_sequence.o
 STUB_ENABLE_SFX = 0
endif

ifeq ($(HOST_ARCH),arm64)
 STUB_PROD_CFLAGS += -DELFZ_STUB_ARM64
endif

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

 ifeq ($(HOST_ARCH),x86_64)
 SHRINKLER_DECOMP_SRC = $(SRC_DIR)/decompressors/shrinkler/shrinkler_decompress.c
 SHRINKLER_ASM_OBJ = $(BUILD_DIR)/shrinkler_decompress_x64.o
 SHRINKLER_STUB_ASM_OBJ = $(BUILD_DIR)/shrinkler_decompress_x64.o
 else ifeq ($(HOST_ARCH),arm64)
 SHRINKLER_DECOMP_SRC = $(SRC_DIR)/decompressors/shrinkler/shrinkler_decompress.c
 SHRINKLER_ASM_OBJ = $(BUILD_DIR)/shrinkler_decompress_arm64.o
 SHRINKLER_STUB_ASM_OBJ = $(BUILD_DIR)/shrinkler_decompress_arm64.o
 else
 SHRINKLER_DECOMP_SRC = $(SRC_DIR)/decompressors/shrinkler/codec_shrinkler.c
 SHRINKLER_ASM_OBJ =
 SHRINKLER_STUB_ASM_OBJ =
 endif

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
	$(SHRINKLER_DECOMP_SRC) \
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
	$(SRC_DIR)/filters/kanzi_exe_encode_arm64_c.cpp \
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
    $(SRC_DIR)/filters/bcj_arm64_enc.c \
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
STAGE0_X86_DIR = $(BUILD_DIR)/stage0_x86_64
STAGE0_ARM64_DIR = $(BUILD_DIR)/stage0_arm64
STAGE0_X86_ELF = $(STAGE0_X86_DIR)/stage0_nrv2b_le32_x86_64.elf
STAGE0_X86_BIN = $(STAGE0_X86_DIR)/stage0_nrv2b_le32_x86_64.bin
STAGE0_X86_OFFSETS = $(STAGE0_X86_DIR)/stage0_nrv2b_le32_offsets_x86_64.h
STAGE0_X86_EMBED_O = $(BUILD_DIR)/stage0_nrv2b_le32_x86_64_bin.o
STAGE0_ARM64_ELF = $(STAGE0_ARM64_DIR)/stage0_nrv2b_le32_arm64.elf
STAGE0_ARM64_BIN = $(STAGE0_ARM64_DIR)/stage0_nrv2b_le32_arm64.bin
STAGE0_ARM64_OFFSETS = $(STAGE0_ARM64_DIR)/stage0_nrv2b_le32_offsets_arm64.h
STAGE0_ARM64_EMBED_O = $(BUILD_DIR)/stage0_nrv2b_le32_arm64_bin.o
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
 PACKER_CFLAGS = -O0 -D_7ZIP_ST -DQLZ_COMPRESSION_LEVEL=3 -DQLZ_STREAMING_BUFFER=0 -I $(SRC_DIR)/compressors/apultra -I $(SRC_DIR)/compressors/apultra/libdivsufsort/include -I $(SRC_DIR)/compressors/lzma/C -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/zstd/lib -I $(SRC_DIR)/compressors/zstd/lib/common -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/qlz -I $(SRC_DIR)/compressors/pp -I $(SRC_DIR)/compressors/lzav -I $(SRC_DIR)/compressors/snappy -I $(SRC_DIR)/compressors/zx7b -I $(SRC_DIR)/compressors/zx0 -I $(SRC_DIR)/compressors/doboz -I $(SRC_DIR)/compressors/exo -I $(SRC_DIR)/compressors/shrinkler -I $(SRC_DIR)/compressors/shrinkler/ref -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/compressors/stonecracker -I $(SRC_DIR)/compressors/lzsa/libs/compression -I $(SRC_DIR)/compressors/lzsa/compressor -I $(SRC_DIR)/compressors/lzsa/compressor/libdivsufsort/include -I $(SRC_DIR)/compressors/density -I $(SRC_DIR)/decompressors/density -I $(SRC_DIR)/others -I $(SRC_DIR)/filters -I $(SRC_DIR)/compressors/lzham -I $(SRC_DIR)/compressors/lzham/vendor/lzhamcomp -I $(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp \
	-I $(SRC_DIR)/compressors/rnc -I $(SRC_DIR)/decompressors/zstd -I $(SRC_DIR)/tools/ucl/vendor/include -I $(SRC_DIR)/tools/ucl/vendor/src
else
 STUB_CFLAGS = -c -O2 -nostdlib -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-builtin
 # Aggressive size optimization for packer binary
 PACKER_CFLAGS = -Os $(PACKER_LTO_FLAG) $(FUSE_LD_FLAG) -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fomit-frame-pointer -ffunction-sections -fdata-sections -fvisibility=hidden -fno-plt -fmerge-all-constants -fno-ident -fno-exceptions \
 	-D_7ZIP_ST -DQLZ_COMPRESSION_LEVEL=3 -DQLZ_STREAMING_BUFFER=0 \
 	-I $(SRC_DIR)/compressors/apultra -I $(SRC_DIR)/compressors/apultra/libdivsufsort/include -I $(SRC_DIR)/compressors/lzma/C -I $(SRC_DIR)/compressors/zstd/lib -I $(SRC_DIR)/compressors/zstd/lib/common -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/qlz -I $(SRC_DIR)/compressors/pp -I $(SRC_DIR)/compressors/lzav -I $(SRC_DIR)/compressors/snappy -I $(SRC_DIR)/compressors/zx7b -I $(SRC_DIR)/compressors/zx0 -I $(SRC_DIR)/compressors/doboz -I $(SRC_DIR)/compressors/exo -I $(SRC_DIR)/compressors/shrinkler -I $(SRC_DIR)/compressors/shrinkler/ref -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/compressors/stonecracker -I $(SRC_DIR)/compressors/lzsa/libs/compression -I $(SRC_DIR)/compressors/lzsa/compressor -I $(SRC_DIR)/compressors/lzsa/compressor/libdivsufsort/include -I $(SRC_DIR)/compressors/density -I $(SRC_DIR)/decompressors/density -I $(SRC_DIR)/others -I $(SRC_DIR)/filters -I $(SRC_DIR)/compressors/lzham -I $(SRC_DIR)/compressors/lzham/vendor/lzhamcomp -I $(SRC_DIR)/compressors/lzham/vendor/lzhamdecomp \
	-I $(SRC_DIR)/compressors/rnc -I $(SRC_DIR)/decompressors/zstd -I $(SRC_DIR)/tools/ucl/vendor/include -I $(SRC_DIR)/tools/ucl/vendor/src
endif
 # Linker flags tuned for size (keep functionality)
 PACKER_LDFLAGS = -lm -lstdc++ \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,--as-needed -Wl,-O1,--compress-debug-sections=zlib -Wl,--relax -Wl,--strip-all -s

 ifeq ($(PROFILE),1)
  PACKER_CFLAGS := $(filter-out -flto=% -fomit-frame-pointer,$(PACKER_CFLAGS))
  PACKER_CFLAGS += -g -fno-omit-frame-pointer -fno-optimize-sibling-calls
  # Use explicit linker flags in profile builds to avoid accidental stripping
  # or malformed option lists after filtering.
  PACKER_LDFLAGS = -lm -lstdc++ \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,--as-needed \
	-Wl,-O1,--compress-debug-sections=zlib -Wl,--relax
 endif

# If requested, build packer and selected tools fully static
ifeq ($(STATIC),1)
 PACKER_LDFLAGS += -static -static-libgcc -static-libstdc++
 TOOLS_LDFLAGS += -static -static-libgcc -static-libstdc++
endif

# Cible par défaut
.PHONY: all static packer
all:
	@$(MAKE) tools
	@$(MAKE) stubs
	@$(MAKE) packer
	@printf "\n    \033[97;40m╔╗\033[0m                            \033[97;40m╔╗\033[0m\n    \033[97;40m╚╬════════════════════════════╬╝\033[0m\n     \033[97;40m║\033[97;48;5;166m  ──════════════════════──  \033[97;40m║\n     \033[97;40m║\033[97;48;5;166m ─= Compilation finished =─ \033[97;40m║\n     \033[97;40m║\033[97;48;5;166m   ──══════════════════──   \033[97;40m║\n    \033[97;40m╔╬════════════════════════════╬╗\033[0m\n    \033[97;40m╚╝\033[0m                            \033[97;40m╚╝\033[0m\n"
	@if [ -n "$(STUB_PIE_CFLAGS)" ]; then \
	  printf "\033[37;43m ⚠ WARNING: Stub builds are not size-optimal (STUB_PIE_CFLAGS enabled: %s).\033[0m\n" "$(STUB_PIE_CFLAGS)"; \
	  printf "\033[37;43m   Reason: this toolchain may emit absolute-addressing in stubs when not built as PIE;\n   raw embedded stubs have no relocations and may crash at runtime.\033[0m\n"; \
	fi

# Convenience alias to build static packer/tools/zelf
static:
	$(MAKE) STATIC=1 all
	@printf "\033[37;43m✔ Compilation finished\033[0m\n"

# Compiler le stub
$(BUILD_DIR)/start.o: $(STUB_SRC)/start.S | $(BUILD_DIR)
	@printf "\n       \033[97;40m.___________________.\033[0m\n\033[97;40m ----==|\033[0m\033[97;48;5;166m Compiling start.S \033[0m\033[97;40m|\033[0m\n       \033[97;40m'───────────────────'\033[0m\n"
	@$(CC) -c $(STUB_SRC)/start.S -o $(BUILD_DIR)/start.o

$(BUILD_DIR)/start_sfx.o: $(STUB_SRC)/start_sfx.S | $(BUILD_DIR)
	@printf "\n"
	@printf "    \033[97;40m._______________________.\033[0m\n -==|\033[0m\033[97;48;5;166m Compiling start_sfx.S \033[0m\033[97;40m|\033[0m\n    \033[97;40m'───────────────────────'\033[0m\n"
	@$(CC) -c $(STUB_SRC)/start_sfx.S -o $(BUILD_DIR)/start_sfx.o

$(BUILD_DIR)/elfz_handoff_gate.o: $(STUB_SRC)/elfz_handoff_gate.S | $(BUILD_DIR)
	@printf "\n    \033[97;40m._______________________________.\033[0m\n\033[97;40m -==|\033[0m\033[97;48;5;166m Compiling elfz_handoff_gate.S \033[0m\033[97;40m|\033[0m\n    \033[97;40m'───────────────────────────────'\033[0m\n"
	@$(CC) -c $(STUB_SRC)/elfz_handoff_gate.S -o $(BUILD_DIR)/elfz_handoff_gate.o

$(BUILD_DIR)/elfz_handoff_sequence.o: $(STUB_SRC)/elfz_handoff_sequence.S | $(BUILD_DIR)
	@printf "\n    \033[97;40m.___________________________________.\033[0m\n\033[97;40m -==|\033[0m\033[97;48;5;166m Compiling elfz_handoff_sequence.S \033[0m\033[97;40m|\033[0m\n    \033[97;40m'───────────────────────────────────'\033[0m\n"
	@$(CC) -c $(STUB_SRC)/elfz_handoff_sequence.S -o $(BUILD_DIR)/elfz_handoff_sequence.o

 # Assemble Shrinkler ASM decompressor
 ifeq ($(HOST_ARCH),x86_64)
 $(BUILD_DIR)/shrinkler_decompress_x64.o: $(DECOMP_SHRINKLER_DIR)/shrinkler_decompress_x64.asm | $(BUILD_DIR)
	@printf "\033[37;44m ⚛  Assembling Shrinkler x86_64 decompressor (NASM)...\033[0m\n"
	@$(NASM) -f elf64 -O2 -o $@ $<
 endif

 ifeq ($(HOST_ARCH),arm64)
 $(BUILD_DIR)/shrinkler_decompress_arm64.o: $(DECOMP_SHRINKLER_DIR)/shrinkler_decompress_arm64.S | $(BUILD_DIR)
	@printf "\033[37;44m ⚛  Assembling Shrinkler arm64 decompressor...\033[0m\n"
	@$(CC) -c -o $@ $<
 endif

$(SHRINKLER_COMP_OBJ): $(SHRINKLER_COMP_SRC) | $(BUILD_DIR)
	@printf "\n    \033[97;40m.___________________________________.\033[0m\n\033[97;40m -==|\033[0m\033[97;48;5;166m Compiling Shrinkler (speed flags) \033[0m\033[97;40m|\033[0m\n    \033[97;40m'───────────────────────────────────'\033[0m\n"
	@$(CXX) $(SHRINKLER_COMP_CXXFLAGS) -c $< -o $@

 # --- GENERATION DES STUBS ---
 # Inclure le makefile généré
 -include $(BUILD_DIR)/stubs.mk
 
 STUB_BIN_STEMS = $(patsubst %.bin,%,$(STUB_BIN_FILES_ALL))
 X86_STUB_EMBED_OBJS = $(addprefix $(BUILD_DIR)/embed_stub_x86_,$(addsuffix .o,$(STUB_BIN_STEMS)))
 ARM64_STUB_EMBED_STEMS = \
	stub_lz4_dynamic_bcj \
	stub_lz4_dynamic_kexe \
	stub_lz4_dynamic_bcj_pwd \
	stub_lz4_dynamic_kexe_pwd \
	stub_lz4_static_bcj \
	stub_lz4_static_kexe \
	stub_lz4_static_bcj_pwd \
	stub_lz4_static_kexe_pwd \
	stub_lz4_dynexec_bcj \
	stub_lz4_dynexec_kexe \
	stub_lz4_dynexec_bcj_pwd \
	stub_lz4_dynexec_kexe_pwd \
	stub_rnc_dynamic_bcj \
	stub_rnc_dynamic_kexe \
	stub_rnc_dynamic_bcj_pwd \
	stub_rnc_dynamic_kexe_pwd \
	stub_rnc_static_bcj \
	stub_rnc_static_kexe \
	stub_rnc_static_bcj_pwd \
	stub_rnc_static_kexe_pwd \
	stub_rnc_dynexec_bcj \
	stub_rnc_dynexec_kexe \
	stub_rnc_dynexec_bcj_pwd \
	stub_rnc_dynexec_kexe_pwd \
	stub_shrinkler_dynamic_bcj \
	stub_shrinkler_dynamic_kexe \
	stub_shrinkler_dynamic_bcj_pwd \
	stub_shrinkler_dynamic_kexe_pwd \
	stub_shrinkler_static_bcj \
	stub_shrinkler_static_kexe \
	stub_shrinkler_static_bcj_pwd \
	stub_shrinkler_static_kexe_pwd \
	stub_shrinkler_dynexec_bcj \
	stub_shrinkler_dynexec_kexe \
	stub_shrinkler_dynexec_bcj_pwd \
	stub_shrinkler_dynexec_kexe_pwd \
	stub_exo_dynamic_bcj \
	stub_exo_dynamic_kexe \
	stub_exo_dynamic_bcj_pwd \
	stub_exo_dynamic_kexe_pwd \
	stub_exo_static_bcj \
	stub_exo_static_kexe \
	stub_exo_static_bcj_pwd \
	stub_exo_static_kexe_pwd \
	stub_exo_dynexec_bcj \
	stub_exo_dynexec_kexe \
	stub_exo_dynexec_bcj_pwd \
	stub_exo_dynexec_kexe_pwd \
	stub_snappy_dynamic_bcj \
	stub_snappy_dynamic_kexe \
	stub_snappy_dynamic_bcj_pwd \
	stub_snappy_dynamic_kexe_pwd \
	stub_snappy_static_bcj \
	stub_snappy_static_kexe \
	stub_snappy_static_bcj_pwd \
	stub_snappy_static_kexe_pwd \
	stub_snappy_dynexec_bcj \
	stub_snappy_dynexec_kexe \
	stub_snappy_dynexec_bcj_pwd \
	stub_snappy_dynexec_kexe_pwd \
	stub_doboz_dynamic_bcj \
	stub_doboz_dynamic_kexe \
	stub_doboz_dynamic_bcj_pwd \
	stub_doboz_dynamic_kexe_pwd \
	stub_doboz_static_bcj \
	stub_doboz_static_kexe \
	stub_doboz_static_bcj_pwd \
	stub_doboz_static_kexe_pwd \
	stub_doboz_dynexec_bcj \
	stub_doboz_dynexec_kexe \
	stub_doboz_dynexec_bcj_pwd \
	stub_doboz_dynexec_kexe_pwd \
	stub_nz1_dynamic_bcj \
	stub_nz1_dynamic_kexe \
	stub_nz1_dynamic_bcj_pwd \
	stub_nz1_dynamic_kexe_pwd \
	stub_nz1_static_bcj \
	stub_nz1_static_kexe \
	stub_nz1_static_bcj_pwd \
	stub_nz1_static_kexe_pwd \
	stub_nz1_dynexec_bcj \
	stub_nz1_dynexec_kexe \
	stub_nz1_dynexec_bcj_pwd \
	stub_nz1_dynexec_kexe_pwd \
	stub_pp_dynamic_bcj \
	stub_pp_dynamic_kexe \
	stub_pp_dynamic_bcj_pwd \
	stub_pp_dynamic_kexe_pwd \
	stub_pp_static_bcj \
	stub_pp_static_kexe \
	stub_pp_static_bcj_pwd \
	stub_pp_static_kexe_pwd \
	stub_pp_dynexec_bcj \
	stub_pp_dynexec_kexe \
	stub_pp_dynexec_bcj_pwd \
	stub_pp_dynexec_kexe_pwd \
	stub_lzav_dynamic_bcj \
	stub_lzav_dynamic_kexe \
	stub_lzav_dynamic_bcj_pwd \
	stub_lzav_dynamic_kexe_pwd \
	stub_lzav_static_bcj \
	stub_lzav_static_kexe \
	stub_lzav_static_bcj_pwd \
	stub_lzav_static_kexe_pwd \
	stub_lzav_dynexec_bcj \
	stub_lzav_dynexec_kexe \
	stub_lzav_dynexec_bcj_pwd \
	stub_lzav_dynexec_kexe_pwd \
	stub_qlz_dynamic_bcj \
	stub_qlz_dynamic_kexe \
	stub_qlz_dynamic_bcj_pwd \
	stub_qlz_dynamic_kexe_pwd \
	stub_qlz_static_bcj \
	stub_qlz_static_kexe \
	stub_qlz_static_bcj_pwd \
	stub_qlz_static_kexe_pwd \
	stub_qlz_dynexec_bcj \
	stub_qlz_dynexec_kexe \
	stub_qlz_dynexec_bcj_pwd \
	stub_qlz_dynexec_kexe_pwd \
	stub_lzsa_dynamic_bcj \
	stub_lzsa_dynamic_kexe \
	stub_lzsa_dynamic_bcj_pwd \
	stub_lzsa_dynamic_kexe_pwd \
	stub_lzsa_static_bcj \
	stub_lzsa_static_kexe \
	stub_lzsa_static_bcj_pwd \
	stub_lzsa_static_kexe_pwd \
	stub_lzsa_dynexec_bcj \
	stub_lzsa_dynexec_kexe \
	stub_lzsa_dynexec_bcj_pwd \
	stub_lzsa_dynexec_kexe_pwd \
	stub_lzham_dynamic_bcj \
	stub_lzham_dynamic_kexe \
	stub_lzham_dynamic_bcj_pwd \
	stub_lzham_dynamic_kexe_pwd \
	stub_lzham_static_bcj \
	stub_lzham_static_kexe \
	stub_lzham_static_bcj_pwd \
	stub_lzham_static_kexe_pwd \
	stub_lzham_dynexec_bcj \
	stub_lzham_dynexec_kexe \
	stub_lzham_dynexec_bcj_pwd \
	stub_lzham_dynexec_kexe_pwd \
	stub_lzfse_dynamic_bcj \
	stub_lzfse_dynamic_kexe \
	stub_lzfse_dynamic_bcj_pwd \
	stub_lzfse_dynamic_kexe_pwd \
	stub_lzfse_static_bcj \
	stub_lzfse_static_kexe \
	stub_lzfse_static_bcj_pwd \
	stub_lzfse_static_kexe_pwd \
	stub_lzfse_dynexec_bcj \
	stub_lzfse_dynexec_kexe \
	stub_lzfse_dynexec_bcj_pwd \
	stub_lzfse_dynexec_kexe_pwd \
	stub_csc_dynamic_bcj \
	stub_csc_dynamic_kexe \
	stub_csc_dynamic_bcj_pwd \
	stub_csc_dynamic_kexe_pwd \
	stub_csc_static_bcj \
	stub_csc_static_kexe \
	stub_csc_static_bcj_pwd \
	stub_csc_static_kexe_pwd \
	stub_csc_dynexec_bcj \
	stub_csc_dynexec_kexe \
	stub_csc_dynexec_bcj_pwd \
	stub_csc_dynexec_kexe_pwd \
	stub_lzma_dynamic_bcj \
	stub_lzma_dynamic_kexe \
	stub_lzma_dynamic_bcj_pwd \
	stub_lzma_dynamic_kexe_pwd \
	stub_lzma_static_bcj \
	stub_lzma_static_kexe \
	stub_lzma_static_bcj_pwd \
	stub_lzma_static_kexe_pwd \
	stub_lzma_dynexec_bcj \
	stub_lzma_dynexec_kexe \
	stub_lzma_dynexec_bcj_pwd \
	stub_lzma_dynexec_kexe_pwd \
	stub_sc_dynamic_bcj \
	stub_sc_dynamic_kexe \
	stub_sc_dynamic_bcj_pwd \
	stub_sc_dynamic_kexe_pwd \
	stub_sc_static_bcj \
	stub_sc_static_kexe \
	stub_sc_static_bcj_pwd \
	stub_sc_static_kexe_pwd \
	stub_sc_dynexec_bcj \
	stub_sc_dynexec_kexe \
	stub_sc_dynexec_bcj_pwd \
	stub_sc_dynexec_kexe_pwd \
	stub_apultra_dynamic_bcj \
	stub_apultra_dynamic_kexe \
	stub_apultra_dynamic_bcj_pwd \
	stub_apultra_dynamic_kexe_pwd \
	stub_apultra_static_bcj \
	stub_apultra_static_kexe \
	stub_apultra_static_bcj_pwd \
	stub_apultra_static_kexe_pwd \
	stub_apultra_dynexec_bcj \
	stub_apultra_dynexec_kexe \
	stub_apultra_dynexec_bcj_pwd \
	stub_apultra_dynexec_kexe_pwd \
	stub_zstd_dynamic_bcj \
	stub_zstd_dynamic_kexe \
	stub_zstd_dynamic_bcj_pwd \
	stub_zstd_dynamic_kexe_pwd \
	stub_zstd_static_bcj \
	stub_zstd_static_kexe \
	stub_zstd_static_bcj_pwd \
	stub_zstd_static_kexe_pwd \
	stub_zstd_dynexec_bcj \
	stub_zstd_dynexec_kexe \
	stub_zstd_dynexec_bcj_pwd \
	stub_zstd_dynexec_kexe_pwd \
	stub_density_dynamic_bcj \
	stub_density_dynamic_kexe \
	stub_density_dynamic_bcj_pwd \
	stub_density_dynamic_kexe_pwd \
	stub_density_static_bcj \
	stub_density_static_kexe \
	stub_density_static_bcj_pwd \
	stub_density_static_kexe_pwd \
	stub_density_dynexec_bcj \
	stub_density_dynexec_kexe \
	stub_density_dynexec_bcj_pwd \
	stub_density_dynexec_kexe_pwd \
	stub_blz_dynamic_bcj \
	stub_blz_dynamic_kexe \
	stub_blz_dynamic_bcj_pwd \
	stub_blz_dynamic_kexe_pwd \
	stub_blz_static_bcj \
	stub_blz_static_kexe \
	stub_blz_static_bcj_pwd \
	stub_blz_static_kexe_pwd \
	stub_blz_dynexec_bcj \
	stub_blz_dynexec_kexe \
	stub_blz_dynexec_bcj_pwd \
	stub_blz_dynexec_kexe_pwd \
	stub_zx0_dynamic_bcj \
	stub_zx0_dynamic_kexe \
	stub_zx0_dynamic_bcj_pwd \
	stub_zx0_dynamic_kexe_pwd \
	stub_zx0_static_bcj \
	stub_zx0_static_kexe \
	stub_zx0_static_bcj_pwd \
	stub_zx0_static_kexe_pwd \
	stub_zx0_dynexec_bcj \
	stub_zx0_dynexec_kexe \
	stub_zx0_dynexec_bcj_pwd \
	stub_zx0_dynexec_kexe_pwd \
	stub_zx7b_dynamic_bcj \
	stub_zx7b_dynamic_kexe \
	stub_zx7b_dynamic_bcj_pwd \
	stub_zx7b_dynamic_kexe_pwd \
	stub_zx7b_static_bcj \
	stub_zx7b_static_kexe \
	stub_zx7b_static_bcj_pwd \
	stub_zx7b_static_kexe_pwd \
	stub_zx7b_dynexec_bcj \
	stub_zx7b_dynexec_kexe \
	stub_zx7b_dynexec_bcj_pwd \
	stub_zx7b_dynexec_kexe_pwd \
	stub_sfx_lz4 \
	stub_sfx_rnc \
	stub_sfx_apultra \
	stub_sfx_lzav \
	stub_sfx_zstd \
	stub_sfx_blz \
	stub_sfx_doboz \
	stub_sfx_exo \
	stub_sfx_pp \
	stub_sfx_qlz \
	stub_sfx_snappy \
	stub_sfx_shrinkler \
	stub_sfx_lzma \
	stub_sfx_zx7b \
	stub_sfx_zx0 \
	stub_sfx_density \
	stub_sfx_sc \
	stub_sfx_lzsa \
	stub_sfx_lzham \
	stub_sfx_lzfse \
	stub_sfx_csc \
	stub_sfx_nz1
 ARM64_STUB_EMBED_OBJS = $(addprefix $(BUILD_DIR)/embed_stub_arm64_,$(addsuffix .o,$(ARM64_STUB_EMBED_STEMS)))
 ALL_STUB_OBJS = $(X86_STUB_EMBED_OBJS) $(ARM64_STUB_EMBED_OBJS)
 
  # Règle pour générer stubs.mk s'il n'existe pas ou si le script change
$(BUILD_DIR)/stubs.mk: tools/gen_stubs_mk.py | $(BUILD_DIR)
	@printf "\n      \033[90;40m╭───────────────────────╮\033[0m\n -----\033[90;40m│\033[0m\033[97;44m ⚙ Generating stubs.mk\033[0m\033[90;40m │\033[0m\n      \033[90;40m╰───────────────────────╯\033[0m\n"
	@ELFZ_STUB_ALGOS=$$( [ "$(HOST_ARCH)" = "arm64" ] && echo "lz4 rnc zstd density blz shrinkler exo snappy doboz nz1 pp zx0 zx7b qlz lzav lzsa lzham lzfse csc lzma sc apultra" || echo all ); \
	  ELFZ_STUB_ALGOS="$$ELFZ_STUB_ALGOS" $(PYTHON) tools/gen_stubs_mk.py > $@

 # Cible stubs qui force la génération
 .PHONY: stubs
 stubs: host_guard $(BUILD_DIR)/stubs.mk
	@$(MAKE) -f Makefile actual_stubs -j$(STUBS_J)

 .PHONY: actual_stubs
 actual_stubs: $(ALL_STUB_BINS)

 .PHONY: check_x86_64_stubs
 check_x86_64_stubs: $(BUILD_DIR)/stubs.mk | $(STUBS_X86_DIR)
	@if [ "$(HOST_ARCH)" != "x86_64" ]; then exit 0; fi
	@missing=0; \
	for f in $(STUB_BIN_FILES); do \
	  if [ ! -f "$(STUBS_X86_DIR)/$$f" ]; then \
	    if [ $$missing -eq 0 ]; then \
	      printf "\033[37;41m✘ Missing required x86_64 stub(s) in %s\033[0m\n" "$(STUBS_X86_DIR)"; \
	    fi; \
	    printf "  - %s\n" "$$f"; \
	    missing=1; \
	  fi; \
	done; \
	if [ $$missing -ne 0 ]; then \
	  printf "\nProvide prebuilt x86_64 stubs into %s and retry.\n" "$(STUBS_X86_DIR)"; \
	  exit 1; \
	fi
 
 .PHONY: check_stage0_prebuilt
 check_stage0_prebuilt:
	@if [ "$(HOST_ARCH)" = "arm64" ]; then \
	  if [ ! -f "$(STAGE0_X86_BIN)" ] || [ ! -f "$(STAGE0_X86_OFFSETS)" ]; then \
	    printf "\n  ╭=====================================╮\n  [\033[30;104m ⚠ stage0 artifacts not detected for \033[0m]\n  [\033[30;104m   for cross-target x86_64           \033[0m]\n  [\033[30;104m   stage0-wrapped codecs.            \033[0m]\n  ╰=====================================╯\n"; \
	    printf "  - %s\n" "$(STAGE0_X86_BIN)"; \
	    printf "  - %s\n" "$(STAGE0_X86_OFFSETS)"; \
	  fi; \
	elif [ "$(HOST_ARCH)" = "x86_64" ]; then \
	  if [ ! -f "$(STAGE0_ARM64_BIN)" ] || [ ! -f "$(STAGE0_ARM64_OFFSETS)" ]; then \
	    printf "\n  ╭=========================================╮\n  [\033[30;104m ⚠ stage0 artifacts not detected for     \033[0m]\n  [\033[30;104m   cross-target arm64 stage0-wrapped     \033[0m]\n  [\033[30;104m   codecs.                               \033[0m]\n  ╰=========================================╯\n"; \
	    printf "  - %s\n" "$(STAGE0_ARM64_BIN)"; \
	    printf "  - %s\n" "$(STAGE0_ARM64_OFFSETS)"; \
	  fi; \
	fi

 .PHONY: host_guard
 host_guard: | $(BUILD_DIR)
	@prev=$$(cat "$(BUILD_ARCH_STAMP)" 2>/dev/null || true); \
	if [ "$$prev" != "$(HOST_ARCH)" ]; then \
	  if [ -n "$$prev" ]; then \
	    printf "\033[37;43m⚠ Host arch changed (%s -> %s): purging incompatible host objects in %s (stubs/stage0 kept).\033[0m\n" "$$prev" "$(HOST_ARCH)" "$(BUILD_DIR)"; \
	  fi; \
	  rm -f $(BUILD_DIR)/*.o; \
	  rm -f $(BUILD_DIR)/*.elf; \
	  rm -f $(BUILD_DIR)/zelf; \
	  rm -f $(BUILD_DIR)/stubs.mk; \
	  rm -f $(BUILD_DIR)/build_info.h; \
	  rm -f $(STAGE0_X86_ELF) $(STAGE0_ARM64_ELF) $(STAGE0_X86_EMBED_O) $(STAGE0_ARM64_EMBED_O); \
	  printf "%s" "$(HOST_ARCH)" > "$(BUILD_ARCH_STAMP)"; \
	fi

 .PHONY: print_cross_arch_status
 print_cross_arch_status:
	@if [ "$(HOST_ARCH)" = "arm64" ]; then \
	  if [ -d "$(STUBS_X86_DIR)" ] && ls "$(STUBS_X86_DIR)"/*.bin >/dev/null 2>&1 && \
	     [ -f "$(STAGE0_X86_BIN)" ] && [ -f "$(STAGE0_X86_OFFSETS)" ]; then \
	    printf "\n  ╭=========================================╮\n  [\033[97;44m  Building the packer on Aarch64 system  \033[0m]\n  [-----------------------------------------]\n  [\033[30;104m                                         \033[0m]\n  [\033[30;104m ➤ x86_64 stubs detected: packer will be \033[0m]\n  [\033[30;104m   able to pack/unpack arm64 and x86_64  \033[0m]\n  [\033[30;104m   ELF binaries.                         \033[0m]\n  ╰=========================================╯\n"; \
	  else \
	    printf "\n  ╭=========================================╮\n  [\033[97;44m  Building the packer on Aarch64 system  \033[0m]\n  [-----------------------------------------]\n  [\033[30;103m                                         \033[0m]\n  [\033[30;103m ✗ x86_64 stubs not detected so the      \033[0m]\n  [\033[30;103m   packer will be only able to           \033[0m]\n  [\033[30;103m   pack/unpack Aarch64 ELF binaries.     \033[0m]\n  ╰=========================================╯\n"; \
	  fi; \
	elif [ "$(HOST_ARCH)" = "x86_64" ]; then \
	  if [ -d "$(STUBS_ARM64_DIR)" ] && ls "$(STUBS_ARM64_DIR)"/*.bin >/dev/null 2>&1 && \
	     [ -f "$(STAGE0_ARM64_BIN)" ] && [ -f "$(STAGE0_ARM64_OFFSETS)" ]; then \
	    printf "\n  ╭=========================================╮\n  [\033[97;44m  Building the packer on x86_64 system   \033[0m]\n  [-----------------------------------------]\n  [\033[30;104m                                         \033[0m]\n  [\033[30;104m ➤ Aarch64 stubs detected: packer will be\033[0m]\n  [\033[30;104m   able to pack/unpack x86_64 and arm64  \033[0m]\n  [\033[30;104m   ELF binaries.                         \033[0m]\n  ╰=========================================╯\n"; \
	  else \
	    printf "\n  ╭=========================================╮\n  [\033[97;44m  Building the packer on x86_64 system   \033[0m]\n  [-----------------------------------------]\n  [\033[30;103m                                         \033[0m]\n  [\033[30;103m ✗ Aarch64 stubs not detected so the     \033[0m]\n  [\033[30;103m   packer will be only able to           \033[0m]\n  [\033[30;103m   pack/unpack x86_64 ELF binaries.      \033[0m]\n  ╰=========================================╯\n"; \
	  fi; \
	fi

$(BUILD_DIR)/embed_stub_x86_%.o: FORCE | $(BUILD_DIR) $(BUILD_DIR)/stubs
	@f="$(BUILD_DIR)/stubs/$*.bin"; \
	if [ ! -f "$$f" ]; then \
	  printf '__asm__(".global _binary_build_stubs_%s_bin_start\\n" \
	                 "_binary_build_stubs_%s_bin_start:\\n" \
	                 ".global _binary_build_stubs_%s_bin_end\\n" \
	                 "_binary_build_stubs_%s_bin_end:\\n");\n' "$*" "$*" "$*" "$*" | $(CC) -c -x c -o $@ -; \
	  exit 0; \
	fi; \
	ld -r -b binary -o $@ "$$f"

$(BUILD_DIR)/embed_stub_arm64_%.o: FORCE | $(BUILD_DIR)
	@f="$(STUBS_ARM64_DIR)/$*.bin"; \
	if [ ! -f "$$f" ]; then \
	  printf "" | $(CC) -c -x c -o $@ -; \
	  exit 0; \
	fi; \
	ld -r -b binary -o $@ "$$f"

$(BUILD_DIR)/static_final_noreturn.o: $(STUB_SRC)/static_final_noreturn.S | $(BUILD_DIR)
	@printf "\n    \033[97;40m.________________________________.\033[0m\n\033[97;40m -==|\033[0m\033[97;48;5;166m Building stage0 NRV2B LE32 ELF \033[0m\033[97;40m|\033[0m\n    \033[97;40m'────────────────────────────────'\033[0m\n"
	@$(CC) -c $(STUB_SRC)/static_final_noreturn.S -o $(BUILD_DIR)/static_final_noreturn.o

$(BUILD_DIR)/zstd_minidec.o: $(SRC_DIR)/decompressors/zstd/zstd_minidec.c | $(BUILD_DIR)
	@printf "\n        \033[97;40m.__________________________.\033[0m\n\033[97;40m -----==|\033[0m\033[97;48;5;166m Compiling zstd_minidec.c \033[0m\033[97;40m|\033[0m\n        \033[97;40m'──────────────────────────'\033[0m\n"
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

$(STAGE0_X86_DIR) $(STAGE0_ARM64_DIR): | $(BUILD_DIR)
	@mkdir -p $@

 ifeq ($(HOST_ARCH),x86_64)
 $(STAGE0_X86_ELF): $(SRC_DIR)/stub/stage0_nrv2b_le32.S $(SRC_DIR)/stub/stage0.ld $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.c $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.h | $(STAGE0_X86_DIR)
	@printf "\n    \033[97;40m.________________________________.\033[0m\n\033[97;40m -==|\033[0m\033[97;48;5;166m Building stage0 NRV2B LE32 ELF \033[0m\033[97;40m|\033[0m\n    \033[97;40m'────────────────────────────────'\033[0m\n"
	@$(CC) -Os -ffreestanding -fno-plt -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -nostdlib -static \
		-I $(SRC_DIR)/tools/ucl \
		-Wl,-T,$(SRC_DIR)/stub/stage0.ld -Wl,-e,stage0_nrv2b_entry -Wl,--build-id=none -Wl,-z,noexecstack \
		$(SRC_DIR)/stub/stage0_nrv2b_le32.S $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.c -o $@

 $(STAGE0_X86_BIN): $(STAGE0_X86_ELF)
	@printf "\033[37;44m ↔ Objcopy stage0 -> bin...\033[0m\n"
	@objcopy -O binary -j .text -j .rodata $< $@

 $(STAGE0_X86_OFFSETS): $(STAGE0_X86_ELF)
	@printf "\033[37;44m ≡ Export stage0 header offsets...\033[0m\n"
	@nm -S $< | perl -ne 'BEGIN{$$b=$$u=$$c=$$d=$$bo=$$f=undef;} if(/stage0_nrv2b_entry/){($$a)=split; $$b=hex($$a);} if(/stage0_hdr_ulen/){($$a)=split; $$u=hex($$a);} if(/stage0_hdr_clen/){($$a)=split; $$c=hex($$a);} if(/stage0_hdr_dst/){($$a)=split; $$d=hex($$a);} if(/stage0_hdr_blob/){($$a)=split; $$bo=hex($$a);} if(/stage0_hdr_flags/){($$a)=split; $$f=hex($$a);} END{die "stage0_nrv2b_entry not found\n" unless defined $$b; print "#define STAGE0_X86_HDR_ULEN_OFF 0x", sprintf("%x",$$u-$$b), "\n"; print "#define STAGE0_X86_HDR_CLEN_OFF 0x", sprintf("%x",$$c-$$b), "\n"; print "#define STAGE0_X86_HDR_DST_OFF  0x", sprintf("%x",$$d-$$b), "\n"; print "#define STAGE0_X86_HDR_BLOB_OFF 0x", sprintf("%x",$$bo-$$b), "\n"; print "#define STAGE0_X86_HDR_FLAGS_OFF 0x", sprintf("%x",$$f-$$b), "\n";}' > $@
 endif

 ifeq ($(HOST_ARCH),arm64)
 $(STAGE0_ARM64_ELF): $(SRC_DIR)/stub/stage0_nrv2b_le32_arm64.S $(SRC_DIR)/stub/stage0.ld $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.c $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.h | $(STAGE0_ARM64_DIR)
	@printf "\n    \033[97;40m.________________________________.\033[0m\n\033[97;40m -==|\033[0m\033[97;48;5;166m Building stage0 NRV2B LE32 ELF \033[0m\033[97;40m|\033[0m\n    \033[97;40m'────────────────────────────────'\033[0m\n"
	@$(CC) -Os -ffreestanding -fno-plt -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -nostdlib -static \
		-I $(SRC_DIR)/tools/ucl \
		-Wl,-T,$(SRC_DIR)/stub/stage0.ld -Wl,-e,stage0_nrv2b_entry -Wl,--build-id=none -Wl,-z,noexecstack \
		$(SRC_DIR)/stub/stage0_nrv2b_le32_arm64.S $(SRC_DIR)/tools/ucl/minidec_nrv2b_le32.c -o $@

 $(STAGE0_ARM64_BIN): $(STAGE0_ARM64_ELF)
	@printf "\033[37;44m ↔ Objcopy stage0 -> bin...\033[0m\n"
	@objcopy -O binary -j .text -j .rodata $< $@

 $(STAGE0_ARM64_OFFSETS): $(STAGE0_ARM64_ELF)
	@printf "\033[37;44m ≡ Export stage0 header offsets...\033[0m\n"
	@nm -S $< | perl -ne 'BEGIN{$$b=$$u=$$c=$$d=$$bo=$$f=undef;} if(/stage0_nrv2b_entry/){($$a)=split; $$b=hex($$a);} if(/stage0_hdr_ulen/){($$a)=split; $$u=hex($$a);} if(/stage0_hdr_clen/){($$a)=split; $$c=hex($$a);} if(/stage0_hdr_dst/){($$a)=split; $$d=hex($$a);} if(/stage0_hdr_blob/){($$a)=split; $$bo=hex($$a);} if(/stage0_hdr_flags/){($$a)=split; $$f=hex($$a);} END{die "stage0_nrv2b_entry not found\n" unless defined $$b; print "#define STAGE0_ARM64_HDR_ULEN_OFF 0x", sprintf("%x",$$u-$$b), "\n"; print "#define STAGE0_ARM64_HDR_CLEN_OFF 0x", sprintf("%x",$$c-$$b), "\n"; print "#define STAGE0_ARM64_HDR_DST_OFF  0x", sprintf("%x",$$d-$$b), "\n"; print "#define STAGE0_ARM64_HDR_BLOB_OFF 0x", sprintf("%x",$$bo-$$b), "\n"; print "#define STAGE0_ARM64_HDR_FLAGS_OFF 0x", sprintf("%x",$$f-$$b), "\n";}' > $@
endif

ifeq ($(HOST_ARCH),x86_64)
$(STAGE0_X86_EMBED_O): $(STAGE0_X86_BIN) | $(BUILD_DIR)
	@printf "\033[37;44m→→ Embedding stage0 bin...\033[0m\n"
	@if [ ! -f "$(STAGE0_X86_BIN)" ]; then \
	  printf '.global _binary_build_stage0_nrv2b_le32_x86_64_bin_start\n.global _binary_build_stage0_nrv2b_le32_x86_64_bin_end\n_binary_build_stage0_nrv2b_le32_x86_64_bin_start:\n_binary_build_stage0_nrv2b_le32_x86_64_bin_end:\n' | $(CC) -c -x assembler -o $@ -; \
	  exit 0; \
	fi; \
	printf '.section .rodata\n.global _binary_build_stage0_nrv2b_le32_x86_64_bin_start\n.global _binary_build_stage0_nrv2b_le32_x86_64_bin_end\n_binary_build_stage0_nrv2b_le32_x86_64_bin_start:\n  .incbin "$(STAGE0_X86_BIN)"\n_binary_build_stage0_nrv2b_le32_x86_64_bin_end:\n' | $(CC) -c -x assembler -o $@ -
else
$(STAGE0_X86_EMBED_O): FORCE | $(BUILD_DIR)
	@printf "\033[37;44m→→ Embedding stage0 bin...\033[0m\n"
	@if [ ! -f "$(STAGE0_X86_BIN)" ]; then \
	  printf '.global _binary_build_stage0_nrv2b_le32_x86_64_bin_start\n.global _binary_build_stage0_nrv2b_le32_x86_64_bin_end\n_binary_build_stage0_nrv2b_le32_x86_64_bin_start:\n_binary_build_stage0_nrv2b_le32_x86_64_bin_end:\n' | $(CC) -c -x assembler -o $@ -; \
	  exit 0; \
	fi; \
	printf '.section .rodata\n.global _binary_build_stage0_nrv2b_le32_x86_64_bin_start\n.global _binary_build_stage0_nrv2b_le32_x86_64_bin_end\n_binary_build_stage0_nrv2b_le32_x86_64_bin_start:\n  .incbin "$(STAGE0_X86_BIN)"\n_binary_build_stage0_nrv2b_le32_x86_64_bin_end:\n' | $(CC) -c -x assembler -o $@ -
endif

ifeq ($(HOST_ARCH),arm64)
$(STAGE0_ARM64_EMBED_O): $(STAGE0_ARM64_BIN) | $(BUILD_DIR)
	@printf "\033[37;44m→→→ Embedding stage0 bin...\033[0m\n"
	@if [ ! -f "$(STAGE0_ARM64_BIN)" ]; then \
	  printf '.global _binary_build_stage0_nrv2b_le32_arm64_bin_start\n.global _binary_build_stage0_nrv2b_le32_arm64_bin_end\n_binary_build_stage0_nrv2b_le32_arm64_bin_start:\n_binary_build_stage0_nrv2b_le32_arm64_bin_end:\n' | $(CC) -c -x assembler -o $@ -; \
	  exit 0; \
	fi; \
	printf '.section .rodata\n.global _binary_build_stage0_nrv2b_le32_arm64_bin_start\n.global _binary_build_stage0_nrv2b_le32_arm64_bin_end\n_binary_build_stage0_nrv2b_le32_arm64_bin_start:\n  .incbin "$(STAGE0_ARM64_BIN)"\n_binary_build_stage0_nrv2b_le32_arm64_bin_end:\n' | $(CC) -c -x assembler -o $@ -
else
$(STAGE0_ARM64_EMBED_O): FORCE | $(BUILD_DIR)
	@printf "\033[37;44m→→→ Embedding stage0 bin...\033[0m\n"
	@if [ ! -f "$(STAGE0_ARM64_BIN)" ]; then \
	  printf '.global _binary_build_stage0_nrv2b_le32_arm64_bin_start\n.global _binary_build_stage0_nrv2b_le32_arm64_bin_end\n_binary_build_stage0_nrv2b_le32_arm64_bin_start:\n_binary_build_stage0_nrv2b_le32_arm64_bin_end:\n' | $(CC) -c -x assembler -o $@ -; \
	  exit 0; \
	fi; \
	printf '.section .rodata\n.global _binary_build_stage0_nrv2b_le32_arm64_bin_start\n.global _binary_build_stage0_nrv2b_le32_arm64_bin_end\n_binary_build_stage0_nrv2b_le32_arm64_bin_start:\n  .incbin "$(STAGE0_ARM64_BIN)"\n_binary_build_stage0_nrv2b_le32_arm64_bin_end:\n' | $(CC) -c -x assembler -o $@ -
endif

ifneq ($(HOST_ARCH),x86_64)
$(STAGE0_X86_OFFSETS): $(SRC_DIR)/stub/stage0_nrv2b_le32.S | $(STAGE0_X86_DIR)
	@if [ ! -f "$@" ]; then \
	  printf '#define STAGE0_X86_HDR_ULEN_OFF 0x4a\n#define STAGE0_X86_HDR_CLEN_OFF 0x4e\n#define STAGE0_X86_HDR_DST_OFF  0x52\n#define STAGE0_X86_HDR_BLOB_OFF 0x5a\n#define STAGE0_X86_HDR_FLAGS_OFF 0x62\n' > $@; \
	fi
endif

ifneq ($(HOST_ARCH),arm64)
$(STAGE0_ARM64_OFFSETS): $(SRC_DIR)/stub/stage0_nrv2b_le32_arm64.S | $(STAGE0_ARM64_DIR)
	@if [ ! -f "$@" ] || grep -q "STAGE0_ARM64_HDR_DST_OFF  0x54" "$@"; then \
	  printf '#define STAGE0_ARM64_HDR_ULEN_OFF 0x4c\n#define STAGE0_ARM64_HDR_CLEN_OFF 0x50\n#define STAGE0_ARM64_HDR_DST_OFF  0x58\n#define STAGE0_ARM64_HDR_BLOB_OFF 0x60\n#define STAGE0_ARM64_HDR_FLAGS_OFF 0x68\n' > $@; \
	fi
endif

 $(PACKER_BIN): host_guard check_x86_64_stubs check_stage0_prebuilt $(BUILD_INFO_H) $(ALL_STUB_OBJS) $(PACKER_C) $(CSC_COMP_OBJS) $(SHRINKLER_COMP_OBJ) $(BUILD_DIR)/zstd_minidec.o $(SHRINKLER_ASM_OBJ) $(STAGE0_X86_EMBED_O) $(STAGE0_ARM64_EMBED_O) $(STAGE0_X86_OFFSETS) $(STAGE0_ARM64_OFFSETS) $(UCL_VENDOR_OBJS) | $(BUILD_DIR)
	@$(CC) $(PACKER_CFLAGS) -I $(BUILD_DIR) $(PACKER_C) $(CSC_COMP_OBJS) $(SHRINKLER_COMP_OBJ) $(ALL_STUB_OBJS) $(BUILD_DIR)/zstd_minidec.o $(SHRINKLER_ASM_OBJ) $(STAGE0_X86_EMBED_O) $(STAGE0_ARM64_EMBED_O) \
		$(UCL_VENDOR_OBJS) \
		-o $(PACKER_BIN) $(PACKER_LDFLAGS)
	@if [ "$(PROFILE)" != "1" ]; then \
	  command -v sstrip >/dev/null 2>&1 && sstrip $(PACKER_BIN) || true; \
	  printf "\033[37;44m ¤ Post-build: purge .o and symlinks in $(BUILD_DIR)\033[0m\n"; \
	  rm -f $(BUILD_DIR)/*.o; \
	  rm -f $(BUILD_DIR)/*.elf; \
#	  rm -f $(BUILD_DIR)/stub_*_*.bin; \
	  rm -f $(BUILD_DIR)/stubs.mk; \
	  rm -f $(BUILD_DIR)/build_info.h; \
	  rm -f $(STAGE0_X86_ELF) $(STAGE0_ARM64_ELF); \
	fi

$(BUILD_INFO_H): | $(BUILD_DIR)
	@dt=$$(date '+%Y-%m-%d %H:%M:%S'); \
	 distro=unknown; \
	 if [ -r /etc/os-release ]; then \
	   . /etc/os-release; \
	   distro=$${ID:-unknown}; \
	   case "$$distro" in \
	     opensuse*|suse|sles) distro=opensuse ;; \
	   esac; \
	 fi; \
	 glibc=$$(getconf GNU_LIBC_VERSION 2>/dev/null || true); \
	 if [ -z "$$glibc" ]; then glibc=$$(ldd --version 2>/dev/null | head -n1 | sed 's/[[:space:]]\+/ /g' || true); fi; \
	 if [ -z "$$glibc" ]; then glibc=unknown; fi; \
	 os=$$(uname -sr 2>/dev/null || echo unknown); \
	 cpu=$$(uname -m 2>/dev/null || echo unknown); \
	 model=$$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2- | sed 's/^ *//'); \
	 if [ -n "$$model" ]; then cpu="$$cpu / $$model"; fi; \
	 if [ "$(STATIC)" = "1" ]; then link=Static; else link=Dynamic; fi; \
	 lsel="$(FUSE_LD)"; \
	 if [ -z "$$lsel" ] || [ "$$lsel" = "default" ]; then lsel=default; fi; \
	 case "$$lsel" in \
	   bfd) linker="ld.bfd" ;; \
	   lld) linker="ld.lld" ;; \
	   *) linker="ld" ;; \
	 esac; \
	 gcc_v=$$( $(CC) --version 2>/dev/null | head -n1 | sed 's/[[:space:]]\+/ /g' || true ); \
	 if [ -z "$$gcc_v" ]; then gcc_v=unknown; fi; \
	 ld_v=$$( $$linker --version 2>/dev/null | head -n1 | sed 's/[[:space:]]\+/ /g' || true ); \
	 if [ -z "$$ld_v" ]; then ld_v=unknown; fi; \
	 objcopy_v=$$( objcopy --version 2>/dev/null | head -n1 | sed 's/[[:space:]]\+/ /g' || true ); \
	 if [ -z "$$objcopy_v" ]; then objcopy_v=unknown; fi; \
	 nm_v=$$( nm --version 2>/dev/null | head -n1 | sed 's/[[:space:]]\+/ /g' || true ); \
	 if [ -z "$$nm_v" ]; then nm_v=unknown; fi; \
	 as_v=$$( as --version 2>/dev/null | head -n1 | sed 's/[[:space:]]\+/ /g' || true ); \
	 if [ -z "$$as_v" ]; then as_v=unknown; fi; \
	 { \
		echo '#ifndef ZELF_BUILD_INFO_H'; \
		echo '#define ZELF_BUILD_INFO_H'; \
		echo '#define ZELF_BUILD_DATETIME "'"$$dt"'"'; \
		echo '#define ZELF_BUILD_DISTRO "'"$$distro"'"'; \
		echo '#define ZELF_BUILD_GLIBC "'"$$glibc"'"'; \
		echo '#define ZELF_BUILD_HOST_OS "'"$$os"'"'; \
		echo '#define ZELF_BUILD_HOST_CPU "'"$$cpu"'"'; \
		echo '#define ZELF_BUILD_LINKER "'"$$linker"'"'; \
		echo '#define ZELF_BUILD_CC "'"$$gcc_v"'"'; \
		echo '#define ZELF_BUILD_LD_VERSION "'"$$ld_v"'"'; \
		echo '#define ZELF_BUILD_OBJCOPY_VERSION "'"$$objcopy_v"'"'; \
		echo '#define ZELF_BUILD_NM_VERSION "'"$$nm_v"'"'; \
		echo '#define ZELF_BUILD_AS_VERSION "'"$$as_v"'"'; \
		echo '#define ZELF_BUILD_LINKAGE "'"$$link"'"'; \
		echo '#endif'; \
	 } > $@.tmp; \
	 mv -f $@.tmp $@

# Build only the packer.
packer: tools | $(BUILD_DIR)
	@printf "\n      \033[90;40m╭────────────────────────╮\033[0m\n -----\033[90;40m│\033[0m\033[97;44m Building packer binary \033[0m\033[90;40m│\033[0m\n      \033[90;40m╰────────────────────────╯\033[0m\n"
	@$(MAKE) host_guard
	@$(MAKE) print_cross_arch_status
	@$(MAKE) $(PACKER_BIN)

# Build the Kanzi .text benchmark tool
.PHONY: kanzi_text_bench
kanzi_text_bench: $(BUILD_DIR) $(FILTERS_DIR)/kanzi_exe_encode.cpp $(FILTERS_DIR)/kanzi_exe_encode_c.cpp $(SRC_DIR)/tools/kanzi_text_bench.c
	@printf "\n      \033[90;40m╭───────────────────────────╮\033[0m\n -----\033[90;40m│\033[0m\033[97;44m Building kanzi_text_bench \033[0m\033[90;40m│\033[0m\n      \033[90;40m╰───────────────────────────╯\033[0m\n"
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
	@printf "\n   \033[90;40m╭─────────────────────────────╮\033[0m\n --\033[90;40m│\033[0m\033[97;44m ❯ Cleaning                  \033[0m\033[90;40m│\033[0m\n   \033[90;40m╰─────────────────────────────╯\033[0m\n"
	rm -f $(BUILD_DIR)/start.o $(BUILD_DIR)/elfz_handoff_gate.o $(BUILD_DIR)/elfz_handoff_sequence.o
	rm -f $(BUILD_DIR)/stubs.mk
	rm -f $(BUILD_DIR)/*.elf
	rm -f $(BUILD_DIR)/*.o
	rm -rf $(BUILD_DIR)/stubs $(STUBS_HOST_DIR)
	rm -f $(BUILD_DIR)/stub_*_*.bin
	rm -f $(BUILD_DIR)/embed_stub_*.o
	@if [ "$(HOST_ARCH)" = "arm64" ]; then \
	  rm -f $(STAGE0_ARM64_ELF) $(STAGE0_ARM64_BIN) $(STAGE0_ARM64_OFFSETS) $(STAGE0_ARM64_EMBED_O); \
	else \
	  rm -f $(STAGE0_X86_ELF) $(STAGE0_X86_BIN) $(STAGE0_X86_OFFSETS) $(STAGE0_X86_EMBED_O); \
	fi
	rm -f $(STUB_SRC)/stub_vars.h.bak
	rm -f $(BUILD_DIR)/build_info.h
	rm -f $(BUILD_DIR)/zelf

.PHONY: clean-keep-stubs
clean-keep-stubs:
	@printf "\033[37;44m❯ Cleaning (keeping all stubs + stage0)...\033[0m\n"
	rm -f $(BUILD_DIR)/start.o $(BUILD_DIR)/elfz_handoff_gate.o $(BUILD_DIR)/elfz_handoff_sequence.o
	rm -f $(BUILD_DIR)/stubs.mk
	rm -f $(BUILD_DIR)/*.elf
	rm -f $(BUILD_DIR)/*.o
	rm -rf $(BUILD_DIR)/stubs
	rm -f $(BUILD_DIR)/stub_*_*.bin
	rm -f $(BUILD_DIR)/embed_stub_*.o
	rm -f $(STUB_SRC)/stub_vars.h.bak
	rm -f $(BUILD_DIR)/build_info.h
	rm -f $(BUILD_DIR)/zelf

.PHONY: distclean
distclean: clean
	@printf "\033[37;43m✔ Cleaning complete\033[0m\n"

.PHONY: help
help:
	@printf "\n"
	@printf "Build options:\n"
	@printf "  STATIC=1                Build static packer (stubs are separate)\n"
	@printf "  FUSE_LD=auto|bfd|lld     Select linker for GCC (default: auto)\n"
	@printf "\n"
	@printf "Linker notes:\n"
	@printf "  auto  : prefers ld.lld if present, else ld.bfd if present, else system default ld\n"
	@printf "  bfd   : keeps stub LTO enabled (small stubs), ICF disabled\n"
	@printf "  lld   : stub LTO is disabled automatically for compatibility\n"
	@printf "\n"

# Créer les répertoires si nécessaire
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/stubs_%: | $(BUILD_DIR)
	mkdir -p $@



$(BUILD_DIR)/stubs: | $(STUBS_X86_DIR)
	@rm -rf $(BUILD_DIR)/stubs
	@ln -s stubs_x86_64 $(BUILD_DIR)/stubs

# Interactive menu target (runs tools/make_menu.sh)
.PHONY: menu
menu:
	@bash tools/make_menu.sh

.PHONY: tools
tools: $(TOOLS_BINARIES)
	@printf "\n         \033[97;40m._______________.\033[0m\n\033[97;40m ------==|\033[0m\033[97;48;5;166m ✔ Tools built \033[0m\033[97;40m|\033[0m\n         \033[97;40m'───────────────'\033[0m\n"

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
