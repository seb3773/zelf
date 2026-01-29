#!/bin/bash
set -e

# Compile ASM
nasm -f elf64 decomp/shrinkler_decompress_x64.asm -o shrinkler_decompress_x64.o

# Compile Compressor
g++ -c comp/shrinkler_comp.cpp -Icomp -O2 -o shrinkler_comp.o

# Compile Decompressor Wrapper
gcc -c decomp/shrinkler_decompress.c -Idecomp -O2 -o shrinkler_decompress.o

# Compile and Link Test
g++ test.cpp shrinkler_comp.o shrinkler_decompress.o shrinkler_decompress_x64.o -o test_shrinkler -Icomp -Idecomp -O2

echo "Build successful!"
./test_shrinkler
