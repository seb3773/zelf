#ifndef _CSC_TYPEDEF_H_
#define _CSC_TYPEDEF_H_

#include "csc_common.h"
#include <stdint.h>

#define KB 1024
#define MB 1048576
#define MinBlockSize (8 * KB)

#define MaxDictSize (1024 * 1024 * 1024)
#define MinDictSize (32 * 1024)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/******Block Type*************/
#define DT_NONE 0x00
#define DT_NORMAL 0x01
#define DT_ENGTXT 0x02
#define DT_EXE 0x03
#define DT_FAST 0x04

///////////////////////////
#define DT_NO_LZ 0x05

#define DT_ENTROPY 0x07
#define DT_BAD 0x08
#define SIG_EOF 0x09
#define DT_DLT 0x10
#define DLT_CHANNEL_MAX 5

// Use static for C header inclusion to avoid multiple defs, or extern.
// For simplicity in this split, static const is easiest.
static const uint32_t DltIndex[DLT_CHANNEL_MAX] = {1, 2, 3, 4, 8};

// DT_SKIP means same with last one
#define DT_SKIP 0x1E
#define DT_MAXINVALID 0x1F

#endif
