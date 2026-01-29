.section .text.stage0,"ax",%progbits
.global stage0_nrv2b_entry
.type stage0_nrv2b_entry, %function

stage0_nrv2b_entry:
    mov     x19, x0
    adr     x20, stage0_nrv2b_entry

    ldr     w21, [x20, #(stage0_hdr_ulen - stage0_nrv2b_entry)]
    ldr     w22, [x20, #(stage0_hdr_clen - stage0_nrv2b_entry)]
    ldr     x23, [x20, #(stage0_hdr_dst - stage0_nrv2b_entry)]
    ldr     x24, [x20, #(stage0_hdr_blob - stage0_nrv2b_entry)]

    add     x25, x20, x23
    add     x0, x20, x24
    mov     w1, w22
    mov     x2, x25

    sub     sp, sp, #16
    str     w21, [sp]
    mov     x3, sp

    bl      stage0_nrv2b_le32

    add     sp, sp, #16
    cbnz    w0, stage0_fail

    // Ensure coherency for self-modifying code on AArch64.
    // The decompressed stub is written via the data cache, but will be executed
    // via the instruction cache. Flush [x25, x25 + ulen) before branching.
    mov     x26, x25
    add     x27, x25, w21, uxtw
    bic     x26, x26, #63
    add     x27, x27, #63
    bic     x27, x27, #63
1:
    dc      cvau, x26
    add     x26, x26, #64
    cmp     x26, x27
    b.lo    1b
    dsb     ish
    mov     x26, x25
    bic     x26, x26, #63
2:
    ic      ivau, x26
    add     x26, x26, #64
    cmp     x26, x27
    b.lo    2b
    dsb     ish
    isb

    mov     x0, x19
    br      x25

stage0_fail:
    brk     #0

.global stage0_hdr_ulen
.global stage0_hdr_clen
.global stage0_hdr_dst
.global stage0_hdr_blob
.global stage0_hdr_flags

stage0_hdr_ulen:
    .long 0
stage0_hdr_clen:
    .long 0

    .balign 8
stage0_hdr_dst:
    .quad 0
stage0_hdr_blob:
    .quad 0
stage0_hdr_flags:
    .byte 0

.size stage0_nrv2b_entry, .-stage0_nrv2b_entry

.section .note.GNU-stack,"",%progbits
