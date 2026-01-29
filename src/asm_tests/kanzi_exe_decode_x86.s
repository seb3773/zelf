.text
.globl kanzi_exe_unfilter_x86
.type kanzi_exe_unfilter_x86, @function
kanzi_exe_unfilter_x86:
    push    %rbp
    mov     %rsp, %rbp

    # Check input_size < 9 or NULL pointers
    cmp     $9, %rsi
    jb      .L_fail

    test    %rdi, %rdi
    jz      .L_fail

    test    %rdx, %rdx
    jz      .L_fail

    # Verify input[0] == 0x40
    cmpb    $0x40, (%rdi)
    jne     .L_fail

    # Read code_start and code_end
    mov     1(%rdi), %r8d      # code_start (little-endian)
    mov     5(%rdi), %r9d      # code_end (little-endian)

    # Validate ranges
    cmp     %rsi, %r9d         # code_end > input_size ?
    ja      .L_fail

    mov     %r8d, %eax         # eax = code_start
    add     $9, %eax           # +9
    cmp     %rsi, %eax         # code_start + 9 > input_size ?
    ja      .L_fail

    # Initialize src_idx = 9, dst_idx = 0
    mov     $9, %r10d          # src_idx
    xor     %r11d, %r11d       # dst_idx

    # Copy data before the code region
    test    %r8d, %r8d
    jz      .L_main_loop

    mov     %rdx, %rax         # dst
    add     %r11, %rax         # dst + dst_idx
    lea     (%rdi,%r10), %rcx  # src + src_idx
    mov     %r8d, %r12d        # copier n = code_start
    call    .L_memcpy
    add     %r8d, %r11d        # dst_idx += code_start
    add     %r8d, %r10d        # src_idx += code_start

.L_main_loop:
    cmp     %r9d, %r10d        # src_idx < code_end ?
    jae     .L_copy_rest

    mov     %r10d, %eax
    mov     (%rdi,%rax), %al   # b = input[src_idx]

    # Check b == 0x0F
    cmp     $0x0F, %al
    jne     .L_check_call_jmp

    # Yes, it is 0x0F
    mov     %rdx, %rax
    add     %r11, %rax
    mov     %al, (%rax)        # output[dst_idx++] = input[src_idx++]
    inc     %r11d
    inc     %r10d

    # Check if input[src_idx] & 0xF0 == 0x80 (JCC)
    mov     (%rdi,%r10), %al
    and     $0xF0, %al
    cmp     $0x80, %al
    jne     .L_skip_escape

    # It is a JCC; continue with CALL/JMP handling
    jmp     .L_process_call_jmp

.L_check_call_jmp:
    # Check (b & 0xFE) == 0xE8
    mov     (%rdi,%r10), %al
    and     $0xFE, %al
    cmp     $0xE8, %al
    jne     .L_skip_escape

.L_process_call_jmp:
    # Read addr big-endian, XOR 0xF0F0F0F0
    mov     1(%rdi,%r10), %eax
    bswap   %eax               # big-endian to host
    xor     $0xF0F0F0F0, %eax  # XOR mask

    # Compute offset = addr - dst_idx
    sub     %r11d, %eax        # eax = offset

    # Apply mask if offset < 0
    test    %eax, %eax
    jns     .L_write_offset
    neg     %eax
    and     $0xFFFFFF, %eax
    neg     %eax

.L_write_offset:
    # Write output[dst_idx] = input[src_idx]
    mov     (%rdi,%r10), %cl
    mov     %rdx, %rbx
    add     %r11, %rbx
    mov     %cl, (%rbx)
    inc     %r11d
    inc     %r10d

    # Write offset in little-endian
    bswap   %eax               # host to little-endian
    mov     %eax, 0(%rdx,%r11)
    add     $4, %r11d
    add     $4, %r10d
    jmp     .L_main_loop

.L_skip_escape:
    # Check if b == 0x9B (escape byte)
    mov     (%rdi,%r10), %al
    cmp     $0x9B, %al
    jne     .L_write_normal

    inc     %r10d              # skip the 0x9B

.L_write_normal:
    # Write input[src_idx] into output[dst_idx]
    mov     (%rdi,%r10), %al
    mov     %rdx, %rbx
    add     %r11, %rbx
    mov     %al, (%rbx)
    inc     %r11d
    inc     %r10d
    jmp     .L_main_loop

.L_copy_rest:
    mov     %r10d, %eax
    sub     %rsi, %rax         # eax = input_size - src_idx
    neg     %rax               # eax = rest
    jz      .L_done

    mov     %rdx, %rax
    add     %r11, %rax         # dst + dst_idx
    lea     (%rdi,%r10), %rcx  # src + src_idx
    mov     %rsi, %r12
    sub     %r10, %r12         # n = input_size - src_idx
    call    .L_memcpy
    mov     %r12, %r12d
    add     %r12d, %r11d       # dst_idx += n

.L_done:
    mov     %rsi, (%r8)        # *processed_size = input_size
    mov     %r11d, %eax        # return dst_idx
    pop     %rbp
    ret

.L_fail:
    mov     $0, (%r8)          # *processed_size = 0
    xor     %eax, %eax         # return 0
    pop     %rbp
    ret

# Local memcpy helper
.L_memcpy:
    test    %r12, %r12
    jz      .L_memcpy_done
    mov     (%rcx), %dl
    mov     %dl, (%rax)
    inc     %rcx
    inc     %rax
    dec     %r12
    jmp     .L_memcpy
.L_memcpy_done:
    ret

