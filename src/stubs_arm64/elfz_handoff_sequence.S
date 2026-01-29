.text

.equ SYS_munmap, 215

// AArch64 ABI parameters:
//   x0 = entry_point (dynamic loader entry)
//   x1 = original_sp (points to argc)
//   x2 = ADRU (address to unmap)
//   x3 = LENU (size to unmap)
//   x4 = elf_base
//   x5 = interp_base
//   x6 = hatch_ptr
//   x7 = saved_x0 (rtld_fini)

.global elfz_fold_final_sequence
.type elfz_fold_final_sequence, %function

.extern elfz_fold_exact_setup

elfz_fold_final_sequence:
    // Save params in callee-saved registers
    mov x19, x0   // entry_point
    mov x20, x1   // original_sp
    mov x21, x2   // ADRU
    mov x22, x3   // LENU
    mov x23, x4   // elf_base
    mov x24, x5   // interp_base
    mov x25, x6   // hatch_ptr
    mov x26, x7   // saved_x0

    // Call elfz_fold_exact_setup(entry_point, original_sp, ADRU, LENU, elf_base,
    //                            interp_base, hatch_ptr, saved_x0)
    mov x0, x19
    mov x1, x20
    mov x2, x21
    mov x3, x22
    mov x4, x23
    mov x5, x24
    mov x6, x25
    mov x7, x26
    bl elfz_fold_exact_setup

    // x0 = at_null_value_ptr
    mov x27, x0

    // Restore stack to original_sp
    mov sp, x20

    // Prepare stack for hatch:
    // [sp+0]  = argc
    // [sp+8]  = argv
    // [sp+16] = envp
    // [sp+24] = rtld_fini (saved_x0)
    // [sp+32] = entry_point (loaded into x30 by hatch)
    sub sp, sp, #48
    ldr x17, [x20, #0]
    str x17, [sp, #0]
    add x18, x20, #8
    str x18, [sp, #8]
    add x18, x18, x17, lsl #3
    add x18, x18, #8
    str x18, [sp, #16]
    str x26, [sp, #24]
    str x19, [sp, #32]

    // Setup munmap syscall args (x0=addr, x1=len, x8=nr)
    mov x0, x21
    mov x1, x22
    mov x8, #SYS_munmap

    // Jump to hatch directly (x25 = hatch_ptr)
    br x25

    // Should never return
    brk #0

.size elfz_fold_final_sequence, .-elfz_fold_final_sequence

.section .note.GNU-stack,"",@progbits
