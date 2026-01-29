.text
.global static_final_transfer
.type static_final_transfer, @function
# static_final_transfer(void *entry, void *orig_sp) noreturn
#   rdi = entry (absolute e_entry)
#   rsi = orig_sp (original kernel stack pointer)
static_final_transfer:
#ifdef __CET__
    endbr64
#endif
    mov %rsi, %rsp
    xor %rbp, %rbp
    xor %rdx, %rdx
    jmp *%rdi
    ud2
.size static_final_transfer, .-static_final_transfer

.section .note.GNU-stack,"",@progbits
