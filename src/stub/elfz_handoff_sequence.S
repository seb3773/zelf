/*
 * Parameters (x86-64 SysV ABI):
 *   %rdi = entry_point (dynamic loader address)
 *   %rsi = original_sp (pointer to argc)
 *   %rdx = ADRU (address to unmap)
 *   %rcx = LENU (size to unmap)
 *   %r8  = elf_base
 *   %r9  = interp_base
 *   (hatch and saved_rdx in globals: g_hatch_ptr, g_saved_rdx)
 */

.text
.globl elfz_fold_final_sequence
.type elfz_fold_final_sequence, @function

elfz_fold_final_sequence:
    // no debug marker
    // Save callee-saved parameters across the C call
    mov %rdi, %r15   // entry_point
    mov %rsi, %rbx   // original_sp
    mov %rdx, %r12   // ADRU
    mov %rcx, %r13   // LENU
    
    // Forward 8 params: first 6 already in rdi..r9.
    // Retrieve 7th/8th from our incoming stack, then place them for the call.
    // Respect SysV ABI: pre-call %rsp 16-byte aligned; 7th/8th in [rsp+0],[rsp+8] pre-call.
    mov 8(%rsp), %r10      // param 7: hatch_ptr
    mov 16(%rsp), %r11     // param 8: saved_rdx
    mov %r11, %rbp         // keep saved_rdx (callee-saved)
    sub $24, %rsp          // 8 to align + 16 for two stack args
    mov %r10, 0(%rsp)      // [rsp+0] = hatch_ptr
    mov %r11, 8(%rsp)      // [rsp+8] = saved_rdx
    lea elfz_fold_exact_setup(%rip), %rax
    call *%rax
    add $24, %rsp          // restore stack
    
    // STEP 2: fetch result
    // %rax holds at_null_entry_addr
    mov %rax, %r14
    
    // STEP 3: restore original_sp (points to argc)
    mov %rbx, %rsp
    
    // STEP 4: reserve 32 bytes and write directly
    // Target layout:
    // [rsp+0]  = ADRU
    // [rsp+8]  = LENU
    // [rsp+16] = saved_rdx
    // [rsp+24] = entry_point
    sub $32, %rsp
    mov %r12, 0(%rsp)   // ADRU
    mov %r13, 8(%rsp)   // LENU
    mov %rbp, 16(%rsp)  // saved_rdx
    mov %r15, 24(%rsp)  // entry_point
    // STEP 5: final sequence
    pop %rdi              // ADRU
    pop %rsi              // LENU
    push $11
    pop %rax
    jmp *(%r14)           // Jump to hatch
    
    // Should never reach here
    ud2

.size elfz_fold_final_sequence, .-elfz_fold_final_sequence

// Mark non-executable stack to silence linker warning
.section .note.GNU-stack,"",@progbits
