.section .text
.global _start
.type _start, %function

.extern elfz_sfx_entry

_start:
    mov x16, sp
    and x16, x16, #0xfffffffffffffff0
    mov sp, x16
    bl elfz_sfx_entry

    mov x8, #93
    svc #0

.section .note.GNU-stack,"",@progbits
