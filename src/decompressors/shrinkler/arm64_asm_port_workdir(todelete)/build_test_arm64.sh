#!/bin/bash
set -e

echo "Building for $(uname -m)..."

# Compile ASM
gcc -c decomp/shrinkler_decompress_arm64.S -o shrinkler_decompress_arm64.o

# Compile Compressor
g++ -c comp/shrinkler_comp.cpp -Icomp -O2 -o shrinkler_comp.o

# Compile Decompressor Wrapper
gcc -c decomp/shrinkler_decompress.c -Idecomp -O2 -o shrinkler_decompress.o

# Compile and Link Test
g++ test.cpp shrinkler_comp.o shrinkler_decompress.o shrinkler_decompress_arm64.o -o test_shrinkler_arm64 -Icomp -Idecomp -O2

echo "Build successful!"
./test_shrinkler_arm64
