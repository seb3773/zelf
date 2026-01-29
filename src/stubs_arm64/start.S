.section .text
.global _start
.type _start, %function

.extern elfz_main_wrapper

// Stack structure (uint64_t slots):
// [sp+0]  = entry_point (to fill)
// [sp+8]  = argc_ptr (to fill)
// [sp+16] = original_sp
// [sp+24] = stub_runtime_addr
// [sp+32] = saved_x0 (rtld_fini / atexit func ptr for dynamic entry ABI)

_start:
    // Save x0 immediately (rtld_fini in AArch64 ABI)
    mov x19, x0

    // Save original sp
    mov x20, sp

    // Runtime address of _start (PC-relative)
    adr x21, _start

    // Reserve stack space (48 bytes, 16-byte aligned)
    sub sp, sp, #48

    // Store original_sp, stub_runtime_addr, saved_x0
    str x20, [sp, #16]
    str x21, [sp, #24]
    str x19, [sp, #32]

    // Call wrapper(stack_frame)
    mov x0, sp
    bl elfz_main_wrapper

    // Load entry_point and argc_ptr produced by wrapper
    ldr x18, [sp, #0]      // entry_point
    ldr x17, [sp, #8]      // argc_ptr

    // Restore sp to argc_ptr
    mov sp, x17

    // Zero registers (best-effort, keep x18 as jump target)
    mov x0, xzr
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
    mov x4, xzr
    mov x5, xzr
    mov x6, xzr
    mov x7, xzr
    mov x8, xzr
    mov x9, xzr
    mov x10, xzr
    mov x11, xzr
    mov x12, xzr
    mov x13, xzr
    mov x14, xzr
    mov x15, xzr
    mov x16, xzr
    mov x17, xzr
    mov x19, xzr
    mov x20, xzr
    mov x21, xzr
    mov x22, xzr
    mov x23, xzr
    mov x24, xzr
    mov x25, xzr
    mov x26, xzr
    mov x27, xzr
    mov x28, xzr
    mov x29, xzr
    mov x30, xzr

    br x18

.section .note.GNU-stack,"",@progbits
