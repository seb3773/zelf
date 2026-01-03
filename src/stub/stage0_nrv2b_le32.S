// stage0_nrv2b_le32.S
// PIC stage-0: read header, decompress NRV2B LE32 blob, jump to decompressed stub.
// Header layout immediately after this code:
//   u32 uncompressed_len
//   u32 compressed_len
//   u64 dst_offset_from_base   (relative to stage0 base)
//   [compressed data...]
//
// Inputs: executed as entry of stub segment.
// Assumptions:
//   - p_memsz reserves enough space for dst (ulen) beyond the file payload.
//   - No relocations; RIP-relative only.
//   - nrvo (dst) region is RW before jump; caller will later execute RX.

    .section .text.stage0,"ax",@progbits
    .global stage0_nrv2b_entry
    .type stage0_nrv2b_entry, @function

stage0_nrv2b_entry:
    mov     %rdx, %r12               // preserve kernel-provided rdx for the decompressed stub
    lea     stage0_nrv2b_entry(%rip), %r15 // r15 = base (start of stage0)
    // Header is placed immediately after code
    // Read uncompressed_len (u32), compressed_len (u32), dst_off (u64)
    mov     0f(%rip), %eax           // ulen
    mov     4f(%rip), %edx           // clen
    mov     8f(%rip), %rcx           // dst_off
    mov     16f(%rip), %r8           // blob_off
    // Build pointers
    lea     (%r15,%r8), %rdi         // rdi = src = base + blob_off
    mov     %edx, %esi               // rsi = src_len = clen
    lea     (%r15,%rcx), %rdx        // rdx = dst = base + dst_off
    mov     %rdx, %r14               // keep dst across call (callee-saved)
    // Prepare dst_len pointer on stack
    push    %rax                     // push ulen
    mov     %rsp, %rcx               // rcx = dst_len*
    // Call C decoder: int stage0_nrv2b_le32(src, src_len, dst, dst_len)
    call    stage0_nrv2b_le32
    add     $8, %rsp                 // pop dst_len
    test    %eax, %eax
    jne     stage0_fail
    mov     %r12, %rdx               // restore initial rdx (rtld_fini / kernel pointer)
    // Jump to dst (decompressed stub entry assumed at its start)
    jmp     *%r14

stage0_fail:
    // Best-effort abort: use ud2
    ud2

// Header anchors (RIP-relative) and exported labels for patching
    .global stage0_hdr_ulen
    .global stage0_hdr_clen
    .global stage0_hdr_dst
    .global stage0_hdr_blob
stage0_hdr_ulen:
0:  .long 0              // ulen (patched by packer)
stage0_hdr_clen:
4:  .long 0              // clen (patched by packer)
stage0_hdr_dst:
8:  .quad 0              // dst_off (patched by packer)
stage0_hdr_blob:
16: .quad 0              // blob_off (patched by packer)
24:
    .size stage0_nrv2b_entry, .-stage0_nrv2b_entry


