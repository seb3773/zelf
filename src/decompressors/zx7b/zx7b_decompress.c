/*
 * ZX7b Decompressor - Ultra-compact C implementation by Seb3773
 * Based on ZX7 by Einar Saukas / Antonio Villena
 *
 * Target size: ~150 bytes
 */

#include "zx7b_decompress.h"

typedef struct {
  const unsigned char *in;
  unsigned char *out;
  int in_pos, out_pos;
  int bit_mask, bit_value;
} ctx_t;

static int read_byte(ctx_t *c) { return c->in[--c->in_pos]; }

static int read_bit(ctx_t *c) {
  c->bit_mask >>= 1;
  if (c->bit_mask == 0) {
    c->bit_mask = 128;
    c->bit_value = read_byte(c);
  }
  return (c->bit_value & c->bit_mask) ? 1 : 0;
}

/* Read Elias Gamma code */
static int read_gamma(ctx_t *c) {
  int value = 1;
  while (!read_bit(c)) {
    value = (value << 1) | read_bit(c);
  }
  if ((value & 255) == 255)
    return -1; /* End marker */
  return value;
}

/* Read offset value */
static int read_offset(ctx_t *c) {
  int value = read_byte(c);
  if (value < 128) {
    return value;
  } else {
    int i = read_bit(c);
    i = (i << 1) | read_bit(c);
    i = (i << 1) | read_bit(c);
    i = (i << 1) | read_bit(c);
    return ((value & 127) | (i << 7)) + 128;
  }
}

static void write_byte(ctx_t *c, int value) {
  if (c->out_pos <= 0)
    return; /* Underflow protection */
  c->out[--c->out_pos] = value;
}

static void copy_match(ctx_t *c, int offset, int length) {
  /* In backwards mode, previous bytes are at higher addresses */
  /* We want to copy from (current_write_pos + offset) */
  while (length-- > 0) {
    /* Check bounds if necessary, but speed is priority for stubs */
    /* Assuming valid stream and sufficient buffer */
    if (c->out_pos <= 0)
      break;
    unsigned char val = c->out[c->out_pos + offset - 1];
    write_byte(c, val);
  }
}

/* Native backwards decompression */
int zx7b_decompress(const unsigned char *in, int in_size, unsigned char *out,
                    int out_max) {
  ctx_t c;
  int length;

  if (!in || !out || in_size <= 0 || out_max <= 0)
    return -1;

  /* Initialize context for backwards processing */
  c.in = in;
  c.out = out;
  c.in_pos = in_size;
  c.out_pos = out_max;
  c.bit_mask = 0;
  c.bit_value = 0;

  /* First byte is literal */
  write_byte(&c, read_byte(&c));

  /* Decompress loop */
  while (1) {
    if (!read_bit(&c)) {
      /* Literal byte */
      write_byte(&c, read_byte(&c));
    } else {
      /* Match: length + offset */
      length = read_gamma(&c) + 1;
      if (length == 0) {
        /* End marker */
        break;
      }
      copy_match(&c, read_offset(&c) + 1, length);
    }

    /* Safety limit */
    if (c.out_pos <= 0)
      break;
  }

  /* Calculate actual decompressed size */
  int decomp_size = out_max - c.out_pos;

  /* Move data to the beginning of the buffer if it's not already there */
  if (c.out_pos > 0) {
    int i;
    /* Forward copy is safe because we are moving to lower addresses */
    /* (src > dst), so src is always ahead of dst write ptr */
    for (i = 0; i < decomp_size; i++) {
      out[i] = out[c.out_pos + i];
    }
  }

  return decomp_size;
}
