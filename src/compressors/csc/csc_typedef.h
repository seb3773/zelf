#ifndef _CSC_TYPEDEF_H_
#define _CSC_TYPEDEF_H_

#include <stdint.h>
#include <csc_common.h>

#define KB 1024u
#define MB 1048576u
#define MinBlockSize (8u * KB)

#define MaxDictSize (1024u * MB)
#define MinDictSize (32u * KB)

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))


/******Block Type*************/
#define DT_NONE 0x00u
#define DT_NORMAL 0x01u
#define DT_ENGTXT 0x02u
#define DT_EXE 0x03u
#define DT_FAST 0x04u

///////////////////////////
#define DT_NO_LZ 0x05u

//const uint32_t DT_AUDIO = 0x06;
//const uint32_t DT_AUDIO = 0x06;
#define DT_ENTROPY 0x07u
#define DT_BAD 0x08u
#define SIG_EOF 0x09u
#define DT_DLT 0x10u
#define DLT_CHANNEL_MAX 5u
static const uint32_t DltIndex[DLT_CHANNEL_MAX] = {1u, 2u, 3u, 4u, 8u};

// DT_SKIP means same with last one
#define DT_SKIP 0x1Eu
#define DT_MAXINVALID 0x1Fu
/******Block Type*************/


#endif
