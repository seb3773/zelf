.text

.global simple_memcpy
.type simple_memcpy, %function
simple_memcpy:
    mov x9, x0
    cbz x2, 2f
1:
    ldrb w10, [x1], #1
    strb w10, [x0], #1
    subs x2, x2, #1
    b.ne 1b
2:
    mov x0, x9
    ret

.global memset
.type memset, %function
memset:
    b simple_memset

.global memmove
.type memmove, %function
memmove:
    b simple_memcpy

.global memcpy
.type memcpy, %function
memcpy:
    b simple_memcpy

.global simple_memset
.type simple_memset, %function
simple_memset:
    mov x9, x0
    cbz x2, 4f
    and w1, w1, #0xff
3:
    strb w1, [x0], #1
    subs x2, x2, #1
    b.ne 3b
4:
    mov x0, x9
    ret

.size simple_memcpy, .-simple_memcpy
.size simple_memset, .-simple_memset

.section .note.GNU-stack,"",@progbits
