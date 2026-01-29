; ============================================================================
; Shrinkler Decompressor - x86_64 port by Seb3773
; ============================================================================
; Copyright 1999-2015 Aske Simon Christensen.
; x86_64 port 2025
;
; The code herein is free to use, in whole or in part,
; modified or as is, for any legal purpose.
;
; No warranties of any kind are given as to its behavior
; or suitability.
;
; Based on the ARM port by Kieran Connell
; ============================================================================

BITS 64

; ============================================================================
; Constants
; ============================================================================

%define INIT_ONE_PROB       0x8000
%define ADJUST_SHIFT        4
%define NUM_CONTEXTS        1536
%define NUM_SINGLE_CONTEXTS 1

%define CONTEXT_KIND         0
%define CONTEXT_REPEATED    -1
%define CONTEXT_GROUP_LIT    0
%define CONTEXT_GROUP_OFFSET 2
%define CONTEXT_GROUP_LENGTH 3

; Compilation options (can be overridden with -D on command line)
%ifndef _PARITY_MASK
%define _PARITY_MASK        0       ; 0=byte data, 1=16-bit word data
%endif

%ifndef _ENDIAN_SWAP
%define _ENDIAN_SWAP        0       ; 0=little-endian input, 1=swap at runtime
%endif

%ifndef _PROGRESS_CB
%define _PROGRESS_CB        1       ; 1=include progress callback
%endif

; Optional debug trace
%ifndef DBG_TRACE
%define DBG_TRACE           0
%endif
%define TRACE_MAX           512

; ============================================================================
; x86_64 Register Usage
; ============================================================================
; RDI  = source pointer (compressed data)      [preserved from caller]
; RSI  = dest pointer (decompressed data)      [preserved from caller]
; RDX  = callback function                     [preserved from caller]
; RCX  = callback argument                     [preserved from caller]
; R8   = context buffer pointer                [preserved from caller]
;
; During decompression:
; RAX  = temp / return value
; RBX  = offset (LZDecode)
; RCX  = intervalsize (range decoder)
; RDX  = intervalvalue (range decoder)
; RSI  = dest pointer (current position)
; RDI  = source pointer (current position)
; R8   = context buffer pointer
; R9   = context_index
; R10  = bit_buffer
; R11  = temp
; R12  = callback function
; R13  = callback argument
; R14  = decompress_base (for progress reporting)
; R15  = temp
; ============================================================================

section .text

; ============================================================================
; Function: ShrinklerDecompress
; ============================================================================
; Input:
;   RDI = compressed data address (source)
;   RSI = decompressed data address (dest)
;   RDX = callback function address (or 0 if not required)
;   RCX = callback argument
;   R8  = context buffer (NUM_CONTEXTS * 4 = 6144 bytes)
;
; Output:
;   RAX = number of bytes written
;
; Preserves callee-saved registers (RBX, R12, R13, R14, R15). Others are not preserved.
; ============================================================================

global ShrinklerDecompress
ShrinklerDecompress:
    ; Preserve System V callee-saved registers we use
    push rbx
    push r12
    push r13
    push r14
    push r15
    ; Save initial destination for progress reporting
%if _PROGRESS_CB
    mov r14, rsi                    ; decompress_base
    mov r12, rdx                    ; callback function
    mov r13, rcx                    ; callback argument
%endif

    ; Initialize range decoder state
    xor edx, edx                    ; intervalvalue = 0
    mov ecx, 1                      ; intervalsize = 1
    mov r10d, 0x80                  ; bit_buffer (8-bit) = 0x80
    xor ebx, ebx                    ; offset = 0
    push rdi
    push rsi
    mov rdi, r8                     ; context buffer
    mov eax, INIT_ONE_PROB
    mov ecx, NUM_CONTEXTS
.init_contexts:
    mov dword [rdi], eax
    add rdi, 4
    loop .init_contexts
    pop rsi
    pop rdi

    ; Adjust context pointer past single contexts
    add r8, NUM_SINGLE_CONTEXTS * 4
    mov ecx, 1                      ; restore intervalsize
    xor r9d, r9d                    ; initialize context/parity to 0

; ============================================================================
; Main decompression loop - decode literals
; ============================================================================
.LZDecode_literal:
    ; Decode literal byte
    ; _PARITY_MASK==0: no parity bits used in literal context
    mov r9d, 1                      ; context = 1

.literal_loop:
    call RangeDecodeBit             ; decode bit
    shl r9d, 1
    or r9d, eax                     ; context = (context << 1) | bit
    cmp r9d, 0x100                  ; byte complete?
    jl .literal_loop

    ; Write literal byte
    mov byte [rsi], r9b
    inc rsi

    ; DEBUG LITERAL
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    mov rdi, 1
    movzx rsi, r9w
    xor rdx, rdx
    
    extern debug_log
    call debug_log
    
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ; END DEBUG

    ; Report progress callback
%if _PROGRESS_CB
    test r12, r12
    jz .no_callback_lit
    call ReportProgress
.no_callback_lit:
%endif

    ; After literal - GetKind
%if _PARITY_MASK != 0
    mov r9d, esi
    and r9d, _PARITY_MASK           ; parity = pos & _PARITY_MASK
    shl r9d, 8
    add r9d, CONTEXT_KIND
%else
    mov r9d, CONTEXT_KIND
%endif
    call RangeDecodeBit             ; ref = decode(CONTEXT_KIND)
    test eax, eax
    jz .LZDecode_literal            ; if ref == 0, decode another literal

; ============================================================================
; Decode reference (LZ match)
; ============================================================================
.LZDecode_reference:
    mov r9d, CONTEXT_REPEATED
    call RangeDecodeBit             ; repeated = decode(CONTEXT_REPEATED)
    test eax, eax
    jz .LZDecode_readoffset         ; if not repeated, read new offset

.LZDecode_readlength:
    mov r9d, CONTEXT_GROUP_LENGTH << 8
    call RangeDecodeNumber          ; length = decodeNumber(...)
    ; RAX now contains length, RBX contains offset
    
    ; DEBUG REF
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    mov rdi, 2
    mov rsi, rbx
    mov rdx, rax
    
    call debug_log
    
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    ; END DEBUG

    ; Copy loop: copy length bytes from (dest - offset)
    push rdi                        ; save source pointer temporarily
    mov rdi, rsi
    sub rdi, rbx                    ; source = dest - offset
.copy_loop:
    mov r11b, byte [rdi]            ; load byte from source (avoid clobbering RCX)
    mov byte [rsi], r11b            ; store to dest
    inc rdi
    inc rsi
    dec rax
    jnz .copy_loop
    pop rdi                         ; restore source pointer

    ; Report progress callback
%if _PROGRESS_CB
    test r12, r12
    jz .no_callback_ref
    call ReportProgress
.no_callback_ref:
%endif

    ; After reference - GetKind
%if _PARITY_MASK != 0
    mov r9d, esi
    and r9d, _PARITY_MASK
    shl r9d, 8
    add r9d, CONTEXT_KIND
%else
    mov r9d, CONTEXT_KIND
%endif
    call RangeDecodeBit
    test eax, eax
    jz .LZDecode_literal

.LZDecode_readoffset:
    mov r9d, CONTEXT_GROUP_OFFSET << 8
    call RangeDecodeNumber          ; offset = decodeNumber(...) - 2
    sub rax, 2
    mov rbx, rax                    ; store offset
    test rbx, rbx
    jnz .LZDecode_readlength        ; if offset != 0, continue

    ; Decompression complete - return number of bytes written
%if _PROGRESS_CB
    mov rax, rsi
    sub rax, r14                    ; bytes written = dest - decompress_base
%else
    xor eax, eax
%endif
    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; ============================================================================
; ReportProgress - Call progress callback
; ============================================================================
%if _PROGRESS_CB
ReportProgress:
    push rdi
    push rsi
    push rdx
    push rcx
    push r8
    push r9
    push r10                       ; preserve bit_buffer across callback

    mov rdi, rsi
    sub rdi, r14                    ; bytes written
    mov rsi, r13                    ; callback argument
    call r12                        ; callback(bytes_written, arg)

    pop r10
    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    ret
%endif

; ============================================================================
; Function: RangeDecodeBit
; ============================================================================
; Input:
;   R9D = context_index
; Output:
;   EAX = decoded bit (0 or 1)
; Uses: RAX, R11, R15
; Preserves: RBX, RSI, RDI, R8
; Modifies: RCX (intervalsize), RDX (intervalvalue), R10 (bit_buffer)
; ===; =========================================================================
%if DBG_TRACE
section .data
dbg_trace_buf times TRACE_MAX*20 db 0
dbg_trace_cnt dd 0
%endif

section .text
RangeDecodeBit:
    ; Ensure intervalsize >= 0x8000
.normalize:
    cmp ecx, 0x8000
    jge .normalized
    shl ecx, 1                      ; intervalsize <<= 1
    and ecx, 0xFFFF

    ; Get next bit from input stream using CPU CF
    shl r10b, 1                     ; shift left, CF = old MSB
    jnz .no_new_byte

    ; Need to read new byte and include previous carry
    mov al, byte [rdi]
    inc rdi
    adc al, al                      ; shift left with carry-in to LSB, CF = new MSB
    mov r10b, al

.no_new_byte:
    adc edx, edx                    ; intervalvalue = (intervalvalue << 1) | CF
    and edx, 0xFFFF
    jmp .normalize

.normalized:
    ; Get probability from context
    movsx r11, r9w                  ; sign-extend 16-bit context_index
    mov eax, dword [r8 + r11*4]     ; prob = contexts[context_index]

    ; Calculate threshold = (intervalsize * prob) >> 16
    mov r15d, ecx
    imul r15d, eax
    shr r15d, 16                    ; threshold

    ; Calculate new_prob = prob - (prob >> ADJUST_SHIFT)
    mov r11d, eax
    shr r11d, ADJUST_SHIFT
    sub eax, r11d                   ; new_prob = prob - (prob >> ADJUST_SHIFT)

    ; Compare intervalvalue with threshold (unsigned)
    cmp edx, r15d
    jae .decode_zero

.decode_one:
    ; Bit is 1
    ; new_prob += (0xFFFF >> ADJUST_SHIFT)
    add eax, (0xFFFF >> ADJUST_SHIFT)

    ; Store updated probability
    movsx r11, r9w
    mov dword [r8 + r11*4], eax

    ; Update range decoder state
    mov ecx, r15d                   ; intervalsize = threshold
    mov r11d, ecx                   ; keep size for trace
    mov eax, 1
%if DBG_TRACE
    ; Debug trace entry for 'one'
    push rdx
    push rcx
    push rax
    push rdi
    mov eax, dword [rel dbg_trace_cnt]
    cmp eax, TRACE_MAX
    jae .skip_trace_one
    mov ecx, eax                    ; ecx = count
    imul ecx, ecx, 20               ; ecx = offset
    lea rdi, [rel dbg_trace_buf]
    add rdi, rcx                    ; rdi = base + offset
    mov dword [rdi + 0], r9d        ; ctx
    mov dword [rdi + 4], r11d       ; size (saved)
    mov eax, dword [rsp + 24]       ; original edx saved on stack
    mov dword [rdi + 8], eax        ; value
    movzx eax, r10b
    mov dword [rdi + 12], eax       ; bitbuf
    mov dword [rdi + 16], 1         ; bit
    inc dword [rel dbg_trace_cnt]
.skip_trace_one:
    pop rdi
    pop rax
    pop rcx
    pop rdx
%endif
    ret

.decode_zero:
    ; Bit is 0
    ; Store updated probability (already calculated)
    movsx r11, r9w
    mov dword [r8 + r11*4], eax

    ; Update range decoder state
    sub edx, r15d                   ; intervalvalue -= threshold
    sub ecx, r15d                   ; intervalsize -= threshold
    and edx, 0xFFFF
    and ecx, 0xFFFF
    mov r11d, ecx                   ; keep size for trace
    xor eax, eax
%if DBG_TRACE
    ; Debug trace entry for 'zero'
    push rdx
    push rcx
    push rax
    push rdi
    mov eax, dword [dbg_trace_cnt]
    cmp eax, TRACE_MAX
    jae .skip_trace_zero
    mov ecx, eax                    ; ecx = count
    imul ecx, ecx, 20               ; ecx = offset
    lea rdi, [rel dbg_trace_buf]
    add rdi, rcx                    ; rdi = base + offset
    mov dword [rdi + 0], r9d        ; ctx
    mov dword [rdi + 4], r11d       ; size (saved)
    mov eax, dword [rsp + 24]       ; original edx saved on stack
    mov dword [rdi + 8], eax        ; value
    movzx eax, r10b
    mov dword [rdi + 12], eax       ; bitbuf
    mov dword [rdi + 16], 0         ; bit
    inc dword [dbg_trace_cnt]
.skip_trace_zero:
    pop rdi
    pop rax
    pop rcx
    pop rdx
%endif
    ret

; ============================================================================
; Function: RangeDecodeNumber
; ============================================================================
; Input:
;   R9D = base_context (already shifted << 8)
; Output:
;   RAX = decoded number (>= 2)
; Uses: RAX, RBX, R14
; =========================================================================
RangeDecodeNumber:
    push r9                         ; save base_context
    push rbx                        ; preserve caller's RBX
    push r14                        ; preserve R14 (decompress_base)

    ; Decode unary prefix to get bit count
    xor r14d, r14d                  ; i = 0
.unary_loop:
    add r9d, 2                      ; context = base_context + (i * 2)
    call RangeDecodeBit
    test eax, eax
    jz .unary_done
    inc r14d                        ; i++
    jmp .unary_loop

.unary_done:
    ; Decode i-bit binary number
    sub r9d, 1                      ; context--
    mov ebx, 1                      ; number = 1 (in EBX)
.binary_loop:
    call RangeDecodeBit             ; bit in EAX
    shl ebx, 1                      ; number <<= 1
    or ebx, eax                     ; number |= bit
    sub r9d, 2                      ; context -= 2
    dec r14d                        ; i--
    jns .binary_loop                ; while (i >= 0)

.done:
    mov eax, ebx                    ; return number in RAX
    pop r14
    pop rbx
    pop r9
    ret

; ============================================================================
; Function: ShrinklerParseHeader (optional)
; ============================================================================
; TODO: Implement header parsing if needed
; ============================================================================

; Mark stack as non-executable
section .note.GNU-stack noalloc noexec nowrite progbits

