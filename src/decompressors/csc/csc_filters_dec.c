#include "csc_internal_dec.h"
#include <string.h>

static const uint32_t wordNum = 123;

static const char wordList[123][8] = {
    "",     "ac",   "ad",  "ai",  "al",  "am",   "an",   "ar",   "as",   "at",
    "ea",   "ec",   "ed",  "ee",  "el",  "en",   "er",   "es",   "et",   "id",
    "ie",   "ig",   "il",  "in",  "io",  "is",   "it",   "of",   "ol",   "on",
    "oo",   "or",   "os",  "ou",  "ow",  "ul",   "un",   "ur",   "us",   "ba",
    "be",   "ca",   "ce",  "co",  "ch",  "de",   "di",   "ge",   "gh",   "ha",
    "he",   "hi",   "ho",  "ra",  "re",  "ri",   "ro",   "rs",   "la",   "le",
    "li",   "lo",   "ld",  "ll",  "ly",  "se",   "si",   "so",   "sh",   "ss",
    "st",   "ma",   "me",  "mi",  "ne",  "nc",   "nd",   "ng",   "nt",   "pa",
    "pe",   "ta",   "te",  "ti",  "to",  "th",   "tr",   "wa",   "ve",   "all",
    "and",  "but",  "dow", "for", "had", "hav",  "her",  "him",  "his",  "man",
    "mor",  "not",  "now", "one", "out", "she",  "the",  "was",  "wer",  "whi",
    "whe",  "wit",  "you", "any", "are", "that", "said", "with", "have", "this",
    "from", "were", "tion"};

static const uint8_t maxSymbol = 0xFC;

void CSC_Filters_Init() {
}

void CSC_Filters_Inverse_Dict(uint8_t *src, uint32_t size, uint8_t *swap_buf) {
  if (size == 0)
    return;

  uint8_t *dst = swap_buf;
  uint32_t i = 0, j;
  uint32_t dstPos = 0, idx;

  while (dstPos < size) {
    if (src[i] >= 0x82 && src[i] < maxSymbol) {
      idx = src[i] - 0x82 + 1;
      for (j = 0; wordList[idx][j] && dstPos < size; j++) {
        dst[dstPos++] = wordList[idx][j];
      }
    } else if (src[i] == 254 && (i + 1 < size && src[i + 1] >= 0x82)) {
      i++;
      dst[dstPos++] = src[i];
    } else {
      dst[dstPos++] = src[i];
    }
    i++;
  }
  memcpy(src, dst, size);
}

void CSC_Filters_Inverse_Delta(uint8_t *src, uint32_t size, uint32_t chnNum,
                               uint8_t *swap_buf) {
  uint32_t dstPos, i, j;
  uint8_t prevByte;

  if (size < 512)
    return;

  memcpy(swap_buf, src, size);

  dstPos = 0;
  prevByte = 0;
  for (i = 0; i < chnNum; i++) {
    for (j = i; j < size; j += chnNum) {
      src[j] = swap_buf[dstPos++] + prevByte;
      prevByte = src[j];
    }
  }
}

// E89 Implementation
static uint32_t E89yswap(uint32_t x) {
  x = ((uint8_t)(x >> 24) << 7) | ((uint8_t)(x >> 16) << 8) |
      ((uint8_t)(x >> 8) << 16) | (x << 24);
  return x >> 7;
}

typedef struct {
  uint32_t x0, x1;
  uint32_t i, k;
  uint8_t cs;
} E89State;

static int32_t E89cache_byte(E89State *s, int32_t c) {
  int32_t d = s->cs & 0x80 ? -1 : (uint8_t)(s->x1);
  s->x1 >>= 8;
  s->x1 |= (s->x0 << 24);
  s->x0 >>= 8;
  s->x0 |= (c << 24);
  s->cs <<= 1;
  s->i++;
  return d;
}

static int32_t E89inverse(E89State *s, int32_t c) {
  uint32_t x;
  if (s->i >= s->k) {
    if ((s->x1 & 0xFE000000) == 0xE8000000) {
      s->k = s->i + 4;
      x = s->x0 - 0xFF000000;
      if (x < 0x02000000) {
        x = E89yswap(x);
        x = (x - s->i) & 0x01FFFFFF;
        s->x0 = x + 0xFF000000;
      }
    }
  }
  return E89cache_byte(s, c);
}

static int32_t E89flush(E89State *s) {
  int32_t d;
  if (s->cs != 0xFF) {
    while (s->cs & 0x80) {
      E89cache_byte(s, 0);
      ++s->cs;
    }
    d = E89cache_byte(s, 0);
    ++s->cs;
    return d;
  } else {
    s->cs = 0xFF;
    s->x0 = s->x1 = 0;
    s->i = 0;
    s->k = 5;
    return -1;
  }
}

void CSC_Filters_Inverse_E89(uint8_t *src, uint32_t size) {
  uint32_t i, j;
  int32_t c;
  E89State s;

  // Init
  s.cs = 0xFF;
  s.x0 = s.x1 = 0;
  s.i = 0;
  s.k = 5;

  for (i = 0, j = 0; i < size; i++) {
    c = E89inverse(&s, src[i]);
    if (c >= 0)
      src[j++] = c;
  }
  while ((c = E89flush(&s)) >= 0)
    src[j++] = c;
}
