############################################################
# LZMA + BCJ stubs (legacy minimal BCJ core + out-of-line decoder)
############################################################
## Moved below variable definitions to ensure $(BUILD_DIR) is defined at parse time

CC = gcc
LD = ld
NASM = nasm

# Static build toggle (usage: make STATIC=1)
STATIC ?= 0

# Parallelism for stubs (usage: make STUBS_J=4 stubs)
STUBS_J ?= 2

# Common flags for small tools
TOOLS_CFLAGS = -O2 -std=c99 -Wall -Wextra -Wshadow -march=x86-64 -mtune=native -fomit-frame-pointer -fstrict-aliasing -fno-asynchronous-unwind-tables
TOOLS_CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -Wshadow -march=x86-64 -mtune=native -fomit-frame-pointer -fstrict-aliasing -fno-asynchronous-unwind-tables
TOOLS_LDFLAGS =
ifeq ($(STATIC),1)
 TOOLS_LDFLAGS += -static -static-libgcc -static-libstdc++
endif

# Flags prod taille (stubs)
# Ensure 16-byte stack alignment for SSE (movaps) even if caller misaligned
STUB_PROD_CFLAGS = -fstrict-aliasing -march=x86-64 -mtune=native -mstackrealign -fno-plt -fno-math-errno -ffast-math -flto -I $(SRC_DIR)/filters -I $(SRC_DIR)/decompressors/lzav -I $(SRC_DIR)/decompressors/zx7b -I $(SRC_DIR)/decompressors/snappy -I $(SRC_DIR)/decompressors/quicklz -I $(SRC_DIR)/decompressors/exo -I $(SRC_DIR)/decompressors/doboz -I $(SRC_DIR)/decompressors/pp -I $(SRC_DIR)/decompressors/lzma -I $(SRC_DIR)/decompressors/zstd -I $(SRC_DIR)/decompressors/zstd/third_party/muzscat -I $(SRC_DIR)/decompressors/apultra -I $(SRC_DIR)/decompressors/lz4 -I $(SRC_DIR)/decompressors/shrinkler -I $(SRC_DIR)/decompressors/zx0 -I $(SRC_DIR)/decompressors/brieflz -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/decompressors/stonecracker -I $(SRC_DIR)/decompressors/lzsa/libs/decompression
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

PACKER_C = $(PACKER_SRC)/zelf_packer.c \
	$(PACKER_SRC)/elf_utils.c \
	$(PACKER_SRC)/stub_selection.c \
	$(PACKER_SRC)/filter_selection.c \
	$(PACKER_SRC)/compression.c \
	$(PACKER_SRC)/elf_builder.c \
	$(PACKER_SRC)/archive_mode.c \
	$(PACKER_SRC)/depacker.c \
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
	$(SRC_DIR)/compressors/shrinkler/shrinkler_comp.cpp \
	$(SRC_DIR)/compressors/doboz/doboz_compress_wrapper.cpp \
	$(SRC_DIR)/compressors/doboz/Compressor.cpp \
	$(SRC_DIR)/compressors/doboz/Dictionary.cpp \
	$(SRC_DIR)/compressors/lzav/lzav_compress.c \
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
    $(SRC_DIR)/compressors/brieflz/blz_compress_wrapper.c \
    $(SRC_DIR)/compressors/brieflz/brieflz.c \
    $(SRC_DIR)/compressors/stonecracker/sc_compress.c \
    $(SRC_DIR)/compressors/stonecracker/bitstream.c \
    $(SRC_DIR)/filters/bcj_x86_enc.c \
    $(SRC_DIR)/others/help_display.c

PACKER_BIN = $(BUILD_DIR)/zelf
PACKED_BIN = $(BUILD_DIR)/packed_binary
STUB_BINS_DIR = $(BUILD_DIR)

TOOLS_DIR = $(SRC_DIR)/tools
TOOLS_BIN_DIR = $(BUILD_DIR)/tools
TOOLS_BINARIES = $(TOOLS_BIN_DIR)/elfz_probe $(TOOLS_BIN_DIR)/filtered_probe

ifeq ($(DEBUG),1)
 STUB_CFLAGS = -c -O0 -g -DDEBUG=1 -nostdlib -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fno-builtin
 PACKER_CFLAGS = -O0 -g -DDEBUG=1 -D_7ZIP_ST -DQLZ_COMPRESSION_LEVEL=3 -DQLZ_STREAMING_BUFFER=0 -I $(SRC_DIR)/compressors/apultra -I $(SRC_DIR)/compressors/apultra/libdivsufsort/include -I $(SRC_DIR)/compressors/lzma/C -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/zstd/lib -I $(SRC_DIR)/compressors/zstd/lib/common -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/qlz -I $(SRC_DIR)/compressors/pp -I $(SRC_DIR)/compressors/lzav -I $(SRC_DIR)/compressors/snappy -I $(SRC_DIR)/compressors/zx7b -I $(SRC_DIR)/compressors/zx0 -I $(SRC_DIR)/compressors/doboz -I $(SRC_DIR)/compressors/exo -I $(SRC_DIR)/compressors/shrinkler -I $(SRC_DIR)/compressors/shrinkler/ref -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/compressors/stonecracker -I $(SRC_DIR)/others -I $(SRC_DIR)/filters
else
 STUB_CFLAGS = -c -O2 -nostdlib -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-builtin
 # Aggressive size optimization for packer binary
 PACKER_CFLAGS = -Os -flto=9 -fuse-ld=gold -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fomit-frame-pointer -ffunction-sections -fdata-sections -fvisibility=hidden -fno-plt -fmerge-all-constants -fno-ident -fno-exceptions -DNDEBUG \
 	-D_7ZIP_ST -DQLZ_COMPRESSION_LEVEL=3 -DQLZ_STREAMING_BUFFER=0 \
 	-I $(SRC_DIR)/compressors/apultra -I $(SRC_DIR)/compressors/apultra/libdivsufsort/include -I $(SRC_DIR)/compressors/lzma/C -I $(SRC_DIR)/compressors/zstd/lib -I $(SRC_DIR)/compressors/zstd/lib/common -I $(SRC_DIR)/compressors/lz4 -I $(SRC_DIR)/compressors/qlz -I $(SRC_DIR)/compressors/pp -I $(SRC_DIR)/compressors/lzav -I $(SRC_DIR)/compressors/snappy -I $(SRC_DIR)/compressors/zx7b -I $(SRC_DIR)/compressors/zx0 -I $(SRC_DIR)/compressors/doboz -I $(SRC_DIR)/compressors/exo -I $(SRC_DIR)/compressors/shrinkler -I $(SRC_DIR)/compressors/shrinkler/ref -I $(SRC_DIR)/compressors/brieflz -DBLZ_NO_LUT -I $(SRC_DIR)/compressors/stonecracker -I $(SRC_DIR)/compressors/lzsa/libs/compression -I $(SRC_DIR)/compressors/lzsa/compressor -I $(SRC_DIR)/compressors/lzsa/compressor/libdivsufsort/include -I $(SRC_DIR)/others -I $(SRC_DIR)/filters \
	-I $(SRC_DIR)/decompressors/zstd
endif
 # Linker flags tuned for size (keep functionality)
 PACKER_LDFLAGS = -lm -lstdc++ \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,--as-needed -Wl,-O1,--icf=all,--compress-debug-sections=zlib -Wl,--relax -Wl,--strip-all -s

# If requested, build packer and selected tools fully static
ifeq ($(STATIC),1)
 PACKER_LDFLAGS += -static -static-libgcc -static-libstdc++
 TOOLS_LDFLAGS += -static -static-libgcc -static-libstdc++
endif

# Cible par défaut
.PHONY: all static packer
all: stubs tools packer
	@echo "✅ Compilation terminée"

# Convenience alias to build static packer/tools/zelf
static:
	$(MAKE) STATIC=1 all
	@echo "✅ Compilation terminée"

# Compiler le stub
$(BUILD_DIR)/start.o: $(STUB_SRC)/start.S | $(BUILD_DIR)
	@echo "🔨 Compilation de start.S..."
	$(CC) -c $(STUB_SRC)/start.S -o $(BUILD_DIR)/start.o

$(BUILD_DIR)/elfz_handoff_gate.o: $(STUB_SRC)/elfz_handoff_gate.S | $(BUILD_DIR)
	@echo "🔨 Compilation de elfz_handoff_gate.S..."
	$(CC) -c $(STUB_SRC)/elfz_handoff_gate.S -o $(BUILD_DIR)/elfz_handoff_gate.o

$(BUILD_DIR)/elfz_handoff_sequence.o: $(STUB_SRC)/elfz_handoff_sequence.S | $(BUILD_DIR)
	@echo "🔨 Compilation de elfz_handoff_sequence.S..."
	$(CC) -c $(STUB_SRC)/elfz_handoff_sequence.S -o $(BUILD_DIR)/elfz_handoff_sequence.o

# Assemble Shrinkler ASM decompressor
$(BUILD_DIR)/shrinkler_decompress_x64.o: $(DECOMP_SHRINKLER_DIR)/shrinkler_decompress_x64.asm | $(BUILD_DIR)
	@echo "🛠️  Assemblage du décompresseur Shrinkler x64 (NASM)..."
	$(NASM) -f elf64 -O2 -o $@ $<

# --- GENERATION DES STUBS ---
# Inclure le makefile généré
-include $(BUILD_DIR)/stubs.mk

# Règle pour générer stubs.mk s'il n'existe pas ou si le script change
$(BUILD_DIR)/stubs.mk: tools/gen_stubs_mk.py | $(BUILD_DIR)
	@echo "⚙️  Génération de $(BUILD_DIR)/stubs.mk..."
	python3 tools/gen_stubs_mk.py > $@

# Cible stubs qui force la génération
.PHONY: stubs
stubs: $(BUILD_DIR)/stubs.mk
	$(MAKE) -f Makefile actual_stubs -j$(STUBS_J)

.PHONY: actual_stubs
actual_stubs: $(ALL_STUB_OBJS)

$(BUILD_DIR)/static_final_noreturn.o: $(STUB_SRC)/static_final_noreturn.S | $(BUILD_DIR)
	@echo " Compilation de static_final_noreturn.o..."
	$(CC) -c $(STUB_SRC)/static_final_noreturn.S -o $(BUILD_DIR)/static_final_noreturn.o

$(BUILD_DIR)/zstd_minidec.o: $(SRC_DIR)/decompressors/zstd/zstd_minidec.c | $(BUILD_DIR)
	@echo "🔨 Compilation de zstd_minidec.c..."
	$(CC) -I $(SRC_DIR)/decompressors/zstd/third_party/muzscat $(PACKER_CFLAGS) -c $< -o $@

$(PACKER_BIN): $(ALL_STUB_OBJS) $(PACKER_C) $(BUILD_DIR)/zstd_minidec.o $(BUILD_DIR)/shrinkler_decompress_x64.o | $(BUILD_DIR)
	$(CC) $(PACKER_CFLAGS) $(PACKER_C) $(ALL_STUB_OBJS) $(BUILD_DIR)/zstd_minidec.o $(BUILD_DIR)/shrinkler_decompress_x64.o -o $(PACKER_BIN) $(PACKER_LDFLAGS)
	@command -v sstrip >/dev/null 2>&1 && sstrip $(PACKER_BIN) || true
	@echo "🧹 Post-build: purge .o et symlinks dans $(BUILD_DIR)"
	rm -f $(BUILD_DIR)/*.o
	rm -f $(BUILD_DIR)/*.elf
	rm -f $(BUILD_DIR)/stub_*_*.bin
	rm -f $(BUILD_DIR)/stubs.mk

# Build only the packer. If stubs .bin are missing, build stubs first.
packer: | $(BUILD_DIR)
	@if [ -z "$(wildcard $(BUILD_DIR)/stubs/stub_*.bin)" ]; then \
	  echo "ℹ️  Stubs absents: génération des stubs avant le packer..."; \
	  $(MAKE) stubs; \
	fi
	$(MAKE) $(PACKER_BIN)

# Build the Kanzi .text benchmark tool
.PHONY: kanzi_text_bench
kanzi_text_bench: $(BUILD_DIR) $(FILTERS_DIR)/kanzi_exe_encode.cpp $(FILTERS_DIR)/kanzi_exe_encode_c.cpp $(SRC_DIR)/tools/kanzi_text_bench.c
	@echo "🔨 Building kanzi_text_bench..."
	$(CC) -O2 -DNDEBUG \
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
	@echo "🧹 Nettoyage..."
	rm -f $(BUILD_DIR)/start.o $(BUILD_DIR)/elfz_handoff_gate.o $(BUILD_DIR)/elfz_handoff_sequence.o $(BUILD_DIR)/upx_final.o $(BUILD_DIR)/upx_final_sequence.o
	rm -f $(BUILD_DIR)/stubs.mk
	rm -f $(BUILD_DIR)/*.elf
	rm -f $(BUILD_DIR)/*.o
	rm -f $(BUILD_DIR)/stubs/*.bin $(BUILD_DIR)/stub_*_*.bin
	rm -f $(BUILD_DIR)/embed_stub_*.o
	rm -f $(STUB_SRC)/stub_vars.h.bak
	@echo "✅ Nettoyage terminé"

.PHONY: distclean
distclean: clean
	@echo "✅ Nettoyage complet"

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
	@echo "✅ Tools built"

FORCE:

$(TOOLS_BINARIES): FORCE

$(TOOLS_BIN_DIR):
	mkdir -p $(TOOLS_BIN_DIR)

$(TOOLS_BIN_DIR)/elfz_probe: $(TOOLS_DIR)/elfz_probe.c | $(TOOLS_BIN_DIR)
	$(CC) $(TOOLS_CFLAGS) $(TOOLS_LDFLAGS) $< -lm -o $@

$(TOOLS_BIN_DIR)/filtered_probe: $(TOOLS_DIR)/filtered_probe.c | $(TOOLS_BIN_DIR)
	$(CXX) $(TOOLS_CXXFLAGS) \
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
