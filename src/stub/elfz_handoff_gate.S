/*
 * FINAL SEQUENCE - pure Assembly
 */

.data
stub_name:
    .asciz "stub"

.bss
.align 4096
elfz_stack_buffer:
    .skip 16384              # 16KB buffer for the new stack

.text
.globl elfz_handoff_complete
.type elfz_handoff_complete, @function

# Arguments:
#   %rdi = original_sp (original stack with argc)
#   %rsi = ADRU (stub address)
#   %rdx = LENU (stub size)
#   %rcx = loader_entry (loader entry point)
#   %r8  = at_null_value_ptr (POINTER to AT_NULL value containing hatch)
elfz_handoff_complete:
    #   %rdi = original_sp (points to argc)
    #   %rsi = ADRU
    #   %rdx = LENU
    #   %rcx = loader_entry
    #   %r8  = at_null_value_ptr (pointer to AT_NULL value)
    # ensure %r8 is a valid pointer (disabled)
    # test %r8, %r8
    # jz .Lerror              # Si NULL, erreur
    # Stack is ALREADY prepared with [ADRU][LENU][rdx][entry][argc]
    # received parameters:
    #   %rdi = safe_stack (points to ADRU, stack already prepared)
    #   %rsi = ADRU (unused, already on stack)
    #   %rdx = LENU (unused, already on stack)
    #   %rcx = loader_entry (unused, already on stack)
    #   %r8  = at_null_value_ptr
    # restore stack to safe_stack (points to ADRU)
    mov %rdi, %rsp          # rsp = safe_stack (pointe vers ADRU)
    # Stack ALREADY: [ADRU][LENU][rdx][entry][argc][argv]...
    #             ^rsp
    # Save %r8 (at_null_value_ptr) into %r14 for the final jump
    mov %r8, %r14           # r14 = at_null_value_ptr (adresse de AT_NULL.a_val contenant hatch)
    pop %rdi                # rdi = ADRU, rsp pointe vers LENU
    pop %rsi                # rsi = LENU, rsp pointe vers rdx  
    # for testing: temporarily disable munmap
    # push $11                # Push munmap syscall number
    # pop %rax                # rax = 11, rsp pointe vers rdx
    # jmp *(%r14)             # Saut vers hatch via *(%r14)
    # Simulate what the hatch does WITHOUT munmap
    pop %rdx                # Pop rdx (comme le hatch)
    # Nettoyer registres avant ret
    xor %rax, %rax
    xor %rbx, %rbx
    xor %rcx, %rcx
    xor %rsi, %rsi
    xor %rdi, %rdi
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15
    xor %rbp, %rbp
    ret                     # Ret vers entry (comme le hatch)
    # Hatch behavior: syscall; pop %rdx; ret
    #   syscall: munmap stub
    #   pop %rdx: clean
    #   ret: jump to loader_entry, rsp-> argc
.Lerror:
    # Error: invalid parameter
    mov $60, %rax           # syscall exit
    mov $1, %rdi            # code erreur 1
    syscall
    ud2

.size elfz_handoff_complete, .-elfz_handoff_complete

msg_asm:
    .ascii "  [ASM] JUMP HATCH\n"

.globl elfz_handoff_asm
.type elfz_handoff_asm, @function

# Arguments (System V x86-64 ABI):
#   %rdi = entry_point (address of decompressed binary)
#   %rsi = safe_stack
#   %rdx = ADRU (stub address to unmap)
#   %rcx = LENU (stub size to unmap)
#   %r8  = at_null_entry_addr (ADDRESS of AT_NULL entry in AUXV)
elfz_handoff_asm:
    # 1. Save parameters
    mov %rdi, %r13          # r13 = entry_point
    mov %rsi, %rsp          # Restore the stack
    mov %rdx, %r15          # r15 = ADRU
    mov %rcx, %r12          # r12 = LENU
    mov %r8, %r14           # r14 = ADDRESS of AT_NULL entry (contains hatch)
    
    # 2. Stack now: [ADRU][LENU][rdx][entry][argc][argv]...
    pop %rdi                # %rdi = ADRU, stack = [LENU][rdx][entry][argc]...
    pop %rsi                # %rsi = LENU, stack = [rdx][entry][argc]...
    
    # ENABLE MUNMAP
    mov $11, %rax           # %rax = 11 (munmap syscall number)
    jmp *(%r14)             # Jump to hatch: syscall; pop %rdx; ret
    # Hatch will do:
    # syscall            - munmap(%rdi, %rsi)
    # pop %rdx           - clears rdx, stack = [entry][argc]...
    # ret                - jumps to entry_point, stack = [argc]...
    # Hatch will do:
    # syscall            - munmap(%rdi, %rsi)
    # pop %rdx           - clears rdx, stack = [entry][argc]...
    # ret                - jumps to entry_point, stack = [argc]...
.Lr14_corrupted:
    # Error: r14 was corrupted
    mov $60, %rax           # syscall exit
    mov $3, %rdi            # code erreur 3
    syscall
    ud2
    # Le hatch fera :
    # pop %rdx           - nettoie rdx, stack = [entry][argc]...
    # ret                - saute vers entry_point, stack = [argc]...
    ud2  # Ne devrait jamais arriver ici
.size elfz_handoff_asm, .-elfz_handoff_asm

// Mark non-executable stack to silence linker warning
.section .note.GNU-stack,"",@progbits
