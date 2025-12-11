/* start.S */

.section .text
.global _start
.type _start, @function

.global elfz_main_wrapper
.type elfz_main_wrapper, @function

/* Stack structure */
/* [rsp+0] = entry_point (to fill), [rsp+8] = argc_ptr (to fill), [rsp+16] = original_sp, [rsp+24] = saved_rdx */

_start:
    /* Save rdx IMMEDIATELY into r12 (kernel fini pointer) */
    /* Cannot use globals because objcopy -O binary drops relocations */
    movq %rdx, %r12
    
    /* Save original rsp */
    movq %rsp, %rbp
    
    /* GET THE RUNTIME ADDRESS OF THE STUB (for ADRU) */
    /* Use RIP-relative lea to obtain the actual mapped address */
    leaq _start(%rip), %rsi    /* rsi = runtime address of _start */
    
    /* Reserve stack space for entry_point, argc_ptr, stub_runtime_addr, saved_rdx */
    subq $40, %rsp
    
    /* Store original_sp, stub_runtime_addr, and saved_rdx on the stack */
    movq %rbp, 16(%rsp)
    movq %rsi, 24(%rsp)        /* stub_runtime_addr at [rsp+24] */
    movq %r12, 32(%rsp)        /* saved_rdx at [rsp+32] */
    
    /* Pass rsp (pointer to structure) to elfz_main_wrapper */
    movq %rsp, %rdi
    call elfz_main_wrapper
    
    /* Restore rsp to its pre-call value */
    /* Call pushed the return address, so rsp changed */
    addq $8, %rsp            /* Drop call return address */
    
    /* Retrieve entry_point and argc_ptr from the structure */
    movq 0(%rsp), %r11       /* r11 = entry_point */
    movq 8(%rsp), %rsp       /* rsp = argc_ptr */
    
    /* Zero ALL registers */
    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    xorq %rdi, %rdi
    xorq %rbp, %rbp          /* rbp=0 (end of call stack) */
    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10
    xorq %r12, %r12
    xorq %r13, %r13
    xorq %r14, %r14
    xorq %r15, %r15
    
    /* Jump to entry_point (r11 holds entry) */
    jmp *%r11

entry_is_null:
    /* Entry point is NULL - exit with code 99 */
    movq $60, %rax           /* sys_exit */
    movq $99, %rdi           /* exit code 99 */
    syscall

entry_invalid:
    /* Invalid entry point - exit with code 98 */
    movq $60, %rax           /* sys_exit */
    movq $98, %rdi           /* exit code 98 */
    syscall

// Mark non-executable stack to silence linker warning
.section .note.GNU-stack,"",@progbits
