CC=gcc
CFLAGS=-O3 -Wall -I../../compressor -I../../compressor/libdivsufsort/include
AR=ar
ARFLAGS=rcs

SRCS=lzsa2_compress.c \
     ../../compressor/shrink_inmem.c \
     ../../compressor/shrink_block_v1.c \
     ../../compressor/shrink_block_v2.c \
     ../../compressor/shrink_context.c \
     ../../compressor/matchfinder.c \
     ../../compressor/frame.c \
     ../../compressor/libdivsufsort/lib/divsufsort.c \
     ../../compressor/libdivsufsort/lib/divsufsort_utils.c \
     ../../compressor/libdivsufsort/lib/sssort.c \
     ../../compressor/libdivsufsort/lib/trsort.c

OBJS=$(SRCS:.c=.o)

all: liblzsa2_compress.a

liblzsa2_compress.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) liblzsa2_compress.a

.PHONY: all clean
