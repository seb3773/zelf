/*
 * help_display.c - zELF help/credits
 */

#include "help_display.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include "help_colors.h"
#include "help_display.h"
#include "../packer/zelf_packer.h"

#define VERSION ZELF_VERSION
#define ZELF_H1                                                                \
  FRED "★" FRED2 "===" FYW "[" GREY17 "ELF binary packer" FYW "]" FRED2        \
       "===" FRED "★"

static const char *logo_L0 = BCK "                             ";
static const char logo_version_line[] =
    BCK "           v" VERSION "             ";
static const char *f1 = FBK1 GORANGE
"═ " FBK2 GORANGE " " FBK3 GORANGE " " FBK4 GORANGE "═" FBK5 GORANGE
" " FBK6 GORANGE " " FBK7 GORANGE "═" FBK8 GORANGE "═" FBK9 GORANGE
" " FBK10 GORANGE "═" FBK11 GORANGE "═" FBK12 GORANGE " " FBK13 GORANGE
"═" FBK14 GORANGE "═══ " FBK15 GORANGE "════════════════" FBK16 GORANGE
"☾" FBK17 "○" FBK18 GORANGE "~" FBK19 "❖❖" FBK18 GORANGE "~" FBK17
"○" FBK16 GORANGE "☽" FBK15 GORANGE "═════════════════ " FBK14 GORANGE
"═══ " FBK13 GORANGE "══ " FBK12 GORANGE "══ " FBK11 GORANGE
" " FBK10 GORANGE "═ " FBK9 GORANGE "  " FBK8 GORANGE "═" RES;
static const char *h1 =
BCK FYW "╭────────────────────────────────────────────╮ " RES;
static const char *h2 = BCK FYW "│";
static const char *h2b = BCK FYW "│ " RES;
static const char *h3 =
BCK FYW "╰────────────────────────────────────────────╯ " RES;
static const char full_help_title_prefix[] =
BCK WHI10 "   " FBB "❂" FBB2 " Full Help:";
static const char algos_help_title_prefix[] =
BCK WHI10 "   " FBB4 "❖" FBK17 " Compression algorithms infos:";
static const char faq_title_prefix[] = BCK WHI10 "   " FYW "★" FYW " FAQ:";
static const char credits_title_prefix[] =
BCK WHI10 "   " FBB5 "✦" FBK17 " Credits & thanks: " GREY17
"(press a key to stop typewriter effect - ⎋:quit)";
static const char credits_title_manual_prefix[] =
BCK WHI10 "   " FBB5 "✦" FBK17 " Credits & acknowledgments: " GREY17
"(↑↓:scroll - ⎋/⏎:quit)";
static const char *const help_lines[] = {
    GRY FBB "●" FBB2 "Usage:",
    GRY WHI10 "  zelf" GRY GREY17 " [" FBK17 "options" GRY GREY17 "] [" FBB3
              "compressor" GRY GREY17 "] <" GRY WHI10 "ELF binary>",
    GRY WHI10 "",
    GRY18 FBB4 "●" FBK17 "Main Options:",
    GRY WHI10 " --help" GREY17 "         display this help and quit",
    GRY18 WHI10 " --help-full" GREY17 "    full help and misc options",
    GRY WHI10 " --help-algos" GREY17 "   compression algos infos",
    GRY18 WHI10 " --verbose" GREY17 "      detailled logs(analysis,...)",
    GRY WHI10 " --no-backup" GREY17 "    replace without backup",
    GRY18 WHI10 " --no-strip" GREY17 "     don't strip before packing",
    GRY WHI10 " --password" GREY17 "     password protected execution",
    GRY18 WHI10 " --archive" GREY17 "      archive mode",
    GRY WHI10 " --archive-tar" GREY17 "  archive directory (tar) mode",
    GRY WHI10 " --unpack" GREY17 "       unpack packed binary/archive",
    GRY18 WHI10 " --help-credits" GREY17 " credits for ideas and tools",
    GRY WHI10 "",
    GRY18 FBB5 "●" FBB3 "Compressor:",
    GRY WHI10 " -lz4" GREY17 " (LZ4, default)      " WHI10 "-snappy" GREY17
              " (Snappy)",
    GRY18 WHI10 " -apultra" GREY17 " (Apultra)       " WHI10 "-doboz" GREY17
                " (Doboz)",
    GRY WHI10 " -zx7b" GREY17 "  (ZX7b)            " WHI10 "-qlz" GREY17
              " (QuickLZ)",
    GRY18 WHI10 " -exo" GREY17 " (Exomizer)          " WHI10 "-lzav" GREY17
                " (LZAV)",
    GRY WHI10 " -pp" GREY17 " (PowerPacker-PP20)   " WHI10 "-lzma" GREY17
              " (LZMA)",
    GRY18 WHI10 " -zstd" GREY17 " (fb zstandard)     " WHI10 "-shr" GREY17
                " (Shrinkler)",
    GRY WHI10 " -zx0" GREY17 " (ZX0)               " WHI10 "-lzsa" GREY17
              " (LZSA2)",
    GRY WHI10 " -stcr" GREY17 " (StoneCracker)     " WHI10 "-blz" GREY17
              " (BriefLZ)",
    GRY18 WHI10 " -dsty" GREY17 " (Density)          " WHI10 "-lzham" GREY17
                " (LZHAM)",
    GRY WHI10 " -rnc" GREY17 " (RNC)               " WHI10 "-csc" GREY17
              " (CSArc)",
    GRY18 WHI10 " -nz1" GREY17 " (NanoZip)           " WHI10 "-lzfse" GREY17
              " (LZFSE)",
    GRY WHI10 " -best" GREY17 "  Try the best predicted combinations",
    GRY WHI10 GREY17 "        (can be very slow)"};

static const char *const full_help_lines[] = {
    GRY WHI10
    "zELF is an executable packer targeting x86_64 ELF binaries (Linux),",
    GRY WHI10
    "providing multiple compression formats, including some exotic ones, for",
    GRY WHI10
    "a variety of use cases. The main goal is to produce a smaller binary",
    GRY WHI10
    "with the same functionality as the original: when the packed binary is",
    GRY WHI10
    "executed, a stub (small code segment) decompresses the program in memory",
    GRY WHI10
    "and launches it. The induced overhead remains minimal (depends on the",
    GRY WHI10
    "compression algorithm used), and everything else behaves exactly like",
    GRY WHI10
    "the execution of the original binary. Contrary to common misconceptions",
    GRY WHI10
    "about such tools, it does not prevent resources sharing nor increase",
    GRY WHI10 "memory usage in any way (except very briefly during in-memory",
    GRY WHI10 "decompression).",
    GRY18 WHI10
    "The packer can also protect the execution of the resulting binary with",
    GRY18 WHI10
    "a password (Note that this is NOT encryption or a method to protect the",
    GRY18 WHI10
    "code itself—zELF is not designed for that purpose. It simply prevents",
    GRY18 WHI10 "execution unless the correct password is provided.)",
    GRY WHI10 "",
    GRY18 WHI10 "➤Full command line usage:",
    GRY WHI10 "  zelf" GRY GREY17 " [" FBK17 "options" GRY GREY17 "] [" FBB3
              "compressor" GRY GREY17 "] <" GRY WHI10 "ELF binary>",
    GRY WHI10 " ●" GREY17 " Available [" FBK17 "options" GRY GREY17 "]:",
    GRY18 FBK17 " --verbose",
    GRY18 GREY17
    "  Show detailed logs during packing (analysis, compression steps, etc.)",
    GRY FBK17 " --no-backup",
    GRY GREY17 "  Overwrite the original binary without creating a backup copy",
    GRY18 FBK17 " --no-strip",
    GRY18 GREY17
    "  Keep debug symbols and metadata; skip the usual 'strip' step",
    GRY FBK17 " --output<File_path>",
    GRY GREY17 "  write packed binary as <File_path>",
    GRY18 FBK17 " --password",
    GRY18 GREY17
    "  Require a password to run the packed binary (not encryption, just",
    GRY18 GREY17 "  access control)",
    GRY FBK17 " --exe-filter=bcj|kanziexe|none",
    GRY GREY17
    "  Force usage of bcj or kanziexe filter instead of automatic choice.",
    GRY GREY17
    "  Automatic choice is based on a predictor tuned for every algorithm,",
    GRY GREY17
    "  but in rare cases it can select the non optimal filter for best ratio.",
    GRY18 FBK17 " --help-full",
    GRY18 GREY17 "  This help :-p",
    GRY FBK17 " --help-algos",
    GRY GREY17
    "  Description and information about compression algorithms availables",
    GRY18 FBK17 " --no-colors",
    GRY18 GREY17 "  disable colors usage for terminal output",
    GRY FBK17 " --archive",
    GRY GREY17 "  Create a .zlf archive from a file",
    GRY18 FBK17 " --archive-tar",
    GRY18 GREY17 "  Create a .tar.zlf archive from a directory (tar + compress)",
    GRY FBK17 " --unpack",
    GRY GREY17
    "  Restore the original binary by decompressing the packed version,",
    GRY GREY17 "  or unpack .zlf/.tar.zlf archive (auto-extract tar.zlf archives)",
    GRY18 FBK17 " --no-progress",
    GRY18 GREY17
    "  Don't display progress bar when compressing, to avoid garbage when",
    GRY18 GREY17 "  redirecting output to a file",
    GRY FBK17 " --help-faq",
    GRY GREY17 "  Classic \"frequently asked questions\" section",
    GRY FBK17 " --help-credits",
    GRY GREY17 "  Show acknowledgments for tools, ideas, and code used in zELF",
    GRY WHI10 "",
    GRY18 WHI10 "➤Algorithm choice for " GREY17 "[" FBB3 "compressor" GRY GREY17
                "]:",
    GRY GREY17 " [" FBB3 "compressor" GRY GREY17 "] can be:",
    GRY WHI10
    "-lz4" GREY17 "|" WHI10 "-snappy" GREY17 "|" WHI10 "-apultra" GREY17 "|" WHI10 "-doboz" GREY17 "|" WHI10 "-zx7b" GREY17 "|" WHI10 "-exo" GREY17 "|" WHI10 "-lzav" GREY17 "|" WHI10 "-pp" GREY17 "|" WHI10 "-lzma" GREY17 "|" WHI10 "-zstd" GREY17 "|" WHI10 "-zx0",
    GRY WHI10 "-shr" GREY17 "|" WHI10 "-stcr" GREY17 "|" WHI10 "-blz" GREY17 "|" WHI10 "-lzham" GREY17 "|" WHI10 "-dsty" GREY17 "|" WHI10 "-rnc" GREY17 "|" WHI10 "-csc" GREY17 "|" WHI10 "-nz1" GREY17 "|" WHI10 "-lzfse" GREY17 "|" WHI10 "-best" GREY17 "|" WHI10 "-crazy",
    GRY WHI10 "",
    GRY18 WHI10
    "\"-best\" will try to pack the binary with the best compression ratio",
    GRY18 WHI10
    "predicted algorithms, with every filter combination, so this mode can be",
    GRY18 WHI10
    "very slow, especially for large binaries (but it won't affect the",
    GRY18 WHI10
    "decompression time in any way).",
    GRY WHI10
    "\"-crazy\" will try to pack the binary with EVERY possible combination",
    GRY WHI10
    "(filters and codecs) so this mode IS very very slow.",
    GRY WHI10 "",
    GRY18 WHI10 "The choice should be based on your specifics constraints:",
    GRY18 WHI10 "-Maximum decompression speed:" GREY17
              " Use algorithms like LZ4, Snappy, or Doboz",
    GRY18 GREY17 "for very fast runtime performance with minimal CPU overhead.",
    GRY WHI10 "-Best compression ratio:" GREY17
                " LZMA, EXO, Shrinkler provide the highest",
    GRY GREY17
    "compression suitable when minimizing binary size is the top priority,",
    GRY GREY17 "though they come with slower decompression.",
    GRY18 WHI10 "-Balanced performance:" GREY17
              " Apultra, QuickLZ, ZX7b, ZX0 offer a good",
    GRY18 GREY17
    "compromise between compression ratio, decompression speed, and stub size.",
    GRY WHI10 "-Stub size constraints:" GREY17
                " If minimizing the decompression stub is",
    GRY GREY17
    "critical,prefer LZ4,ZX7b,QuickLZ,BriefLZ or Doboz over heavier options.",
    GRY18 WHI10 "",
    GRY18 WHI10
    "# The optimal choice depends on whether your priority is decompression",
    GRY18 WHI10
    "speed, minimal size, low memory usage, or a balance of these factors.",
    GRY18 WHI10 "",
    GRY18 WHI10 " " FRED "✱" WHI10
              " Please note that zELF is not designed for code obfuscation or",
    GRY18 WHI10
    "concealment. All source code is openly available, and even the more",
    GRY18 WHI10
    "exotic compression format can be integrated and parsed by any analysis",
    GRY18 WHI10
    "tool thanks to the provided sources. The goal of this packer is binary",
    GRY18 WHI10
    "size reduction and efficient runtime unpacking—not evasion of antivirus",
    GRY18 WHI10 "software or distribution of malicious code. It is intended for",
    GRY18 WHI10
    "legitimate use cases such as embedded systems, demos, general binary",
    GRY18 WHI10
    "optimization or educational purposes like studying ELF structure and",
    GRY18 WHI10 "advanced packing techniques."};

static const char *const help_algos_lines[] = {
    GRY WHI10 "",
    GRY WHI10
    " ●LZ4" GREY17
    ":the smallest and fastest (~850 MB/s) algorithm in zELF, offering",
    GRY GREY17 "  efficiency and performance, though it achieves only moderate",
    GRY GREY17 "  compression ratio.",
    GRY18 WHI10 " ●Snappy" GREY17
                ":an industrial Google format, very fast, offers a compression",
    GRY18 GREY17 "  ratio similar to LZ4 with a slighty larger stub.",
    GRY WHI10 " ●Apultra" GREY17
              ":based on aPLib format, very good compression ratio with fast",
    GRY GREY17 "  decompression (~700 MB/s) and a reasonable stub size.",
    GRY18 WHI10
    " ●Doboz" GREY17
    ":offers an excellent size-to-ratio compromise with a reasonable",
    GRY18 GREY17 "  stub size and very fast decompression performance.",
    GRY WHI10 " ●ZX7b" GREY17
              ":backwards LZ77/LZSS compression format originally designed for",
    GRY GREY17 "  ZX Spectrum systems, excellent compression ratio with fast",
    GRY GREY17 "  decompression and very small stub.",
    GRY18 WHI10 " ●QuickLZ" GREY17
                ":provides an excellent size-to-ratio balance with good",
    GRY18 GREY17 "  compression ratio, requires no external RAM, and maintains",
    GRY18 GREY17 "  solid overall efficiency with a small stub.",
    GRY WHI10 " ●Exomizer" GREY17
              ":originally designed for 8-bit computers (C64, MSX...), it",
    GRY GREY17
    "  offers a very good compression ratio, fast decompression, minimal",
    GRY GREY17
    "  memory footprint, making it ideal for resource-limited environments.",
    GRY18 WHI10 " ●LZAV" GREY17
                ":fast algorithm based on LZ77, optimized to provide a solid",
    GRY18 GREY17 "  balance between speed and moderate compression ratio.",
    GRY WHI10 " ●PowerPacker-PP20" GREY17
              ":classic file compression utility for Amiga computers,",
    GRY GREY17
    "  widely used in the late 80s and 90s, known for good ratio with fast",
    GRY GREY17 "  decompression and small runtime overhead.",
    GRY18 WHI10
    " ●LZMA" GREY17
    ":highest compression ratio of all, known to be widely used in 7z",
    GRY18 GREY17
    "  archives, but with slow decompression compared to other algorithms.",
    GRY WHI10
    " ●ZSTD" GREY17
    ":Zstandard from facebook provides a high compression ratio while",
    GRY GREY17
    "  maintaining high decompression speed, but with largest stubs.",
    GRY18 WHI10 " ●Shrinkler" GREY17
                ":high-performance compression tool originally designed for",
    GRY18 GREY17
    "  Amiga 4k intros, delivering excellent compression ratios through an",
    GRY18 GREY17
    "  lzma like algorithm and highly optimized asm decompression routine.",
    GRY WHI10
    " ●ZX0" GREY17
    ":designed initially for 8-bit computers, such as the ZX Spectrum,",
    GRY GREY17
    "  uses an approach based on searching for repeated patterns in the data,",
    GRY GREY17
    "  like LZ77, but with specific optimizations to reduce the size of the",
    GRY GREY17
    "  decompression code and the memory required during decompression.",
    GRY18 WHI10
    " ●BriefLZ" GREY17
    ":small and fast Lempel-Ziv based algorithm designed to balance",
    GRY18 GREY17
    "  speed, ratio and code size. It offers reasonable compression with a",
    GRY18 GREY17
    "  very compact decompression routine, making it ideal for use in",
    GRY18 GREY17
    "  microcontrollers and other embedded or resource-constrained systems.",
    GRY WHI10 " ●StoneCracker" GREY17
              ":compression tool used primarily on the Amiga platform,",
    GRY GREY17
    "  known for its ability to compress executable code into a compact",
    GRY GREY17
    "  format while maintaining quick decompression speed. It became popular",
    GRY GREY17
    "  in the demo scene for its ratio and performance on 68k systems.",
    GRY18 WHI10
    " ●LZSA2" GREY17
    ":high-compression tool set at level 9, particularly suitable for",
    GRY18 GREY17
    "  compressing executable code, as it uses advanced compression techniques",
    GRY18 GREY17
    "  to achieve superior compression ratios beyond standard LZ methods.",
    GRY18 GREY17
    "  It is optimized for scenarios where maximum compression is needed.",
    GRY WHI10 " ●LZHAM" GREY17
              ":lossless data compression codec with good compression ratio",
    GRY GREY17
    "  and good decompression speed, designed to provide LZMA-like ratios",
    GRY GREY17 "  with significantly faster decompression performance.",
    GRY18 WHI10 " ●RNC" GREY17
              ":compression format developed by Rob Northen in 1991,combining",
    GRY18 GREY17
    "  LZSS and Huffman coding techniques,widely used in late-era",
    GRY18 GREY17 "  Mega Drive,Atari Lynx & Amiga games by US and UK developers.",

    GRY WHI10 " ●LZFSE" GREY17
              ":Apple's high-performance compression algorithm that combines",
    GRY GREY17
    "  Lempel-Ziv parsing with Finite State Entropy encoding for best ratios",
    GRY GREY17 "  and rapid decompression. Developed for platforms like iOS and macOS,",
    GRY GREY17 "  it excels in file archiving and system-level data handling.",
    GRY18 WHI10 " ●CSC" GREY17
              ":CSArc is Siyuan Fu's compressor inspired by LZMA,using LZ77 with",
    GRY18 GREY17
    "  range coding, advanced match finders, and adaptive preprocessors like",
    GRY18 GREY17 "  e8e9 or delta. It delivers competitive compression ratios similar",
    GRY18 GREY17 "  to LZMA, prioritizing fast decompression through efficient range",
    GRY18 GREY17 "  decoding and optimized match finding.",
    GRY WHI10 " ●NanoZip 1" GREY17
              ":minimalist, dependency-free C compression library optimized",
    GRY GREY17
    "  for embedded systems and high-performance use, achieving good ratios",
    GRY GREY17 "  on repeating data and very good speed for compression/decompression",
    GRY18 WHI10 ""};

static const char *const help_faq_lines[] = {
    GRY WHI10 "",
    GRY WHI10
    " 1. Why create a “new” packer for Linux ELF executables when UPX already",
    GRY WHI10 "    works perfectly and is widely used?",
    GRY18 GREY17
    "UPX is a fantastic tool—an exceptional piece of engineering—and was the",
    GRY18 GREY17
    "main inspiration behind zELF. The goal of zELF is not to compete with",
    GRY18 GREY17
    "UPX, but to offer an alternative that is easily extensible. Adding a new",
    GRY18 GREY17
    "compression algorithm is straightforward, and a dedicated guide explains",
    GRY18 GREY17 "how to do it. zELF also serves an educational purpose:",
    GRY18 GREY17
    "it provides insight into the inner workings of ELF binaries and the",
    GRY18 GREY17
    "techniques used to manipulate them. The code is clean, well-documented,",
    GRY18 GREY17
    "and can serve as a foundation for any project. It’s released under a",
    GRY18 GREY17
    "GPLv3 license. I also wanted a packer I fully understand and control,",
    GRY18 GREY17
    " tailored to my own binaries.",
    GRY18 GREY17
    "This gave me the opportunity to integrate historical compression",
    GRY18 GREY17
    "algorithms from the 8/16-bit world I’ve encountered over the years.",
    GRY WHI10 "",
    GRY WHI10
    " 2. Why include exotic algorithms like Exomizer, PowerPacker, ZX7b, etc.?",
    GRY18 GREY17 "The first reason is simple: for fun!",
    GRY18 GREY17
    "The deeper reason is that I come from the 8/16-bit world—MSX, Amiga,",
    GRY18 GREY17
    "C64, Atari. These are the machines that made me fall in love with",
    GRY18 GREY17
    "computing. I have a strong nostalgic connection to that era of tight",
    GRY18 GREY17
    "constraints and CPUs running at just a few MHz. Optimization wasn’t",
    GRY18 GREY17
    "optional—it was essential. Some compression algorithms developed",
    GRY18 GREY17
    "under those conditions are true technical achievements. Integrating",
    GRY18 GREY17 "them into a modern tool is a way to honor their ingenuity.",
    GRY18 GREY17
    "Their philosophy aligns perfectly with the goals of executable packing:",
    GRY18 GREY17
    "small code, fast decompression, and impressive ratios. I thoroughly",
    GRY18 GREY17 "enjoyed adapting and optimizing them for x86.",
    GRY WHI10 "",
    GRY WHI10
    " 3. Aren’t these “exotic” formats likely to evade antivirus detection?",
    GRY18 GREY17
    "No. The formats are open-source and extensively documented. There’s no",
    GRY18 GREY17
    "mystery in decompressing the payloads generated by zELF, and any of",
    GRY18 GREY17
    "these algorithms can be easily integrated into other projects.",
    GRY18 GREY17
    "The format itself is simple and transparent—clearly marked compressor",
    GRY18 GREY17
    "types, headers, and data layout. Moreover, zELF performs no encryption.",
    GRY18 GREY17
    "It’s not designed for concealment. The only protection feature is",
    GRY18 GREY17
    "optional password-based execution control, which does not encrypt or",
    GRY18 GREY17 "hide the code in any way.",
    GRY WHI10 "",
    GRY WHI10
    " 4. Is zELF compatible with all types of ELF binaries (static, dynamic,",
    GRY WHI10 "    PIE, etc.)?",
    GRY18 GREY17 "Yes, all valid ELF x86_64 binary types are supported.",
    GRY WHI10 "",
    GRY WHI10
    " 5. Does zELF work on all architectures (x86, x86_64, ARM, etc.)?",
    GRY18 GREY17
    "Currently, zELF targets Linux x86_64 only. An ARM64 version may be",
    GRY18 GREY17
    "developed in the future, and a Windows x86_64 port is also being",
    GRY18 GREY17 "considered—though with no clear timeline.",
    GRY WHI10 "",
    GRY WHI10
    " 6. Can multiple compression layers be chained (multi-layer packing)?",
    GRY18 GREY17
    "Technically, yes—nothing prevents you from applying multiple layers of",
    GRY18 GREY17
    "compression. However, this is unlikely to improve the overall compression",
    GRY18 GREY17
    "ratio. It will simply stack multiple decompression stubs, and",
    GRY18 GREY17
    "re-compressing already compressed data rarely yields better results.",
    GRY18 GREY17
    "So while valid, this approach offers little/no practical benefit.",
    GRY WHI10 "",
    GRY WHI10 " 7. What is the typical size of the stub added by zELF?",
    GRY18 GREY17
    "Stub size depends on the chosen compression algorithm and ranges from",
    GRY18 GREY17
    "1.5 KB up to 22 KB for ZSTD.This means,for example,that ZSTD is not ideal",
    GRY18 GREY17
    "for very small binaries, but its excellent compression ratio makes it",
    GRY18 GREY17
    "a good choice for larger binaries, where stub size becomes negligible",
    GRY18 GREY17 "compared to the space saved.",
    GRY WHI10 "",
    GRY WHI10 " 8. What are the BCJ and KanziEXE filters ?",
    GRY18 GREY17
    "These are pre-processing/post-processing filters designed to transform",
    GRY18 GREY17
    "code sections in order to enhance compressibility. BCJ is well-known",
    GRY18 GREY17 "and widely adopted, notably in UPX.",
    GRY18 GREY17
    "KanziEXE, on the other hand, is the executable-data filter used in the",
    GRY18 GREY17
    "Kanzi compressor. It has been extracted, with the decoding code converted",
    GRY18 GREY17
    "into a pure, optimized C implementation, and in some cases it achieves",
    GRY18 GREY17
    "a superior final compression ratio compared to BCJ, despite requiring a",
    GRY18 GREY17
    "slightly larger stub. Filter selection is performed automatically by a",
    GRY18 GREY17
    "refined predictor, tailored for each compression algorithm, and trained",
    GRY18 GREY17
    "on a corpus of more than 2,000 diverse ELF binaries. Full info about the",
    GRY18 GREY17
    "process and datasets csv used to train the predictor are provided in doc.",
    GRY WHI10 "",
    GRY WHI10 " 9. Can I easily add my own compression algorithm ?",
    GRY18 GREY17
    "Absolutely! That’s one of the core goals of the project: to make it easy",
    GRY18 GREY17
    "to integrate any compression algorithm for specific use cases.",
    GRY18 GREY17
    "A complete guide is available to walk you through adding a new",
    GRY18 GREY17
    "compressor. If the process feels too complex, but you have an algorithm",
    GRY18 GREY17
    "you believe would be a good fit for zELF, feel free to suggest it—along",
    GRY18 GREY17
    "with source references—and I’ll consider adding it 'officially' " FBB4
    " :-)",
    GRY WHI10 "",
    GRY WHI10 " 10. Why the name 'zELF' ?",
    GRY18 GREY17
    "'ELF' is self-explanatory.The 'z' evokes compression (as in zlib,LZx,...)",
    GRY18 GREY17 "I just think it sounds good—and that’s reason enough ^^",
    GRY WHI10 "",
    GRY WHI10 " 11. What type of license does zELF use ?",
    GRY18 GREY17
    "'zELF is released under the TFYW (The Fuck You Want) license. ",
    GRY18 GREY17
    "However, you must comply with the licensing terms of the integrated",
    GRY18 GREY17
    "components, such as compression algorithms, filters, and any external",
    GRY18 GREY17
    "code included within the tool itself.In other words,while zELF itself is",
    GRY18 GREY17 "under TFYW licence, the bundled third‑party code — even when",
    GRY18 GREY17
    "converted, adapted, or optimized for specific use within zELF — remains",
    GRY18 GREY17 "subject to its respective licenses, which must be respected.",
    GRY18 GREY17 "",
    GRY WHI10 " 12. Is zELF stable enough for production use ?",
    GRY18 GREY17
    "Probably... I use it daily on binaries packed with various supported ",
    GRY18 GREY17
    "algorithms and haven’t encountered any issues so far. If you do run into ",
    GRY18 GREY17 "bugs, please report them—I’d really appreciate it.",
    GRY WHI10 "",
    GRY WHI10 " 13. And if I have other questions not covered here ?",
    GRY18 GREY17 "You can reach me on GitHub: https://github.com/seb3773",
    GRY18 GREY17
    "I am also often present on the Q4OS forum (an excellent Linux",
    GRY18 GREY17 "distribution based on Debian).",
    GRY18 GREY17 ""};

static const char *const help_credits_lines[] = {
    GRY WHI10 "",
    GRY WHI10 "zELF would not have been possible without the code developed",
    GRY WHI10 "by these authors, whether it be code adapted/converted",
    GRY WHI10
    "to C, or essential ideas/techniques that enabled the development",
    GRY WHI10 "of this ELF packer; so, I would really like to thank all these",
    GRY WHI10 "authors for their work, and for the invaluable inspiration:",
    GRY WHI10 "",
    GRY FBB "                                ★★★",
    GRY WHI10 "",
    GRY WHI10 "apultra: Emmanuel Marty",
    GRY GREY17 "    https://github.com/emmanuel-marty/apultra",
    GRY18 WHI10 "BriefLZ: Joergen Ibsen",
    GRY18 GREY17 "    https://github.com/jibsen/brieflz",
    GRY WHI10 "Doboz: Attila T. Afra",
    GRY GREY17 "    https://github.com/nemequ/doboz",
    GRY18 WHI10 "Exomizer: Magnus (lft) Lind",
    GRY18 GREY17 "    https://github.com/bitshifters/exomizer",
    GRY WHI10 "LZ4: Yann Collet",
    GRY GREY17 "    https://github.com/lz4/lz4",
    GRY18 WHI10 "LZAV: Aleksey Vaneev (avaneev)",
    GRY18 GREY17 "    https://github.com/avaneev/lzav",
    GRY WHI10 "LZMA: Igor Pavlov",
    GRY GREY17 "    http://www.7-zip.org",
    GRY18 WHI10 "LZSA: Emmanuel Marty",
    GRY18 GREY17 "    https://github.com/emmanuel-marty/lzsa",
    GRY WHI10 "PowerPacker: Nico François / Stuart Caie",
    GRY GREY17 "    http://justsolve.archiveteam.org/wiki/PowerPacker",
    GRY18 WHI10 "QuickLZ: Lasse Mikkel Reinhold",
    GRY18 GREY17 "    http://www.quicklz.com",
    GRY WHI10 "Shrinkler: Aske Simon (Blueberry) Christensen / Loonies",
    GRY18 WHI10 "Snappy: Google / S.Melikhov",
    GRY18 GREY17 "    https://github.com/google/snappy",
    GRY WHI10 "StoneCracker:  StoneCracker (“STC”)",
    GRY18 WHI10 "zstd: Yann Collet",
    GRY18 GREY17 "    https://github.com/facebook/zstd",
    GRY WHI10 "zx0: Einar Saukas",
    GRY GREY17 "    https://github.com/emmanuel-marty/salvador",
    GRY18 WHI10 "zx7b: Antonio José Villena Godoy / Einar Saukas",
    GRY18 GREY17 "    https://github.com/antoniovillena/zx7b",
    GRY WHI10 "RNC: Rob Northen",
    GRY GREY17 "    https://github.com/lab313ru/rnc_propack_source",
    GRY18 WHI10 "LZHAM: Rich Geldreich",
    GRY18 GREY17 "    https://github.com/richgel999/lzham_codec",
    GRY WHI10 "Density: Guillaume Voirin (g1mv)",
    GRY GREY17 "    https://github.com/g1mv/density",
    GRY18 WHI10 "LZFSE: Apple Inc.",
    GRY18 GREY17 "    https://github.com/lzfse/lzfse",
    GRY WHI10 "CSArc: Siyuan Fu",
    GRY GREY17 "    https://github.com/fusiyuan2010/CSC",
    GRY18 WHI10 "NanoZip 1: Ferki",
    GRY18 GREY17 "    https://github.com/Ferki-git-creator/NZ1",
    GRY WHI10 "UCL: Markus F.X.J. Oberhumer",
    GRY GREY17 "    http://www.oberhumer.com/opensource/ucl/",
    GRY18 WHI10 "sstrip: David Madore / aunali1",
    GRY18 GREY17 "    https://github.com/aunali1/super-strip",
    GRY WHI10 "BCJ Filter:  Igor Pavlov / Lasse Collin",
    GRY GREY17
    "    https://github.com/torvalds/linux/blob/master/lib/xz/xz_dec_bcj.c",
    GRY18 WHI10 "KanziEXE Filter: Frederic Langlet (flanglet)",
    GRY18 GREY17
    "    filter extracted from project https://github.com/flanglet/kanzi-cpp",
    GRY WHI10 "libdivsufsort: Yuta Mori",
    GRY GREY17 "    https://github.com/y-256/libdivsufsort",
    GRY18 WHI10 "",
    GRY18 WHI10
    "If, for any reason, I have overlooked you, please don’t hesitate",
    GRY18 WHI10 "to reach out so I can correct this omission.",
    GRY18 WHI10 "(The work on this packer has spanned several months, very,",
    GRY18 WHI10 "intermittently, as I’ve been developing it in my spare time.",
    GRY18 WHI10
    "So, it’s always possible that I may have used elements of your",
    GRY18 WHI10 "code without noting the author at the time :-p )",
    GRY18 WHI10 "",
    GRY FBB4 "                                ★ ★ ★",
    GRY WHI10 "",
    GRY WHI10 "A HUGE thank you to Markus F.X.J. Oberhumer, László Molnár,",
    GRY WHI10
    "and John F. Reiser, the brilliant minds behind UPX—the gold standard",
    GRY WHI10 "of packers.",
    GRY WHI10
    "Thank you for this marvel of engineering, which allowed me to grasp the",
    GRY WHI10
    "principles of ELF packing and attempt to replicate them as faithfully as",
    GRY WHI10
    "possible in zELF. Without these developers, this packer simply would",
    GRY WHI10
    "not exist. Thank you for your work and for being an invaluable source",
    GRY WHI10 "of inspiration.",
    GRY WHI10 "",
    GRY18 FBB5 "                                  ❂ ❂",
    GRY18 WHI10 "",
    GRY18 WHI10
    "A special thanks also goes to the authors of the 8/16-bit scene packers.",
    GRY18 WHI10
    "Integrating your brilliant algorithms—written so many years ago with",
    GRY18 WHI10 "ingenuity for particularly 'constrained' systems with limited",
    GRY18 WHI10
    "resources—is my way of paying tribute to you and to that era when",
    GRY18 WHI10
    "computing and the approach to development were a ~little~ different from",
    GRY18 WHI10
    "today... Thank you, too, for inspiring my dreams as a teenager with your",
    GRY18 WHI10 "demos and other unforgettable creations.",
    GRY18 WHI10 "",
    GRY18 WHI10 "",
    GRY18 FBB "                                ☾○~❖❖~○☽",
    GRY18 WHI10 "",
    GRY18 WHI10 "              GPLv3 - seb3773 - https://github.com/seb3773",
    GRY WHI10 ""};

static const char *const logo_lines[] = {
BCK BK0 "        " BCK2 BK "▄" BGR1 GR "▄" BCK3 BK11 "▄" BCK BK11
"▄" BCK GR11 "▄" BCK GR11 "▄" BCK GR6 "▄" BCK GR7 "▄" BCK BK0
"▄" BCK BK0 "▄▄▄▄▄▄▄▄▄▄▄▄",
BCK BK0 "         " BCK BK0 "▄" BCK GR9 "▄" BCK7 GR4 "▄" BGR2 GR10
"▄" BGR10 GR11 "▄" BGR10 GR10 "▄" BGR12 GR10 "▄" BCK8 GR11
"▄" BCK BK5 "▄" BCK BK0 "▄▄▄▄▄▄▄▄▄▄▄",
BCK BK0 "          " BCK8 BK17 "▄" BGR12 BK11 "▄" BGR2 BK "▄" BGR2 BK
"▄▄▄" BGR10 BK "▄" BGR2 BK11 "▄" BCK10 BK17 "▄" BCK BK0 "▄▄" BCK BK0
"▄" BCK BK0 "▄▄▄▄▄▄▄",
BCK BK0 "      " BYW1 YW21 "▄" BCK11 YW3 "▄" BCK BK13 "▄" BGR8 BK9
"▄" BGR2 BK "▄" BGR10 BK0 "▄▄▄" BGR10 BK0 "▄" BGR12 BK0
"▄" BGR12 BK0 "▄" BGR13 BK11 "▄" BCK8 BK9 "▄" BCK BK0 "▄" BCK YW34
"▄" BYW2 YW16 "▄" BCK12 BK21 "▄" BCK BK0 "▄▄▄▄▄▄",
BCK BK0 "      " BYW12 YW16 "▄" BYW4 GR15 "▄" BYW5 YW12 "▄" BCK BK0
"▄" BCK13 GREY "▄" GRY2 GREY18 "▄" GRY10 GREY28 "▄" BCK15 GREY13
"▄" BCK15 GREY20 "▄" GRY10 GREY22 "▄" BGR15 GREY16 "▄" BCK13 GREY21
"▄" BCK BK0 "▄" BYW39 YW34 "▄" BYW7 BK25 "▄" BYW7 YW16
"▄" BCK17 BK14 "▄" BCK BK0 "▄▄▄▄▄▄",
BCK BK0 "      " BYW9 "▄" BYW10 YW10 "▄" BYW11 YW13 "▄" BCK BK0
"▄" GRY17 BK0 "▄" GRY5 BK16 "▄" BCK13 GR13 "▄" GRY6 YW19
"▄" GRY7 YW19 "▄" BCK15 GR23 "▄" GRY8 BK39 "▄" GRY18 BK0 "▄" BCK BK0
"▄" BYW12 GR22 "▄" BYW13 GR21 "▄" BCK20 BK0 "▄" BCK BK0 "▄▄▄▄▄▄▄",
BCK BK0 "          ▄" BYW14 GR16 "▄" BYW15 YW34 "▄" BYW16 GR16
"▄" BYW17 GR16 "▄" BYW18 YW31 "▄" BYW14 GR15 "▄" BCK28 BK0
"▄" BCK BK0 "▄▄▄▄▄▄▄▄▄▄▄",
BCK BK0 "          " BK29 "▄" BCK13 GREY10 "▄" BYW22 GREY2 "▄" BYW12 BK19
"▄" BYW24 BK19 "▄" BCK20 GREY7 "▄" BCK13 GREY11 "▄" BCK GREY2
"▄" BCK BK0 "▄" BCK BK0 "▄▄▄▄▄▄▄▄▄▄",
BCK BK0 "       ▄" BK18 "▄" BCK GREY15 "▄" GRY10 WHI12 "▄" BWHI1 WHI12
"▄" BWHI1 WHI12 "▄" BWHI1 WHI12 "▄" BWHI1 WHI12 "▄" BWHI1 WHI12
"▄" BWHI1 WHI12 "▄" BWHI10 WHI12 "▄" BCK13 GREY24 "▄" BCK BK18
"▄" BCK BK41 "▄" BCK BK0 "▄▄▄▄▄▄▄▄",
BCK BK0 "       " BCK21 BK27 "▄" BCK15 BK10 "▄" GRY20 WHI3 "▄" BWHI5 WHI12
"▄" BWHI1 WHI12 "▄" BWHI1 WHI12 "▄" BWHI1 WHI12 "▄" BWHI1 WHI10
"▄" BWHI1 WHI10 "▄" BWHI1 WHI10 "▄" BWHI1 WHI10 "▄" GRY12 WHI11
"▄" BCK15 BK46 "▄" BCK21 BK27 "▄" BCK BK0 "▄▄▄▄▄▄▄▄",
BCK BK0 "     " BK0 "▄" BYW37 YW11 "▄" BCK38 YW8 "▄" BCK32 GR3
"▄" BWHI9 GREY12 "▄" BWHI1 WHI12 "▄" BWHI1 WHI10 "▄" BWHI1 WHI10
"▄" BWHI1 WHI10 "▄" BWHI5 WHI12 "▄" BWHI1 WHI10 "▄" BWHI1 WHI12
"▄" BWHI1 WHI10 "▄" BWHI5 GREY25 "▄" BCK31 BK43 "▄" BCK28 YW36
"▄" BYW2 YW21 "▄" BCK11 BK21 "▄" BCK BK0 "▄▄▄▄▄▄",
BCK BK0 "    " BCK11 YW20 "▄" BYW12 YW "▄" BYW17 YW15 "▄" BYW17 YW
"▄" BYW38 YW15 "▄" BCK40 YW22 "▄" GRY30 BK22 "▄" BWHI1 GREY14
"▄" BWHI1 WHI12 "▄" BWHI1 WHI12 "▄" BWHI1 WHI10 "▄" BWHI1 WHI12
"▄" BWHI1 GREY14 "▄" GRY25 BK29 "▄" BCK29 YW22 "▄" BYW38 YW
"▄" BYW17 YW "▄" BYW16 YW "▄" BYW31 YW "▄" BCK17 YW16 "▄" BCK BK0
"▄▄▄▄▄",
BCK BK0 "    " BYW45 BK23 "▄" BYW17 YW14 "▄" BYW17 YW22 "▄" BYW17 YW
"▄" BYW16 YW "▄" BYW16 YW "▄" BYW27 YW20 "▄" GRY23 BK0
"▄" BWHI8 BK29 "▄" BWHI5 GREY21 "▄" BWHI1 GREY21 "▄" BWHI8 BK46
"▄" GRY18 BK0 "▄" BCK43 YW12 "▄" BYW17 YW "▄" BYW16 YW
"▄" BYW17 YW15 "▄" BYW16 YW28 "▄" BYW28 BK20 "▄" BYW27 BK0
"▄" BCK BK0 "▄▄▄▄▄",
BCK BK0 "      " BCK40 "▄" BYW53 BK0 "▄" BYW31 BK14 "▄" BYW10 BK15
"▄" BCK43 BK0 "▄" BCK BK0 "▄▄▄▄▄▄" BYW44 BK0 "▄" BYW10 BK15
"▄" BCK43 BK0 "▄" BYW39 BK0 "▄" BCK BK0 "▄▄▄▄▄▄▄▄",
BCK BK0 "    " BK32 "▄" BCK BK22 "▄" BCK BK22 "▄▄" BCK BK18 "▄" BCK32 BK19
"▄" GRY12 WHI10 "▄" GRY24 GREY10 "▄" GRY12 GREY10 "▄" GRY23 GREY15
"▄" BCK15 BK10 "▄" GRY30 WHI10 "▄" BCK32 BK27 "▄" BCK BK0
"▄▄▄" GRY12 WHI10 "▄" GRY24 GREY28 "▄" GRY24 GREY10 "▄" GRY22 GREY29
"▄" BCK BK0 "▄▄▄▄▄",
BCK BK0 "    " BCK21 "▄" GRY36 BK0 "▄" GRY36 BK22 "▄" BWHI10 GREY4
"▄" GRY22 BK46 "▄" BCK34 BK19 "▄" BWHI1 WHI12 "▄" BCK36 WHI12
"▄" BCK34 WHI10 "▄" BCK BK0 "▄" BCK32 BK10 "▄" BWHI1 WHI10
"▄" BCK21 BK27 "▄" BCK BK0 "▄▄▄" BWHI1 WHI10 "▄" GRY23 WHI10
"▄" BCK36 WHI10 "▄" BCK15 BK29 "▄" BCK BK0 "▄▄▄▄▄",
BCK BK0 "    " BK27 "▄" BCK31 GREY16 "▄" GRY36 GREY16 "▄" GRY28 GREY3
"▄" BCK GREY15 "▄" BCK34 BK19 "▄" BWHI1 WHI10 "▄" BCK15 GREY28
"▄" BCK GREY3 "▄" BCK GREY31 "▄" BCK32 BK10 "▄" BWHI1 WHI10
"▄" BCK21 GREY28 "▄" BCK GREY3 "▄" BCK GREY33 "▄" BCK BK0
"▄" BWHI5 WHI10 "▄" BCK36 BK22 "▄" BCK BK0 "▄▄▄▄▄▄▄",
BCK BK0 "    " BCK32 "▄" GRY13 BK0 "▄" GRY13 BK0 "▄" GRY13 BK0 "▄" GRY28 BK0
"▄" BCK32 BK0 "▄" GRY13 BK0 "▄" GRY13 BK0 "▄▄" GRY17 BK0
"▄" BCK15 BK0 "▄" GRY16 BK0 "▄" GRY13 BK0 "▄" GRY13 BK0
"▄" GRY14 BK0 "▄" BCK BK0 "▄" GRY13 BK0 "▄" BCK22 BK0 "▄" BCK BK0
"▄▄▄▄▄▄▄",
BCK " " ZELF_H1 " ",
BCK BK0 "      " GREY17 "   By Seb3773          ",
logo_version_line};

static const char logo_version_ascii[] =
    "           v" VERSION "             ";

static const char *const logo_lines_ascii[] = {"                             ",
"    ░▓▓▒  ░░░░               ",
"    ▒▓▓▓▓▓▓▓▓▓▓▒░            ",
"    ░▓▒▒ ▒▓▒▒▒▒▓▓▒░          ",
"        ░▓▒▒▒▒▒▒▒▒▓▒         ",
"       ░▓▒▒▒▒▒▒▒▒▒▒▓▒        ",
"      ░▓▒▒▒▒▒▒▒▒▒▒▒▒▓▒       ",
"      ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░      ",
"▒   ░▒▓▓▒▒▒▒▒▒▒▒▒▒▒▒▒▓▓▒   ▒ ",
"▒▒░ ▓▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▓ ░▒▒ ",
"▒░▒▒▓▓▓▓▓▓▒▒▓▓▓▓▓▒▒▓▓▓▓▓▒▒░▒ ",
"▒░░▒▓▓▓▓▒    ▒▓░   ░▓▓▓▓▒░░▒ ",
"░▒░▒▒▓▓▓  ░░░ ▒ ░░  ▒▓▓▒▒░▒░ ",
" ▒▒░▒▒▓▓  ▒▓▒ ▒ ▓▒  ▒▓▓▒░▒▒  ",
"  ░▒▒▒▓▓▒░░▒▒▒▒▒▒▒░░▓▓▒▒▒░   ",
"    ░▒▓▓▓▒░░░░░░░░▒▒▓▓▓▒░    ",
"    ░▓▓▓▓▒░░░░░░░░░▒▓▓▓▓░    ",
"     ▒▓▓▓▓▓▒▒▒▒▒▒▒▓▓▓▓▓▒     ",
"       ▒▓▓▓▓▓▓▓▓▓▓▓▓▓▒       ",
"                             ",
" ★===[ELF binary packer]===★ ",
"         By Seb3773          ",
logo_version_ascii};

static int disable_colors = 0;
static int skip_animation = 0;
static int skip_exit_requested = 0;
static size_t skip_initial_len = 0;
static char skip_initial_keys[64];
static int stdin_is_tty = -1;
static int stdin_termios_modified = 0;
static int credits_cleanup_registered = 0;
static int credits_signals_installed = 0;
static struct termios stdin_termios_saved;
static struct sigaction credits_old_sigint;
static struct sigaction credits_old_sigterm;
static struct sigaction credits_old_sigquit;
static const useconds_t credits_delay_us = 3000;
static const useconds_t credits_spinner_step_us_initial = 18000;
static const size_t credits_spinner_max_preview = 5;
static const size_t credits_window_height = 20;
static inline void credits_restore_terminal(void);
static void credits_cleanup_exit(void) { credits_restore_terminal(); }
static void credits_signal_handler(int sig) {
  credits_restore_terminal();
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  sigaction(sig, &sa, NULL);
  raise(sig);
}

static inline void credits_prepare_terminal(void) {
  if (stdin_is_tty < 0) {
    stdin_is_tty = isatty(STDIN_FILENO) ? 1 : 0;
  }
  if (!stdin_is_tty || stdin_termios_modified) {
    return;
  }

  struct termios raw;
  if (tcgetattr(STDIN_FILENO, &stdin_termios_saved) != 0) {
    stdin_is_tty = 0;
    return;
  }
  raw = stdin_termios_saved;
  raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
    stdin_termios_modified = 1;
    if (!credits_cleanup_registered) {
      atexit(credits_cleanup_exit);
      credits_cleanup_registered = 1;
    }
    if (!credits_signals_installed) {
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa));
      sa.sa_handler = credits_signal_handler;
      sigemptyset(&sa.sa_mask);
      sigaction(SIGINT, &sa, &credits_old_sigint);
      sigaction(SIGTERM, &sa, &credits_old_sigterm);
      sigaction(SIGQUIT, &sa, &credits_old_sigquit);
      credits_signals_installed = 1;
    }
  }
}

static inline void credits_restore_terminal(void) {
  if (!stdin_termios_modified) {
    return;
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &stdin_termios_saved);
  stdin_termios_modified = 0;
  if (credits_signals_installed) {
    sigaction(SIGINT, &credits_old_sigint, NULL);
    sigaction(SIGTERM, &credits_old_sigterm, NULL);
    sigaction(SIGQUIT, &credits_old_sigquit, NULL);
    credits_signals_installed = 0;
  }
}

enum {
  CREDITS_KEY_NONE = 0,
  CREDITS_KEY_UP = 1,
  CREDITS_KEY_DOWN = 2,
  CREDITS_KEY_ESC = 3,
  CREDITS_KEY_ENTER = 4
};

static inline int credits_decode_key(const char *buf, size_t len);

static inline void credits_poll_skip(void) {
  if (skip_animation) {
    return;
  }
  if (stdin_is_tty <= 0) {
    return;
  }

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  int res = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
  if (res > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
    char buf[128];
    ssize_t rd = read(STDIN_FILENO, buf, sizeof(buf));
    if (rd > 0) {
      skip_initial_len = 0;
      skip_exit_requested = 0;

      size_t copy = (size_t)rd;
      if (copy > sizeof(skip_initial_keys)) {
        copy = sizeof(skip_initial_keys);
      }
      if (copy > 0) {
        memcpy(skip_initial_keys, buf, copy);
        skip_initial_len = copy;
      }

      while (skip_initial_len < sizeof(skip_initial_keys)) {
        ssize_t extra = read(STDIN_FILENO, skip_initial_keys + skip_initial_len,
                             sizeof(skip_initial_keys) - skip_initial_len);
        if (extra <= 0) {
          break;
        }
        skip_initial_len += (size_t)extra;
      }

      const int key = credits_decode_key(skip_initial_keys, skip_initial_len);
      if (key == CREDITS_KEY_ESC) {
        skip_exit_requested = 1;
        skip_initial_len = 0;
      } else if (key == CREDITS_KEY_ENTER) {
        skip_initial_len = 0;
      }

      skip_animation = 1;
    }
  }
}

static inline void emit_raw(const char *s) {
  if (!s) {
    return;
  }
  if (!disable_colors) {
    fputs(s, stdout);
    return;
  }
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    if (*p == 0x1b) {
      ++p;
      if (*p == '[') {
        ++p;
        while (*p && *p != 'm') {
          ++p;
        }
        if (*p == 'm') {
          ++p;
        }
      }
      continue;
    }
    fputc(*p, stdout);
    ++p;
  }
}

static inline void emit_raw_ln(const char *s) {
  emit_raw(s);
  fputc('\n', stdout);
}

static inline void emit_raw_with_delay(const char *s, useconds_t delay_us) {
  if (!s) {
    return;
  }
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    credits_poll_skip();
    if (skip_animation) {
      emit_raw((const char *)p);
      fflush(stdout);
      return;
    }
    const unsigned char *start = p;
    unsigned char c = *p;
    if (c == 0x1b) {
      ++p;
      if (*p == '[') {
        ++p;
        while (*p && *p != 'm') {
          ++p;
        }
        if (*p == 'm') {
          ++p;
        }
      }
      if (!disable_colors) {
        fwrite(start, 1, (size_t)(p - start), stdout);
        fflush(stdout);
      }
      continue;
    }

    size_t extra = 0;
    if ((c & 0x80u) != 0) {
      if ((c & 0xE0u) == 0xC0u) {
        extra = 1;
      } else if ((c & 0xF0u) == 0xE0u) {
        extra = 2;
      } else if ((c & 0xF8u) == 0xF0u) {
        extra = 3;
      }
    }

    const size_t glyph_bytes = extra + 1;
    const int do_delay = delay_us != 0;
    useconds_t spinner_step = 0;
    useconds_t spinner_consumed = 0;
    if (do_delay && glyph_bytes == 1 && *p > 32 && *p <= 126) {
      size_t preview_len = 0;
      char preview_buf[credits_spinner_max_preview];
      size_t remaining = credits_spinner_max_preview;
      const unsigned char *lookahead = p;
      while (remaining && *lookahead && *lookahead != '\n') {
        unsigned char lc = *lookahead;
        if (lc == 0x1b) {
          ++lookahead;
          if (*lookahead == '[') {
            ++lookahead;
            while (*lookahead && *lookahead != 'm') {
              ++lookahead;
            }
            if (*lookahead == 'm') {
              ++lookahead;
            }
          }
          continue;
        }
        preview_buf[preview_len++] = (char)lc;
        ++lookahead;
        --remaining;
      }
      if (preview_len == 0) {
        preview_buf[preview_len++] = (char)*p;
      }
      spinner_step = credits_spinner_step_us_initial;
      for (size_t idx = 0; idx < preview_len; ++idx) {
        const char ch = preview_buf[idx];
        fputc(ch, stdout);
        fflush(stdout);
        if (spinner_step) {
          usleep(spinner_step);
        }
        fputc('\b', stdout);
        fflush(stdout);
        spinner_consumed += spinner_step;
        credits_poll_skip();
        if (skip_animation) {
          emit_raw((const char *)p);
          fflush(stdout);
          return;
        }
      }
    }

    fwrite(p, 1, glyph_bytes, stdout);
    fflush(stdout);
    if (spinner_step) {
      spinner_consumed += spinner_step;
    }
    if (do_delay) {
      if (delay_us > spinner_consumed) {
        usleep(delay_us - spinner_consumed);
      }
    }
    p += extra + 1;
    credits_poll_skip();
    if (skip_animation) {
      emit_raw((const char *)p);
      fflush(stdout);
      return;
    }
  }
}

static inline size_t visible_width(const char *s) {
  size_t width = 0;
  if (!s) {
    return 0;
  }
  while (*s) {
    unsigned char c = (unsigned char)*s;
    if (c == 0x1b) {
      ++s;
      if (*s == '[') {
        ++s;
        while (*s && *s != 'm') {
          ++s;
        }
        if (*s == 'm') {
          ++s;
        }
      }
      continue;
    }
    if ((c & 0x80u) == 0) {
      ++width;
      ++s;
      continue;
    }
    size_t extra = 0;
    if ((c & 0xE0u) == 0xC0u) {
      extra = 1;
    } else if ((c & 0xF0u) == 0xE0u) {
      extra = 2;
    } else if ((c & 0xF8u) == 0xF0u) {
      extra = 3;
    }
    ++width;
    ++s;
    while (extra && *s) {
      ++s;
      --extra;
    }
  }
  return width;
}

static inline void emit_line(const char *left, const char *right,
                             const char *border, size_t text_cols) {
  if (left) {
    emit_raw(left);
  }
  emit_raw(border);

  if (right) {
    emit_raw(right);
    size_t width = visible_width(right);
    if (width < text_cols) {
      size_t pad = text_cols - width;
      for (size_t i = 0; i < pad; ++i) {
        fputc(' ', stdout);
      }
    }
  } else {
    for (size_t i = 0; i < text_cols; ++i) {
      fputc(' ', stdout);
    }
  }

  emit_raw(RES);
  emit_raw(h2b);
  fputc('\n', stdout);
}

static inline void emit_line_typewriter(const char *left, const char *right,
                                        const char *border, size_t text_cols,
                                        useconds_t delay_us) {
  if (left) {
    emit_raw(left);
  }
  emit_raw(border);

  if (right) {
    emit_raw_with_delay(right, delay_us);
    size_t width = visible_width(right);
    if (width < text_cols) {
      size_t pad = text_cols - width;
      for (size_t i = 0; i < pad; ++i) {
        fputc(' ', stdout);
      }
    }
  } else {
    for (size_t i = 0; i < text_cols; ++i) {
      fputc(' ', stdout);
    }
  }

  emit_raw(RES);
  emit_raw(h2b);
  fputc('\n', stdout);
}

static inline void emit_full_border(int is_top, size_t text_cols) {
  emit_raw(BCK FYW);
  emit_raw(is_top ? "╭" : "╰");
  for (size_t i = 0; i < text_cols; ++i) {
    emit_raw("─");
  }
  emit_raw(is_top ? "╮" : "╯");
  fputc(' ', stdout);
  emit_raw(RES);
  fputc('\n', stdout);
}

static inline void credits_emit_title_line(const char *title,
                                           size_t header_cols) {
  emit_raw(title);
  size_t title_width = visible_width(title);
  if (header_cols > title_width) {
    size_t pad = header_cols - title_width;
    for (size_t i = 0; i < pad; ++i) {
      fputc(' ', stdout);
    }
  }
  emit_raw(RES);
  fputc('\n', stdout);
}

static inline void credits_cursor_up(size_t lines);
static inline void credits_cursor_down(size_t lines);

static inline void credits_update_title(const char *title, size_t header_cols,
                                        size_t window_height) {
  const size_t header_offset = window_height + 4;
  credits_cursor_up(header_offset);
  credits_emit_title_line(title, header_cols);
  credits_cursor_down(header_offset);
  fflush(stdout);
}

static inline void credits_render_manual_window(size_t frame_inner,
                                                size_t window_height,
                                                size_t frame_move_up,
                                                size_t top_index,
                                                size_t count) {
  credits_cursor_up(frame_move_up);
  for (size_t row = 0; row < window_height; ++row) {
    const size_t abs_index = top_index + row;
    const char *row_text =
        (abs_index < count) ? help_credits_lines[abs_index] : "";
    emit_line(NULL, row_text, h2, frame_inner);
  }
  emit_full_border(0, frame_inner);
  emit_raw_ln(f1);
  fflush(stdout);
}

static ssize_t credits_wait_key(char *buf, size_t buf_size) {
  if (stdin_is_tty <= 0) {
    return 0;
  }

  for (;;) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    const int res = select(STDIN_FILENO + 1, &readfds, NULL, NULL, NULL);
    if (res < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (res == 0) {
      continue;
    }
    ssize_t rd = read(STDIN_FILENO, buf, buf_size);
    if (rd < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (rd == 0) {
      continue;
    }
    return rd;
  }
}

static inline int credits_decode_key(const char *buf, size_t len) {
  if (len == 0 || !buf) {
    return CREDITS_KEY_NONE;
  }

  if (buf[0] == '\n' || buf[0] == '\r') {
    return CREDITS_KEY_ENTER;
  }

  if ((unsigned char)buf[0] == 0x1B) {
    if (len >= 3 && buf[1] == '[') {
      if (buf[2] == 'A') {
        return CREDITS_KEY_UP;
      }
      if (buf[2] == 'B') {
        return CREDITS_KEY_DOWN;
      }
    }
    return CREDITS_KEY_ESC;
  }

  return CREDITS_KEY_NONE;
}

static void credits_manual_loop(size_t frame_inner, size_t window_height,
                                size_t frame_move_up, size_t count,
                                size_t first_index, size_t top_index,
                                const char *initial_keys, size_t initial_len) {
  if (stdin_is_tty <= 0) {
    return;
  }

  size_t top = top_index;
  if (top < first_index) {
    top = first_index;
  }

  if (count <= window_height) {
    top = first_index;
  } else {
    size_t max_top = count - window_height;
    if (max_top < first_index) {
      max_top = first_index;
    }
    if (top > max_top) {
      top = max_top;
    }
  }

  credits_render_manual_window(frame_inner, window_height, frame_move_up, top,
                               count);

  for (;;) {
    int key;
    if (initial_len > 0 && initial_keys) {
      key = credits_decode_key(initial_keys, initial_len);
      initial_len = 0;
    } else {
      char keybuf[32];
      const ssize_t rd = credits_wait_key(keybuf, sizeof(keybuf));
      if (rd <= 0) {
        break;
      }
      key = credits_decode_key(keybuf, (size_t)rd);
    }
    if (key == CREDITS_KEY_ESC || key == CREDITS_KEY_ENTER) {
      break;
    }
    if (key == CREDITS_KEY_UP) {
      if (top > first_index) {
        --top;
        credits_render_manual_window(frame_inner, window_height, frame_move_up,
                                     top, count);
      }
      continue;
    }
    if (key == CREDITS_KEY_DOWN) {
      size_t max_top;
      if (count <= window_height) {
        max_top = first_index;
      } else {
        max_top = count - window_height;
        if (max_top < first_index) {
          max_top = first_index;
        }
      }
      if (top < max_top) {
        ++top;
        credits_render_manual_window(frame_inner, window_height, frame_move_up,
                                     top, count);
      }
      continue;
    }
  }
}

static inline void emit_full_blank_line(size_t text_cols) {
  emit_raw(BCK);
  for (size_t i = 0; i < text_cols; ++i) {
    fputc(' ', stdout);
  }
  emit_raw(RES);
  fputc('\n', stdout);
}

static inline void emit_full_centered_line(const char *text, size_t text_cols) {
  const size_t width = visible_width(text);
  size_t left_pad = 0;
  size_t right_pad = 0;
  if (text_cols > width) {
    const size_t remaining = text_cols - width;
    left_pad = remaining / 2;
    right_pad = remaining - left_pad;
  }
  emit_raw(BCK);
  for (size_t i = 0; i < left_pad; ++i) {
    fputc(' ', stdout);
  }
  emit_raw(text);
  emit_raw(BCK);
  for (size_t i = 0; i < right_pad; ++i) {
    fputc(' ', stdout);
  }
  emit_raw(RES);
  fputc('\n', stdout);
}

static inline void credits_cursor_up(size_t lines) {
  if (lines == 0) {
    return;
  }

  char seq[32];
  const int len = snprintf(seq, sizeof(seq), "\x1b[%zuA", lines);
  if (len <= 0) {
    return;
  }

  fwrite(seq, 1, (size_t)len, stdout);
}

static inline void credits_cursor_down(size_t lines) {
  if (lines == 0) {
    return;
  }

  char seq[32];
  const int len = snprintf(seq, sizeof(seq), "\x1b[%zuB", lines);
  if (len <= 0) {
    return;
  }

  fwrite(seq, 1, (size_t)len, stdout);
}

static inline void help_display_set_colors(GBOOLEAN use_colors) {
  disable_colors = (use_colors == GFALSE);
}

void help_display_show_overview(GBOOLEAN use_colors) {
  help_display_set_colors(use_colors);
  const size_t frame_inner = visible_width(h1) - 3;
  const size_t help_cols = frame_inner;
  const size_t text_count = sizeof(help_lines) / sizeof(help_lines[0]);
  const char *const *logos = disable_colors ? logo_lines_ascii : logo_lines;
  const size_t logo_count =
      disable_colors ? (sizeof(logo_lines_ascii) / sizeof(logo_lines_ascii[0]))
                     : (sizeof(logo_lines) / sizeof(logo_lines[0]));
  const char *logo_blank = disable_colors ? logo_lines_ascii[0] : logo_L0;

  emit_raw_ln(f1);
  emit_raw(logo_blank);
  emit_raw_ln(h1);

  const size_t total_lines =
      (text_count > logo_count) ? text_count : logo_count;
  const size_t logo_start = (total_lines - logo_count) / 2;
  const size_t text_start = (total_lines - text_count) / 2;

  for (size_t i = 0; i < total_lines; ++i) {
    const char *left = (i >= logo_start && i < logo_start + logo_count)
                           ? logos[i - logo_start]
                           : logo_blank;
    const char *right = (i >= text_start && i < text_start + text_count)
                            ? help_lines[i - text_start]
                            : help_lines[2];
    emit_line(left, right, h2, help_cols);
  }

  emit_raw(logo_blank);
  emit_raw_ln(h3);
  emit_raw_ln(f1);
  if (stdin_is_tty > 0) {
    tcflush(STDIN_FILENO, TCIFLUSH);
  }

  putchar('\n');
}

void help_display_show_full(GBOOLEAN use_colors) {
  help_display_set_colors(use_colors);
  const size_t frame_inner = visible_width(f1) - 3;
  const size_t header_cols = frame_inner + 3;

  emit_raw_ln(f1);
  emit_full_blank_line(header_cols);
  emit_full_centered_line(ZELF_H2, header_cols);
  emit_full_blank_line(header_cols);
  emit_raw(full_help_title_prefix);
  size_t title_width = visible_width(full_help_title_prefix);
  if (header_cols > title_width) {
    size_t pad = header_cols - title_width;
    for (size_t i = 0; i < pad; ++i) {
      fputc(' ', stdout);
    }
  }
  emit_raw(RES);
  fputc('\n', stdout);
  emit_full_border(1, frame_inner);

  const size_t count = sizeof(full_help_lines) / sizeof(full_help_lines[0]);
  for (size_t i = 0; i < count; ++i) {
    emit_line(NULL, full_help_lines[i], h2, frame_inner);
  }

  emit_full_border(0, frame_inner);
  emit_raw_ln(f1);
  putchar('\n');
}

void help_display_show_algorithms(GBOOLEAN use_colors) {
  help_display_set_colors(use_colors);
  const size_t frame_inner = visible_width(f1) - 3;
  const size_t header_cols = frame_inner + 3;

  emit_raw_ln(f1);
  emit_full_blank_line(header_cols);
  emit_full_centered_line(ZELF_H2, header_cols);
  emit_full_blank_line(header_cols);
  emit_raw(algos_help_title_prefix);
  size_t title_width = visible_width(algos_help_title_prefix);
  if (header_cols > title_width) {
    size_t pad = header_cols - title_width;
    for (size_t i = 0; i < pad; ++i) {
      fputc(' ', stdout);
    }
  }
  emit_raw(RES);
  fputc('\n', stdout);
  emit_full_border(1, frame_inner);

  const size_t count = sizeof(help_algos_lines) / sizeof(help_algos_lines[0]);
  for (size_t i = 0; i < count; ++i) {
    emit_line(NULL, help_algos_lines[i], h2, frame_inner);
  }

  emit_full_border(0, frame_inner);
  emit_raw_ln(f1);
  putchar('\n');
}

void help_display_show_faq(GBOOLEAN use_colors) {
  help_display_set_colors(use_colors);
  const size_t frame_inner = visible_width(f1) - 3;
  const size_t header_cols = frame_inner + 3;

  emit_raw_ln(f1);
  emit_full_blank_line(header_cols);
  emit_full_centered_line(ZELF_H2, header_cols);
  emit_full_blank_line(header_cols);
  emit_raw(faq_title_prefix);
  size_t title_width = visible_width(faq_title_prefix);
  if (header_cols > title_width) {
    size_t pad = header_cols - title_width;
    for (size_t i = 0; i < pad; ++i) {
      fputc(' ', stdout);
    }
  }
  emit_raw(RES);
  fputc('\n', stdout);
  emit_full_border(1, frame_inner);

  const size_t count = sizeof(help_faq_lines) / sizeof(help_faq_lines[0]);
  for (size_t i = 0; i < count; ++i) {
    emit_line(NULL, help_faq_lines[i], h2, frame_inner);
  }

  emit_full_border(0, frame_inner);
  emit_raw_ln(f1);
  putchar('\n');
}

void display_help_credits() {
  const size_t frame_inner = visible_width(f1) - 3;
  const size_t header_cols = frame_inner + 3;

  skip_animation = 0;
  skip_exit_requested = 0;
  skip_initial_len = 0;
  stdin_is_tty = -1;
  credits_prepare_terminal();

  emit_raw_ln(f1);
  emit_full_blank_line(header_cols);
  emit_full_centered_line(ZELF_H2, header_cols);
  emit_full_blank_line(header_cols);
  credits_emit_title_line(credits_title_prefix, header_cols);
  emit_full_border(1, frame_inner);

  const size_t count =
      sizeof(help_credits_lines) / sizeof(help_credits_lines[0]);
  size_t first_visible = 0;
  while (first_visible < count) {
    const char *candidate = help_credits_lines[first_visible];
    if (!candidate) {
      break;
    }
    if (visible_width(candidate) != 0) {
      break;
    }
    ++first_visible;
  }
  const size_t window_height = credits_window_height;
  const char *window[credits_window_height];
  size_t window_start = 0;
  size_t window_count = 0;

  for (size_t i = 0; i < window_height; ++i) {
    window[i] = "";
    emit_line(NULL, "", h2, frame_inner);
  }
  emit_full_border(0, frame_inner);
  emit_raw_ln(f1);

  const size_t frame_move_up = window_height + 2;
  size_t idx = first_visible;
  int skip_engaged = 0;

  while (idx < count) {
    const char *line = help_credits_lines[idx];
    size_t newest_index;

    if (window_count < window_height) {
      const size_t insert_index = (window_start + window_count) % window_height;
      window[insert_index] = line ? line : "";
      newest_index = insert_index;
      ++window_count;
    } else {
      window[window_start] = line ? line : "";
      window_start = (window_start + 1) % window_height;
      newest_index = (window_start + window_height - 1) % window_height;
    }

    credits_cursor_up(frame_move_up);
    const int animate_new_line = (!skip_engaged && !skip_animation);

    for (size_t row = 0; row < window_height; ++row) {
      const int has_text = (row < window_count);
      const size_t buffer_index =
          has_text ? (window_start + row) % window_height : 0;
      const char *row_text = has_text ? window[buffer_index] : "";
      const int is_newest_row = has_text && (buffer_index == newest_index);

      if (is_newest_row && animate_new_line && row_text[0] != '\0') {
        emit_line(NULL, "", h2, frame_inner);
        credits_cursor_up(1);
        emit_line_typewriter(NULL, row_text, h2, frame_inner, credits_delay_us);
      } else {
        emit_line(NULL, row_text, h2, frame_inner);
      }
    }

    emit_full_border(0, frame_inner);
    emit_raw_ln(f1);

    ++idx;

    if (!skip_engaged && skip_animation) {
      skip_engaged = 1;

      if (skip_exit_requested) {
        break;
      }

      credits_update_title(credits_title_manual_prefix, header_cols,
                           window_height);

      size_t manual_top =
          (idx > window_count) ? (idx - window_count) : first_visible;
      if (manual_top < first_visible) {
        manual_top = first_visible;
      }
      credits_manual_loop(frame_inner, window_height, frame_move_up, count,
                          first_visible, manual_top, skip_initial_keys,
                          skip_initial_len);
      skip_initial_len = 0;
      skip_exit_requested = 0;
      break;
    }
  }

  putchar('\n');
}

void help_display_show_credits(GBOOLEAN use_colors) {
  help_display_set_colors(use_colors);
  display_help_credits();
}
