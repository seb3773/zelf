#ifndef STUB_VARS_H
#define STUB_VARS_H

#define VIRTUAL_START 0x5100
#define COMPRESSED_SIZE_PLACEHOLDER 0xDEADBEEF
#define DECOMPRESSED_SIZE_PLACEHOLDER 0xCAFEBABE
#define PACKED_DATA_ADDR 0x00005200
#define SEGMENT_COUNT 0

typedef struct {
    unsigned long page_base;
    unsigned long page_offset;
    unsigned long map_size;
    unsigned int prot;
    unsigned long data_offset;
    unsigned long filesz;
    unsigned long memsz;
} SegmentDesc;

static const SegmentDesc segments[] = {};

#endif
