.text
.globl bcj_x86_decode
.type bcj_x86_decode, @function
bcj_x86_decode:
    # rdi = buf
    # rsi = size
    # edx = offset
    
    push    %rbp
    mov     %rsp, %rbp
    push    %rbx
    push    %r12

    # size < 5 check
    cmp     $5, %rsi
    jb      .L_fail

    # Save offset in r12d (preserved)
    mov     %edx, %r12d
    
    # r9d = pos = offset - 5
    mov     %r12d, %r9d
    sub     $5, %r9d
    
    xor     %rcx, %rcx                 # i = 0
    xor     %r8d, %r8d                 # mask = 0

.L_loop:
    # Check i <= size - 5
    mov     %rsi, %rax
    sub     $5, %rax
    cmp     %rax, %rcx
    ja      .L_end

    # Check buf[i] == E8 or E9
    mov     (%rdi,%rcx), %al
    cmp     $0xE8, %al
    je      .L_do
    cmp     $0xE9, %al
    jne     .L_next

.L_do:
    # off (eax) = (offset + i) - pos
    mov     %r12d, %eax
    add     %ecx, %eax
    sub     %r9d, %eax
    
    # pos = offset + i
    mov     %r12d, %r9d
    add     %ecx, %r9d
    
    # if (off > 5) mask = 0
    cmp     $5, %eax
    jbe     .L_calc_mask
    xor     %r8d, %r8d
    jmp     .L_check_b
    
.L_calc_mask:
    # while (off--) { mask &= 0x77; mask <<= 1; }
    mov     %eax, %ebx
.L_mk_loop:
    test    %ebx, %ebx
    jz      .L_check_b
    and     $0x77, %r8d
    shl     $1, %r8d
    dec     %ebx
    jmp     .L_mk_loop

.L_check_b:
    # b = buf[i+4]
    mov     4(%rdi,%rcx), %bl
    
    # Check (b == 0 || b == 0xFF)
    # Using copy to not destroy bl
    mov     %bl, %al
    inc     %al
    test    $0xFE, %al
    jnz     .L_else       # Neither 0 nor FF
    
    # Check (mask >> 1) <= 4 && (mask >> 1) != 3
    mov     %r8d, %eax
    shr     $1, %eax
    cmp     $4, %eax
    ja      .L_else
    cmp     $3, %eax
    je      .L_else
    
    # TRUE BRANCH
    # src = load32(buf+i+1) (which includes b as MSB)
    mov     1(%rdi,%rcx), %edx  # edx = src
    
    # term (ebx) = offset + i + 5
    mov     %r12d, %ebx
    add     %ecx, %ebx
    add     $5, %ebx
    
.L_inner_loop:
    # dest (eax) = src - term
    mov     %edx, %eax
    sub     %ebx, %eax
    
    test    %r8d, %r8d
    jz      .L_write
    
    # idx (r10d) = t[mask >> 1]
    mov     %r8d, %r10d
    shr     $1, %r10d
    lea     .L_table(%rip), %r11
    movzx   (%r11,%r10), %r10d
    
    # shift (r11d) = 24 - idx * 8
    mov     $24, %r11d
    shl     $3, %r10d           # idx*8
    sub     %r10d, %r11d
    
    # b_temp = (uint8_t)(dest >> shift)
    push    %rcx
    mov     %r11d, %ecx
    mov     %eax, %r11d
    shr     %cl, %r11d
    pop     %rcx
    
    # if (b_temp != 0 && b_temp != 0xFF) break;
    mov     %r11b, %al          # b_temp
    inc     %al
    test    $0xFE, %al
    jz      .L_xor              # 0 or FF -> continue loop (update src)
    
    # break -> write (dest is in eax from before clobbering? No, eax was clobbered)
    # We need dest in eax.
    # Recompute dest? Or restore it.
    # 'mov %edx, %eax; sub %ebx, %eax' was at top of loop.
    # 'mov %eax, %r11d' preserved it? No, shifted it.
    # I need dest. R10d has idx*8.
    # Just recompute dest.
    mov     %edx, %eax
    sub     %ebx, %eax
    jmp     .L_write

.L_xor:
    # src = dest ^ ((1 << (32 - idx*8)) - 1)
    # dest is recomputable (or we saved it? No).
    mov     %edx, %eax
    sub     %ebx, %eax          # dest
    
    # shift (ecx) = 32 - idx*8
    # idx*8 is in r10d
    mov     $32, %r11d
    sub     %r10d, %r11d
    
    push    %rcx
    mov     %r11d, %ecx
    mov     $1, %r11d
    shl     %cl, %r11d
    dec     %r11d
    pop     %rcx
    
    xor     %r11d, %eax         # dest ^ mask -> new src
    mov     %eax, %edx          # update src
    jmp     .L_inner_loop

.L_write:
    # buf[i+4] = ~(((dest >> 24) & 1) - 1)
    # dest is in eax
    mov     %eax, %r10d
    shr     $24, %r10d
    and     $1, %r10d
    dec     %r10d
    not     %r10d
    mov     %r10b, 4(%rdi,%rcx)
    
    # buf[i+1]...buf[i+4] = dest (MSB overwritten above)
    mov     %eax, 1(%rdi,%rcx)
    mov     %r10b, 4(%rdi,%rcx) # Rewrite MSB just in case
    
    add     $4, %rcx
    xor     %r8d, %r8d          # mask = 0
    jmp     .L_check_loop

.L_else:
    # mask |= 1
    or      $1, %r8d
    # if (b == 0 || b == 0xFF) mask |= 0x10
    # Check b again (bl)
    mov     %bl, %al
    inc     %al
    test    $0xFE, %al
    jnz     .L_next
    or      $0x10, %r8d

.L_next:
    inc     %rcx
.L_check_loop:
    jmp     .L_loop

.L_end:
    mov     %rcx, %rax
    pop     %r12
    pop     %rbx
    pop     %rbp
    ret

.L_fail:
    xor     %rax, %rax
    pop     %r12
    pop     %rbx
    pop     %rbp
    ret

.L_table:
    .byte   0, 1, 2, 2, 3
