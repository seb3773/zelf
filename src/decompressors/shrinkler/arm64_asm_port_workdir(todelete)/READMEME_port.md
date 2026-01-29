Shrinkler Decompressor ARM64 Port Walkthrough
I have successfully ported the Shrinkler decompressor from x86_64 assembly to ARM64 assembly. The port allows for decompression of Shrinkler-packed data on ARMv8-A architecture (Aarch64).

1. Porting Strategy
The original x86_64 assembly (
shrinkler_decompress_x64.asm
) was manually transpiled to ARM64 assembly (
shrinkler_decompress_arm64.S
). Key aspects of the port include:

Register Mapping:

RDI (src) -> x0 (s_src/x19)
RSI (dst) -> x1 (s_dst/x20)
RCX/RDX (Range Decoder State) -> w21 (s_interval) / w22 (s_value)
RBX (offset) -> x28 (s_offset, with stack spilling for dstbase)
Range Decoder Logic:

Adapted the x86 32-bit arithmetic logic to ARM64, ensuring 16-bit masking (0xFFFF) matches the reference implementation exactly.
Implemented the mul and shift operations to replicate the probability update mechanism.
2. Debugging & Verification
The porting process involved significant debugging to match the bit-exact behavior of the x86 reference implementation.

Issues Encountered & Fixed:
Segfaults: Initial stack frame management issues were resolved by properly saving/restoring callee-saved registers and ensuring 16-byte stack alignment.
Context Calculation Error: A subtle off-by-one error in lz_decode_read_offset where RangeDecodeNumber's loop logic for context updates was slightly different in the initial ARM translation.
Control Flow Logic: The most critical bug was unrelated to arithmetic but to control flow. The ARM64 port initially checked for a "repeated offset" even after decoding a reference, whereas the Shrinkler protocol (and the x86 ASM) skips this check if the previous symbol was already a reference. This was fixed by jumping correctly to lz_decode_read_offset vs lz_decode_reference.
Verification Code:
A 
test.cpp
 harness was created to:

Compress a sample string using the reference C++ compressor (
shrinkler_comp.cpp
).
Decompress it using the new ARM64 assembly decompressor.
Verify the output matches the original input byte-for-byte.
3. Final Result
The ARM64 decompressor is now fully functional and verified.

Source File: 
decomp/shrinkler_decompress_arm64.S
Build Script: 
build_test_arm64.sh
Verification: Verification SUCCESS! output from the test harness.
The resulting code is a standalone assembly file suitable for integration into ELF stubs or other bare-metal/minimal environments on ARM64 Linux.
