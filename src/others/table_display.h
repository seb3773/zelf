/*
 * table_display.h - Terminal table display library
 *
 * Pure C library for displaying formatted tables in Linux terminal
 * using UTF-8 box-drawing characters and ANSI colors.
 *
 * Memory: Uses compact limits, caller provides workspace
 * Dependencies: None (uses write() syscall directly)
 * Target: Linux x86_64
 */

#ifndef TABLE_DISPLAY_H
#define TABLE_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

/* Configuration limits */
#define TD_MAX_COLS 16
#define TD_MAX_ROWS 32
#define TD_MAX_SECTIONS 8
#define TD_MAX_LINES 4
#define TD_MAX_LINE_LEN 64
#define TD_MAX_TITLE_LEN 128

/* Special color value: no custom color (use default or no color) */
#define TD_COLOR_NONE 0

/* Color structure for ANSI 256-color palette */
typedef struct {
  uint8_t title_bg;    /* title background (default: 238 dark gray) */
  uint8_t subtitle_bg; /* subtitle background (default: 208 orange) */
  uint8_t row_bg;  /* row header/labels background (default: 22 dark green) */
  uint8_t col_bg;  /* column header/labels background (default: 24 dark blue) */
  uint8_t text_fg; /* text foreground (default: 250 light gray) */
} td_colors_t;

/* Per-cell color structure */
typedef struct {
  uint8_t fg; /* foreground color (0 = no custom color) */
  uint8_t bg; /* background color (0 = no custom color) */
} td_cell_color_t;

/* Section data structure */
typedef struct {
  char subtitle[TD_MAX_LINE_LEN];
  char cells[TD_MAX_ROWS][TD_MAX_COLS][TD_MAX_LINES][TD_MAX_LINE_LEN];
  size_t cell_line_count[TD_MAX_ROWS][TD_MAX_COLS];
  size_t row_heights[TD_MAX_ROWS];
  td_cell_color_t cell_colors[TD_MAX_ROWS][TD_MAX_COLS];
} td_section_t;

/* Table context structure */
typedef struct {
  /* Title */
  char title[TD_MAX_TITLE_LEN];

  /* Headers */
  char row_header[TD_MAX_LINE_LEN];
  char col_header[TD_MAX_LINE_LEN];

  /* Column labels and widths */
  char col_labels[TD_MAX_COLS][TD_MAX_LINE_LEN];
  size_t col_widths[TD_MAX_COLS];
  size_t num_cols;

  /* Row labels */
  char row_labels[TD_MAX_ROWS][TD_MAX_LINE_LEN];
  size_t num_rows;

  /* Sections */
  td_section_t sections[TD_MAX_SECTIONS];
  size_t num_sections;

  /* Colors */
  td_colors_t colors;
  int use_custom_colors;
  int colors_disabled; /* 1 = no ANSI colors in output */

  /* Computed dimensions */
  size_t row_label_width;
  size_t total_width;
} td_table_t;

/* Initialize table with title */
void td_init(td_table_t *t, const char *title);

/* Disable all colors (for piping to files or non-color terminals) */
void td_disable_colors(td_table_t *t);

/* Enable colors (default state) */
void td_enable_colors(td_table_t *t);

/* Set custom table colors (ANSI 256-color palette indices) */
void td_set_colors(td_table_t *t, uint8_t title_bg, uint8_t subtitle_bg,
                   uint8_t row_bg, uint8_t col_bg, uint8_t text_fg);

/* Set individual table colors */
void td_set_title_color(td_table_t *t, uint8_t bg);
void td_set_subtitle_color(td_table_t *t, uint8_t bg);
void td_set_row_color(td_table_t *t, uint8_t bg);
void td_set_col_color(td_table_t *t, uint8_t bg);
void td_set_text_color(td_table_t *t, uint8_t fg);

/* Set per-cell colors (section, row, col are 1-indexed) */
void td_set_cell_color(td_table_t *t, size_t section, size_t row, size_t col,
                       uint8_t fg, uint8_t bg);

/* Set headers */
void td_set_row_header(td_table_t *t, const char *header);
void td_set_col_header(td_table_t *t, const char *header);

/* Set labels */
void td_set_columns(td_table_t *t, const char **labels, size_t count);
void td_set_rows(td_table_t *t, const char **labels, size_t count);

/* Section management */
size_t td_add_section(td_table_t *t, const char *subtitle);
void td_set_section_cell(td_table_t *t, size_t section, size_t row, size_t col,
                         const char *content);

/* Backward-compatible cell setter (uses section 1) */
void td_set_cell(td_table_t *t, size_t row, size_t col, const char *content);

/* Compute dimensions and print */
void td_compute_dimensions(td_table_t *t);
void td_print(td_table_t *t);

#endif /* TABLE_DISPLAY_H */
