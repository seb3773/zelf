#ifndef ZELF_PACKER_H
#define ZELF_PACKER_H

// Global version for zELF packer and help UI
// Update this single definition to bump the version across the project.
#define ZELF_VERSION "0.91"

// Reuse ANSI reset and a few colors; define packer color wrappers.
// Note: help_colors.h declares many color constants; we only use a subset here.
#include "help_colors.h"
#include <stdio.h>

// Runtime flag (defined in zelf_packer.c)
extern int g_no_colors;
extern FILE *g_ui_stream;

// Return the ANSI sequence or empty string if --no-colors is set
static inline const char* PKC(const char* c) {
    return g_no_colors ? "" : c;
}

// Print raw string, stripping ANSI SGR sequences when --no-colors is active
static inline void pk_emit_raw(const char* s) {
    if (!s) return;
    FILE *out = g_ui_stream ? g_ui_stream : stdout;
    if (!g_no_colors) { fputs(s, out); return; }
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        if (*p == 0x1b) {
            ++p;
            if (*p == '[') {
                ++p;
                while (*p && *p != 'm') ++p;
                if (*p == 'm') ++p;
            }
            continue;
        }
        fputc(*p, out);
        ++p;
    }
}

// Common packer color aliases (foreground)
#define PK_RES   PKC(RES)              // reset
#define PK_ERR   PKC(FRED)             // error red
#define PK_WARN  PKC(FBB4)             // warning yellow (fg 228)
#define PK_INFO  PKC("\x1b[38;5;33m") // info blue-ish
#define PK_ARROW PKC(FYW)              // arrow '>' style

// Success/accents
#define PK_OK    PKC("\x1b[38;5;46m")  // green check
#define PK_ACC1  PKC("\x1b[38;5;177m") // accent dot
#define PK_ACC2  PKC("\x1b[38;5;81m")  // accent arrow

// Symbols (kept even in monochrome)
#define PK_SYM_ERR   "✖"
#define PK_SYM_INFO  "ℹ"
#define PK_SYM_WARN  "⚠"
#define PK_SYM_ARROW "❯"
#define PK_SYM_COMPUT "⚙"
#define PK_SYM_CHECK "✘"
#define PK_SYM_EST "✜"
#define PK_SYM_FOCUS "☉"
#define PK_SYM_OK    "✔"
#define PK_SYM_DOT   "●"
#define PK_SYM_RARROW "➤"

#endif // ZELF_PACKER_H
