/**
 * @file help_display.h
 * @brief Terminal help rendering API for zELF auxiliary tools.
 *
 * The helpers exposed here print the various interactive help sections
 * (overview, extended help, algorithms, FAQ, credits) used by the zELF
 * command-line helpers. Each function accepts a `GBOOLEAN use_colors`
 * parameter: pass `GTRUE` to enable ANSI color sequences, or `GFALSE`
 * to request monochrome output.
 *
 * Example usage:
 * ```c
 * #include "help_display.h"
 *
 * int main(void) {
 *     help_display_show_overview(GTRUE);
 *     help_display_show_credits(GFALSE); // ASCII mode, still animated.
 *     return 0;
 * }
 * ```
 */
#ifndef HELP_DISPLAY_H
#define HELP_DISPLAY_H

#include "help_colors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int GBOOLEAN;
#define GFALSE 0
#define GTRUE 1

void help_display_show_overview(GBOOLEAN use_colors);
void help_display_show_full(GBOOLEAN use_colors);
void help_display_show_algorithms(GBOOLEAN use_colors);
void help_display_show_faq(GBOOLEAN use_colors);
void help_display_show_credits(GBOOLEAN use_colors);

/* Shared banner used by packer and help UI */
#define ZELF_H2 BCK FRED "★" FRED2 "===" FYW "[ " WHI10 "zELF" GREY17 " - ELF binary packer" FYW "]" FRED2 "===" FRED "★"

#ifdef __cplusplus
}
#endif

#endif /* HELP_DISPLAY_H */
