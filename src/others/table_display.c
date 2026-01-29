/*
 * table_display.c - Terminal table display library implementation
 *
 * Pure C implementation for Linux x86_64
 * Uses UTF-8 box-drawing characters for borders
 * ANSI escape codes for colors (256-color palette)
 * Supports multiple sections, customizable colors, and per-cell colors
 */

#include "table_display.h"
#include <string.h>
#include <unistd.h>

/* UTF-8 box drawing characters */
static const char BOX_TL[] = "\xe2\x94\x8c";
static const char BOX_TR[] = "\xe2\x94\x90";
static const char BOX_BL[] = "\xe2\x94\x94";
static const char BOX_BR[] = "\xe2\x94\x98";
static const char BOX_H[] = "\xe2\x94\x80";
static const char BOX_V[] = "\xe2\x94\x82";
static const char BOX_TJ[] = "\xe2\x94\xac";
static const char BOX_BJ[] = "\xe2\x94\xb4";
static const char BOX_LJ[] = "\xe2\x94\x9c";
static const char BOX_RJ[] = "\xe2\x94\xa4";
static const char BOX_CJ[] = "\xe2\x94\xbc";

/* Default colors (ANSI 256-color palette) */
#define DEF_TITLE_BG 238
#define DEF_SUBTITLE_BG 208
#define DEF_ROW_BG 22
#define DEF_COL_BG 24
#define DEF_TEXT_FG 250

/* Output buffer */
static char out_buf[32768];
static size_t out_pos = 0;

/* Current table pointer for color checks */
static td_table_t *cur_table = NULL;

static inline void flush_output(void) {
  if (out_pos > 0) {
    write(STDOUT_FILENO, out_buf, out_pos);
    out_pos = 0;
  }
}

static inline void emit(const char *s, size_t len) {
  if (out_pos + len >= sizeof(out_buf))
    flush_output();
  memcpy(out_buf + out_pos, s, len);
  out_pos += len;
}

static inline void emit_str(const char *s) { emit(s, strlen(s)); }

static inline void emit_char(char c) {
  if (out_pos + 1 >= sizeof(out_buf))
    flush_output();
  out_buf[out_pos++] = c;
}

static inline void emit_spaces(size_t n) {
  while (n--)
    emit_char(' ');
}

static inline void emit_horizontal(size_t n) {
  while (n--)
    emit_str(BOX_H);
}

/* Emit ANSI color codes (respects colors_disabled flag) */
static void emit_fg(uint8_t color) {
  if (cur_table && cur_table->colors_disabled)
    return;
  char buf[16];
  int len = 0;
  buf[len++] = '\x1b';
  buf[len++] = '[';
  buf[len++] = '3';
  buf[len++] = '8';
  buf[len++] = ';';
  buf[len++] = '5';
  buf[len++] = ';';
  if (color >= 100)
    buf[len++] = '0' + (color / 100);
  if (color >= 10)
    buf[len++] = '0' + ((color / 10) % 10);
  buf[len++] = '0' + (color % 10);
  buf[len++] = 'm';
  emit(buf, (size_t)len);
}

static void emit_bg(uint8_t color) {
  if (cur_table && cur_table->colors_disabled)
    return;
  char buf[16];
  int len = 0;
  buf[len++] = '\x1b';
  buf[len++] = '[';
  buf[len++] = '4';
  buf[len++] = '8';
  buf[len++] = ';';
  buf[len++] = '5';
  buf[len++] = ';';
  if (color >= 100)
    buf[len++] = '0' + (color / 100);
  if (color >= 10)
    buf[len++] = '0' + ((color / 10) % 10);
  buf[len++] = '0' + (color % 10);
  buf[len++] = 'm';
  emit(buf, (size_t)len);
}

static void emit_reset(void) {
  if (cur_table && cur_table->colors_disabled)
    return;
  emit_str("\x1b[0m");
}

/* Get display width of UTF-8 string */
static size_t str_display_width(const char *s) {
  size_t w = 0;
  while (*s) {
    if (*s == '\x1b') {
      while (*s && *s != 'm')
        s++;
      if (*s)
        s++;
      continue;
    }
    if ((*s & 0x80) == 0) {
      w++;
      s++;
    } else if ((*s & 0xE0) == 0xC0) {
      w++;
      s += 2;
    } else if ((*s & 0xF0) == 0xE0) {
      w++;
      s += 3;
    } else if ((*s & 0xF8) == 0xF0) {
      w++;
      s += 4;
    } else
      s++;
  }
  return w;
}

static inline size_t max_sz(size_t a, size_t b) { return (a > b) ? a : b; }

static void copy_str(char *dst, const char *src, size_t max_len) {
  if (src) {
    size_t len = strlen(src);
    if (len >= max_len)
      len = max_len - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
  } else {
    dst[0] = '\0';
  }
}

/* Get color values */
static inline uint8_t get_title_bg(td_table_t *t) {
  return t->use_custom_colors && t->colors.title_bg ? t->colors.title_bg
                                                    : DEF_TITLE_BG;
}
static inline uint8_t get_subtitle_bg(td_table_t *t) {
  return t->use_custom_colors && t->colors.subtitle_bg ? t->colors.subtitle_bg
                                                       : DEF_SUBTITLE_BG;
}
static inline uint8_t get_row_bg(td_table_t *t) {
  return t->use_custom_colors && t->colors.row_bg ? t->colors.row_bg
                                                  : DEF_ROW_BG;
}
static inline uint8_t get_col_bg(td_table_t *t) {
  return t->use_custom_colors && t->colors.col_bg ? t->colors.col_bg
                                                  : DEF_COL_BG;
}
static inline uint8_t get_text_fg(td_table_t *t) {
  return t->use_custom_colors && t->colors.text_fg ? t->colors.text_fg
                                                   : DEF_TEXT_FG;
}

void td_init(td_table_t *t, const char *title) {
  memset(t, 0, sizeof(*t));
  copy_str(t->title, title, TD_MAX_TITLE_LEN);
}

void td_disable_colors(td_table_t *t) { t->colors_disabled = 1; }

void td_enable_colors(td_table_t *t) { t->colors_disabled = 0; }

void td_set_colors(td_table_t *t, uint8_t title_bg, uint8_t subtitle_bg,
                   uint8_t row_bg, uint8_t col_bg, uint8_t text_fg) {
  t->use_custom_colors = 1;
  t->colors.title_bg = title_bg;
  t->colors.subtitle_bg = subtitle_bg;
  t->colors.row_bg = row_bg;
  t->colors.col_bg = col_bg;
  t->colors.text_fg = text_fg;
}

void td_set_title_color(td_table_t *t, uint8_t bg) {
  t->use_custom_colors = 1;
  t->colors.title_bg = bg;
}

void td_set_subtitle_color(td_table_t *t, uint8_t bg) {
  t->use_custom_colors = 1;
  t->colors.subtitle_bg = bg;
}

void td_set_row_color(td_table_t *t, uint8_t bg) {
  t->use_custom_colors = 1;
  t->colors.row_bg = bg;
}

void td_set_col_color(td_table_t *t, uint8_t bg) {
  t->use_custom_colors = 1;
  t->colors.col_bg = bg;
}

void td_set_text_color(td_table_t *t, uint8_t fg) {
  t->use_custom_colors = 1;
  t->colors.text_fg = fg;
}

void td_set_cell_color(td_table_t *t, size_t section, size_t row, size_t col,
                       uint8_t fg, uint8_t bg) {
  if (section == 0 || row == 0 || col == 0)
    return;
  section--;
  row--;
  col--;
  if (section >= t->num_sections || row >= TD_MAX_ROWS || col >= TD_MAX_COLS)
    return;
  t->sections[section].cell_colors[row][col].fg = fg;
  t->sections[section].cell_colors[row][col].bg = bg;
}

void td_set_row_header(td_table_t *t, const char *header) {
  copy_str(t->row_header, header, TD_MAX_LINE_LEN);
}

void td_set_col_header(td_table_t *t, const char *header) {
  copy_str(t->col_header, header, TD_MAX_LINE_LEN);
}

void td_set_columns(td_table_t *t, const char **labels, size_t count) {
  if (count > TD_MAX_COLS)
    count = TD_MAX_COLS;
  t->num_cols = count;
  for (size_t i = 0; i < count; i++)
    copy_str(t->col_labels[i], labels[i], TD_MAX_LINE_LEN);
}

void td_set_rows(td_table_t *t, const char **labels, size_t count) {
  if (count > TD_MAX_ROWS)
    count = TD_MAX_ROWS;
  t->num_rows = count;
  for (size_t i = 0; i < count; i++)
    copy_str(t->row_labels[i], labels[i], TD_MAX_LINE_LEN);
}

size_t td_add_section(td_table_t *t, const char *subtitle) {
  if (t->num_sections >= TD_MAX_SECTIONS)
    return 0;
  size_t idx = t->num_sections++;
  copy_str(t->sections[idx].subtitle, subtitle, TD_MAX_LINE_LEN);
  return idx + 1;
}

static void parse_cell_content(td_section_t *sec, size_t row, size_t col,
                               const char *content) {
  if (!content) {
    sec->cell_line_count[row][col] = 0;
    return;
  }
  size_t line = 0;
  const char *p = content, *ls = p;
  while (*p && line < TD_MAX_LINES) {
    if (*p == '\n') {
      size_t len = (size_t)(p - ls);
      if (len >= TD_MAX_LINE_LEN)
        len = TD_MAX_LINE_LEN - 1;
      memcpy(sec->cells[row][col][line], ls, len);
      sec->cells[row][col][line++][len] = '\0';
      ls = p + 1;
    }
    p++;
  }
  if (ls < p && line < TD_MAX_LINES) {
    size_t len = (size_t)(p - ls);
    if (len >= TD_MAX_LINE_LEN)
      len = TD_MAX_LINE_LEN - 1;
    memcpy(sec->cells[row][col][line], ls, len);
    sec->cells[row][col][line++][len] = '\0';
  }
  sec->cell_line_count[row][col] = line;
}

void td_set_section_cell(td_table_t *t, size_t section, size_t row, size_t col,
                         const char *content) {
  if (section == 0 || row == 0 || col == 0)
    return;
  section--;
  row--;
  col--;
  if (section >= t->num_sections || row >= t->num_rows || col >= t->num_cols)
    return;
  parse_cell_content(&t->sections[section], row, col, content);
}

void td_set_cell(td_table_t *t, size_t row, size_t col, const char *content) {
  if (t->num_sections == 0)
    td_add_section(t, "");
  td_set_section_cell(t, 1, row, col, content);
}

void td_compute_dimensions(td_table_t *t) {
  t->row_label_width = str_display_width(t->row_header);
  for (size_t r = 0; r < t->num_rows; r++)
    t->row_label_width =
        max_sz(t->row_label_width, str_display_width(t->row_labels[r]));
  for (size_t s = 0; s < t->num_sections; s++)
    t->row_label_width =
        max_sz(t->row_label_width, str_display_width(t->sections[s].subtitle));

  for (size_t c = 0; c < t->num_cols; c++)
    t->col_widths[c] = str_display_width(t->col_labels[c]);

  for (size_t s = 0; s < t->num_sections; s++) {
    td_section_t *sec = &t->sections[s];
    for (size_t r = 0; r < t->num_rows; r++) {
      for (size_t c = 0; c < t->num_cols; c++) {
        for (size_t ln = 0; ln < sec->cell_line_count[r][c]; ln++)
          t->col_widths[c] =
              max_sz(t->col_widths[c], str_display_width(sec->cells[r][c][ln]));
      }
    }
  }

  for (size_t s = 0; s < t->num_sections; s++) {
    td_section_t *sec = &t->sections[s];
    for (size_t r = 0; r < t->num_rows; r++) {
      sec->row_heights[r] = 1;
      for (size_t c = 0; c < t->num_cols; c++) {
        size_t h = sec->cell_line_count[r][c];
        sec->row_heights[r] = max_sz(sec->row_heights[r], h ? h : 1);
      }
    }
  }

  t->total_width = 1 + (t->row_label_width + 2) + 1;
  for (size_t c = 0; c < t->num_cols; c++)
    t->total_width += (t->col_widths[c] + 2) + 1;

  size_t title_len = str_display_width(t->title);
  size_t inner = t->total_width - 2;
  if (title_len > inner) {
    size_t extra = title_len - inner + 2;
    size_t pc = extra / t->num_cols, lft = extra % t->num_cols;
    for (size_t c = 0; c < t->num_cols; c++) {
      t->col_widths[c] += pc;
      if (c == 0)
        t->col_widths[c] += lft;
    }
    t->total_width = 1 + (t->row_label_width + 2) + 1;
    for (size_t c = 0; c < t->num_cols; c++)
      t->total_width += (t->col_widths[c] + 2) + 1;
  }

  size_t ch_len = str_display_width(t->col_header);
  size_t ca = 0;
  for (size_t c = 0; c < t->num_cols; c++)
    ca += t->col_widths[c] + 3;
  ca -= 1;
  if (ch_len > ca && t->num_cols > 0) {
    size_t extra = ch_len - ca + 2;
    size_t pc = extra / t->num_cols, lft = extra % t->num_cols;
    for (size_t c = 0; c < t->num_cols; c++) {
      t->col_widths[c] += pc;
      if (c == 0)
        t->col_widths[c] += lft;
    }
    t->total_width = 1 + (t->row_label_width + 2) + 1;
    for (size_t c = 0; c < t->num_cols; c++)
      t->total_width += (t->col_widths[c] + 2) + 1;
  }
}

static void emit_padded(const char *s, size_t width) {
  size_t len = str_display_width(s);
  emit_str(s);
  if (len < width)
    emit_spaces(width - len);
}

static void emit_centered(const char *s, size_t width) {
  size_t len = str_display_width(s);
  if (len >= width) {
    emit_str(s);
    return;
  }
  size_t pad = width - len, left = pad / 2;
  emit_spaces(left);
  emit_str(s);
  emit_spaces(pad - left);
}

static void print_section_header(td_table_t *t, td_section_t *sec,
                                 int is_first) {
  uint8_t fg = get_text_fg(t);
  uint8_t row_bg = get_row_bg(t);
  uint8_t col_bg = get_col_bg(t);
  uint8_t sub_bg = get_subtitle_bg(t);
  uint8_t sub_fg = 16;
  size_t cols_area = t->total_width - 3 - (t->row_label_width + 2);

  if (is_first) {
    emit_str(BOX_LJ);
    emit_horizontal(t->row_label_width + 2);
    emit_str(BOX_TJ);
    emit_horizontal(t->total_width - 3 - (t->row_label_width + 2));
    emit_str(BOX_RJ);
    emit_char('\n');
  } else {
    emit_str(BOX_LJ);
    emit_fg(fg);
    emit_bg(row_bg);
    emit_horizontal(t->row_label_width + 2);
    emit_reset();
    emit_str(BOX_CJ);
    for (size_t c = 0; c < t->num_cols; c++) {
      emit_horizontal(t->col_widths[c] + 2);
      if (c < t->num_cols - 1)
        emit_str(BOX_BJ);
    }
    emit_str(BOX_RJ);
    emit_char('\n');
  }

  /* Subtitle row */
  emit_str(BOX_V);
  emit_fg(sub_fg);
  emit_bg(sub_bg);
  emit_char(' ');
  emit_centered(sec->subtitle, t->row_label_width);
  emit_char(' ');
  emit_reset();
  emit_str(BOX_V);
  emit_fg(fg);
  emit_bg(col_bg);
  emit_centered(t->col_header, cols_area);
  emit_reset();
  emit_str(BOX_V);
  emit_char('\n');

  /* Separator before column labels */
  emit_str(BOX_LJ);
  emit_horizontal(t->row_label_width + 2);
  emit_str(BOX_CJ);
  for (size_t c = 0; c < t->num_cols; c++) {
    emit_fg(fg);
    emit_bg(col_bg);
    emit_horizontal(t->col_widths[c] + 2);
    if (c < t->num_cols - 1)
      emit_str(BOX_TJ);
    emit_reset();
  }
  emit_str(BOX_RJ);
  emit_char('\n');

  /* Column labels row */
  emit_str(BOX_V);
  emit_fg(fg);
  emit_bg(row_bg);
  emit_char(' ');
  emit_padded(t->row_header, t->row_label_width);
  emit_char(' ');
  emit_reset();
  emit_str(BOX_V);
  for (size_t c = 0; c < t->num_cols; c++) {
    emit_fg(fg);
    emit_bg(col_bg);
    emit_char(' ');
    emit_padded(t->col_labels[c], t->col_widths[c]);
    emit_char(' ');
    if (c < t->num_cols - 1)
      emit_str(BOX_V);
    emit_reset();
  }
  emit_str(BOX_V);
  emit_char('\n');

  /* Data rows */
  for (size_t r = 0; r < t->num_rows; r++) {
    /* Row separator */
    emit_str(BOX_LJ);
    emit_fg(fg);
    emit_bg(row_bg);
    emit_horizontal(t->row_label_width + 2);
    emit_reset();
    emit_str(BOX_CJ);
    for (size_t c = 0; c < t->num_cols; c++) {
      emit_horizontal(t->col_widths[c] + 2);
      if (c < t->num_cols - 1)
        emit_str(BOX_CJ);
    }
    emit_str(BOX_RJ);
    emit_char('\n');

    /* Row content */
    for (size_t ln = 0; ln < sec->row_heights[r]; ln++) {
      emit_str(BOX_V);
      emit_fg(fg);
      emit_bg(row_bg);
      emit_char(' ');
      if (ln == 0)
        emit_padded(t->row_labels[r], t->row_label_width);
      else
        emit_spaces(t->row_label_width);
      emit_char(' ');
      emit_reset();

      for (size_t c = 0; c < t->num_cols; c++) {
        emit_str(BOX_V);

        /* Check for cell-specific colors */
        td_cell_color_t *cc = &sec->cell_colors[r][c];
        if (cc->fg)
          emit_fg(cc->fg);
        if (cc->bg)
          emit_bg(cc->bg);

        emit_char(' ');
        if (ln < sec->cell_line_count[r][c])
          emit_padded(sec->cells[r][c][ln], t->col_widths[c]);
        else
          emit_spaces(t->col_widths[c]);
        emit_char(' ');

        if (cc->fg || cc->bg)
          emit_reset();
      }
      emit_str(BOX_V);
      emit_char('\n');
    }
  }
}

static void print_section(td_table_t *t, size_t s, int is_first) {
  print_section_header(t, &t->sections[s], is_first);
}

void td_print(td_table_t *t) {
  size_t inner_width = t->total_width - 2;
  uint8_t fg = get_text_fg(t);
  uint8_t title_bg = get_title_bg(t);

  cur_table = t;
  out_pos = 0;

  emit_str(BOX_TL);
  emit_horizontal(inner_width);
  emit_str(BOX_TR);
  emit_char('\n');

  emit_str(BOX_V);
  emit_fg(fg);
  emit_bg(title_bg);
  emit_centered(t->title, inner_width);
  emit_reset();
  emit_str(BOX_V);
  emit_char('\n');

  for (size_t s = 0; s < t->num_sections; s++)
    print_section(t, s, (s == 0));

  emit_str(BOX_BL);
  emit_horizontal(t->row_label_width + 2);
  for (size_t c = 0; c < t->num_cols; c++) {
    emit_str(BOX_BJ);
    emit_horizontal(t->col_widths[c] + 2);
  }
  emit_str(BOX_BR);
  emit_char('\n');

  flush_output();
  cur_table = NULL;
}
