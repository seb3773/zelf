#include "csc_compressor.h"
#include "csc_enc.h"
#include <stdlib.h>
#include <string.h>

extern "C" {

struct MemInStream {
  ISeqInStream s;
  const uint8_t *cur;
  const uint8_t *end;
};

static SRes MemInStream_Read(void *p, void *buf, size_t *size) {
  MemInStream *s = (MemInStream *)p;
  size_t rem = s->end - s->cur;
  if (*size > rem) {
    *size = rem;
  }
  if (*size > 0) {
    memcpy(buf, s->cur, *size);
    s->cur += *size;
  }
  return SZ_OK;
}

struct MemOutStream {
  ISeqOutStream s;
  uint8_t *cur;
  uint8_t *end;
  int overflow;
};

static size_t MemOutStream_Write(void *p, const void *buf, size_t size) {
  MemOutStream *s = (MemOutStream *)p;
  size_t rem = s->end - s->cur;
  if (size > rem) {
    s->overflow = 1;
    return 0; // Error: buffer overflow
  }
  memcpy(s->cur, buf, size);
  s->cur += size;
  return size;
}

struct ProgressStub {
  ICompressProgress s;
};

static SRes ProgressStub_Progress(void *p, UInt64 inSize, UInt64 outSize) {
  (void)p;
  (void)inSize;
  (void)outSize;
  return SZ_OK;
}

// Default allocator wrapper
static void *SzAlloc(void *p, size_t size) {
  (void)p;
  return malloc(size);
}
static void SzFree(void *p, void *address) {
  (void)p;
  free(address);
}
static ISzAlloc g_Alloc = {SzAlloc, SzFree};

size_t CSC_Compress(const void *src, size_t src_len, void *dst,
                    size_t dst_capacity, const CSCCompressOptions *opts) {
  if (!src || !dst)
    return 0;

  CSCProps props;
  uint32_t dict_size = 64 * 1024 * 1024;
  int level = 2;

  if (opts) {
    if (opts->dict_size >= 32 * 1024)
      dict_size = opts->dict_size;
    if (opts->level >= 1 && opts->level <= 5)
      level = opts->level;
  }

  // Initialize source stream
  MemInStream is;
  is.s.Read = MemInStream_Read;
  is.cur = (const uint8_t *)src;
  is.end = is.cur + src_len;

  // Initialize destination stream
  MemOutStream os;
  os.s.Write = MemOutStream_Write;
  os.cur = (uint8_t *)dst;
  os.end = os.cur + dst_capacity;
  os.overflow = 0;

  // Progress stub
  ProgressStub progress;
  progress.s.Progress = ProgressStub_Progress;

  // Init props
  // IMPORTANT: clamp dict BEFORE CSCEncProps_Init so all derived fields are
  // consistent. Do not mutate props.dict_size after init.
  uint32_t eff_dict = dict_size;
  if (src_len < (size_t)eff_dict) {
    eff_dict = (uint32_t)src_len;
    if (eff_dict < 32 * 1024)
      eff_dict = 32 * 1024;
  }
  CSCEncProps_Init(&props, eff_dict, level);

  // Write properties to start of stream (CSC header)
  // CSCEnc_WriteProperties writes CSC_PROP_SIZE (10 bytes)
  if (dst_capacity < CSC_PROP_SIZE)
    return 0;

  // We write props directly to output stream via fwrite in original code,
  // but here we can write to buffer directly or use the stream.
  // However, the stream position is advanced by calls.
  // Let's use temporary buffer for props then write to stream to keep valid
  // state if needed, or just direct write if we trust our stream impl.
  uint8_t propBuf[CSC_PROP_SIZE];
  CSCEnc_WriteProperties(&props, propBuf, 0);

  if (os.s.Write(&os, propBuf, CSC_PROP_SIZE) != CSC_PROP_SIZE)
    return 0;

  // Create encoder
  CSCEncHandle h = CSCEnc_Create(&props, (ISeqOutStream *)&os, &g_Alloc);
  if (!h)
    return 0;

  int res =
      CSCEnc_Encode(h, (ISeqInStream *)&is, (ICompressProgress *)&progress);

  // Flush
  if (res == SZ_OK) {
    res = CSCEnc_Encode_Flush(h);
  }

  CSCEnc_Destroy(h);

  if (res != SZ_OK || os.overflow) {
    return 0;
  }

  return (size_t)(os.cur - (uint8_t *)dst);
}
}
