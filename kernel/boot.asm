; 64-bit entry shim
[BITS 64]
[GLOBAL _start]
[EXTERN kmain]
[EXTERN __bss_start]
[EXTERN __bss_end]

_start:
    ; Send 'K' to serial to confirm we entered kernel
    mov dx, 0x3F8
    mov al, 'K'
    out dx, al
    
    ; Set up stack with proper alignment
    mov rsp, 0x00200000      ; 2MB mark - safe above kernel
    and rsp, ~0xF            ; Ensure 16-byte alignment
    
    ; Enable SSE (required for x86-64 floating point and some optimizations)
    mov rax, cr0
    and ax, 0xFFFB           ; Clear CR0.EM (bit 2)
    or ax, 0x2               ; Set CR0.MP (bit 1)
    mov cr0, rax
    
    mov rax, cr4
    or ax, 3 << 9            ; Set CR4.OSFXSR (bit 9) and CR4.OSXMMEXCPT (bit 10)
    mov cr4, rax
    
    ; Send 'M' to serial before BSS clear
    mov dx, 0x3F8
    mov al, 'M'
    out dx, al
    
    ; Zero .bss
    lea rdi, [rel __bss_start]
    lea rcx, [rel __bss_end]
    sub rcx, rdi              ; rcx = size in bytes
    
    ; Check if BSS is reasonable
    cmp rcx, 0
    jl .skip_bss              ; Skip if negative (linker error)
    cmp rcx, 0x100000         ; Skip if > 1MB (something is wrong)
    jg .skip_bss
    
    shr rcx, 3
    xor rax, rax
    rep stosq
    
.skip_bss:
    ; Send 'N' to serial before calling kmain
    mov dx, 0x3F8
    mov al, 'N'
    out dx, al
    
    ; Re-align stack before call (call pushes 8 bytes, so we need rsp % 16 == 8)
    and rsp, ~0xF
    sub rsp, 8
    
    call kmain
    
    ; Send 'X' to serial if kmain returns
    mov dx, 0x3F8
    mov al, 'X'
    out dx, al
    
.halt:
    hlt
    jmp .halt