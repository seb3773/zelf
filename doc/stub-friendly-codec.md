Detailed Constraints for "Stub-Friendly" Decompressors:

A "stub-friendly codec" must adhere to the following constraints (this simplifies integration and prevents breaking nostdlib stubs):

## Interface Model
The decompressor must fit this model to be usable in the zelf stubs:

Input: Compressed buffer + size
Output: Destination buffer + capacity
Return: Produced size (≥0); on error, return -1 (or exit in the stub if you want to "fail hard")

## Memory Allocation / Dependencies
The stub is designed to be nostdlib/minimalist:

Avoid malloc/free, stdio, pthread, etc.
If a workspace is needed:

Either do everything "in-place" (no workspace)
Or allocate via syscalls (e.g., mmap), as the LZHAM codec does (see z_syscall_mmap/z_syscall_munmap)

No dependencies on external libraries (or they must be "mini-ported" to C and compiled into the stub)
Size / Code Style

The stub must remain compact! So:
Avoid as much as possible large unnecessary tables, heavy CRC/IO, etc.
Simple C code, no exotic ABI; ideally no deep recursion

## Determinism & Validation

Must be deterministic and never read outside src[0..compressedSize-1] or write outside dst[0..dstCapacity-1]
Clear error handling (return < 0), as stubs don’t "recover" from errors
