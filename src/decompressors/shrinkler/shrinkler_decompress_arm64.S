/*
 * Shrinkler Decompressor - ARM64 port
 *
 * Based on x86_64 port by Seb3773 and original C++ code by Aske Simon Christensen.
 */

.arch armv8-a
.text
.global ShrinklerDecompress

// Constants
.equ INIT_ONE_PROB, 0x8000
.equ ADJUST_SHIFT, 4
.equ NUM_CONTEXTS, 1536
.equ NUM_SINGLE_CONTEXTS, 1

.equ CONTEXT_KIND, 0
.equ CONTEXT_REPEATED, -1
.equ CONTEXT_GROUP_LIT, 0
.equ CONTEXT_GROUP_OFFSET, 2
.equ CONTEXT_GROUP_LENGTH, 3

// Register Mapping
// Arguments:
// x0: src
// x1: dst
// x2: callback
// x3: cb_arg
// x4: contexts

// Preserved State (x19-x28):
#define s_src       x19
#define s_dst       x20
#define s_interval  w21
#define s_value     w22
#define s_bitbuf    w23
#define s_bitcount  w24
#define s_ctx       x25
#define s_cb        x26
#define s_cbarg     x27
#define s_dstbase   x28
// s_dstbase is spilled to stack to free x28 for s_offset.
#undef s_dstbase
#define s_offset    x28

// Stack Frame Layout:
// Size: 112 bytes
// [x29, #0] : FP, LR
// [x29, #16]: x19, x20
// [x29, #32]: x21, x22
// [x29, #48]: x23, x24
// [x29, #64]: x25, x26
// [x29, #80]: x27, x28
// [x29, #96]: saved_dstbase

ShrinklerDecompress:
    // Save registers and setup frame
    stp x29, x30, [sp, #-112]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48]
    stp x25, x26, [sp, #64]
    stp x27, x28, [sp, #80]

    // Init Arguments
    mov s_src, x0
    mov s_dst, x1
    mov s_cb, x2
    mov s_cbarg, x3
    // Save dstbase (x1) to stack
    str x1, [x29, #96]

    // Init Contexts
    // x4 points to buffer
    mov w0, INIT_ONE_PROB
    mov w1, NUM_CONTEXTS
    mov x2, x4
init_ctx_loop:
    str w0, [x2], #4
    subs w1, w1, #1
    b.ne init_ctx_loop

    // Adjust context pointer
    add s_ctx, x4, #(NUM_SINGLE_CONTEXTS * 4)

    // Init Range Decoder
    mov s_interval, #1
    mov s_value, #0
    mov s_bitcount, #0 
    mov s_bitbuf, #0
    mov s_offset, #0

    // Main Loop
lz_decode_literal:
    mov w9, #1

literal_loop:
    mov w0, w9
    bl range_decode_bit
    lsl w9, w9, #1
    orr w9, w9, w0
    tst w9, #0x100
    b.eq literal_loop

    strb w9, [s_dst], #1

    // Callback
    cbz s_cb, no_cb_lit
    bl report_progress
no_cb_lit:

    // Check Kind
    mov w0, #CONTEXT_KIND
    bl range_decode_bit
    cbz w0, lz_decode_literal

lz_decode_reference:
    mov w0, #CONTEXT_REPEATED
    bl range_decode_bit
    cbnz w0, lz_decode_read_length // if 1, use s_offset (preserved)

lz_decode_read_offset:
    mov w0, #(CONTEXT_GROUP_OFFSET << 8)
    bl range_decode_number
    // x0 is number
    sub x0, x0, #2
    mov s_offset, x0
    cbnz s_offset, lz_decode_read_length
    
    // Done
    ldr x1, [x29, #96] // load dstbase
    sub x0, s_dst, x1
    b exit_func

lz_decode_read_length:
    mov w0, #(CONTEXT_GROUP_LENGTH << 8)
    bl range_decode_number
    mov x12, x0  // length

    // Copy Loop
    sub x13, s_dst, s_offset
copy_loop:
    ldrb w14, [x13], #1
    strb w14, [s_dst], #1
    subs x12, x12, #1
    b.ne copy_loop

    // Callback
    cbz s_cb, no_cb_ref
    bl report_progress
no_cb_ref:

    // Check Kind
    mov w0, #CONTEXT_KIND
    bl range_decode_bit
    cbnz w0, lz_decode_read_offset // CORRECTED: Skip REPEATED check if prev was ref
    b lz_decode_literal


// Helper: RangeDecodeBit
range_decode_bit:
    ldr w1, =0x8000
norm_loop:
    cmp s_interval, w1
    b.ge normalized
    lsl s_interval, s_interval, #1
    and s_interval, s_interval, #0xFFFF
    
    subs s_bitcount, s_bitcount, #1
    b.pl got_bit
    ldrb s_bitbuf, [s_src], #1
    mov s_bitcount, #7
got_bit:
    lsr w2, s_bitbuf, s_bitcount
    and w2, w2, #1
    lsl s_value, s_value, #1
    orr s_value, s_value, w2
    and s_value, s_value, #0xFFFF
    b norm_loop

normalized:
    sxtw x3, w0
    ldr w2, [s_ctx, x3, lsl #2] // prob

    mul w4, s_interval, w2
    lsr w4, w4, #16 // threshold

    cmp s_value, w4
    b.hs decode_zero

decode_one:
    lsr w5, w2, #ADJUST_SHIFT
    sub w2, w2, w5
    mov w6, #(0xFFFF >> ADJUST_SHIFT)
    add w2, w2, w6
    str w2, [s_ctx, x3, lsl #2]
    
    mov s_interval, w4
    mov w0, #1
    ret

decode_zero:
    lsr w5, w2, #ADJUST_SHIFT
    sub w2, w2, w5
    str w2, [s_ctx, x3, lsl #2]
    
    sub s_value, s_value, w4
    sub s_interval, s_interval, w4
    
    and s_value, s_value, #0xFFFF
    and s_interval, s_interval, #0xFFFF
    
    mov w0, #0
    ret


// Helper: RangeDecodeNumber
range_decode_number:
    str x30, [sp, #-16]!
    mov w5, w0
    mov w6, #0 // i

unary_loop:
    add w0, w5, w6, lsl #1
    add w0, w0, #2
    stp x5, x6, [sp, #-16]!
    bl range_decode_bit
    ldp x5, x6, [sp], #16
    
    cbz w0, unary_done
    add w6, w6, #1
    b unary_loop

unary_done:
    // sub w5, w5, #1 // REMOVED (fix off-by-one)
    mov w7, #1 // number
    
binary_loop:
    cmp w6, #0
    b.lt number_done
    
    add w0, w5, w6, lsl #1
    add w0, w0, #1
    
    stp x5, x6, [sp, #-16]!
    stp x0, x7, [sp, #-16]!
    bl range_decode_bit
    ldp x8, x7, [sp], #16 // Preservation fix
    ldp x5, x6, [sp], #16
    
    lsl w7, w7, #1
    orr w7, w7, w0
    
    sub w6, w6, #1
    b binary_loop
    
number_done:
    mov x0, x7
    ldr x30, [sp], #16
    ret


report_progress:
    str x30, [sp, #-16]!
    ldr x8, [x29, #96] // load dstbase
    sub x0, s_dst, x8
    mov x1, s_cbarg
    blr s_cb
    ldr x30, [sp], #16
    ret

exit_func:
    ldp x27, x28, [sp, #80]
    ldp x25, x26, [sp, #64]
    ldp x23, x24, [sp, #48]
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #112
    ret
