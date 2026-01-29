.section .text
.global _start
.type _start, @function

.extern elfz_sfx_entry

_start:
    andq $-16, %rsp
    call elfz_sfx_entry
    movq %rax, %rdi
    movq $60, %rax
    syscall

.section .note.GNU-stack,"",@progbits
