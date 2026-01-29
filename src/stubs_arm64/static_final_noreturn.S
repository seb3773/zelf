.text
.global static_final_transfer
.type static_final_transfer, %function
// static_final_transfer(void *entry, void *orig_sp) noreturn
//   x0 = entry (absolute e_entry)
//   x1 = orig_sp (original kernel stack pointer)
static_final_transfer:
    mov sp, x1
    mov x29, xzr
    mov x30, xzr
    br x0
    brk #0
.size static_final_transfer, .-static_final_transfer

.section .note.GNU-stack,"",@progbits
