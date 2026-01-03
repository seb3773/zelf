CC=gcc
CFLAGS=-O3 -Wall -DNDEBUG -I../../decompressor -I../../decompressor/libdivsufsort/include
AR=ar
ARFLAGS=rcs

SRCS=lzsa2_decompress.c \
     ../../decompressor/expand_inmem.c \
     ../../decompressor/expand_block_v1.c \
     ../../decompressor/expand_block_v2.c \
     ../../decompressor/expand_context.c \
     ../../compressor/frame.c

OBJS=$(SRCS:.c=.o)

all: liblzsa2_decompress.a

liblzsa2_decompress.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) liblzsa2_decompress.a

.PHONY: all clean
