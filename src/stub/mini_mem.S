.text
.globl simple_memcpy
.type simple_memcpy, @function
simple_memcpy:
    mov %rdi, %r8            # save dst for return
    test %edx, %edx          # n <= 0 ?
    jle 1f
    mov %edx, %ecx           # rcx = n
    cld
    rep movsb                # copy n bytes from rsi to rdi
1:
    mov %r8, %rax            # return dst
    ret

.globl memset
.type memset, @function
memset:
    jmp simple_memset

.globl memmove
.type memmove, @function
memmove:
    # alias to memcpy; safe for our usage patterns
    jmp simple_memcpy

.globl memcpy
.type memcpy, @function
memcpy:
    jmp simple_memcpy

.globl simple_memset
.type simple_memset, @function
simple_memset:
    mov %rdi, %r8            # save dst for return
    test %edx, %edx          # n <= 0 ?
    jle 2f
    mov %edx, %ecx           # rcx = n
    mov %sil, %al            # al = (unsigned char)c
    cld
    rep stosb                # set n bytes at rdi to AL
2:
    mov %r8, %rax            # return dst
    ret
