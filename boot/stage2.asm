; stage2 loader: real mode -> protected -> long mode -> jump to kernel
; Kernel location metadata is patched by Makefile at the META marker.
[BITS 16]
[ORG 0]
%define BASE_PHYS 0x80000

%define KERNEL_LOAD_SEG 0x2000
%define KERNEL_LOAD_OFF 0x0000

%define PML4_ADDR 0x90000
%define PDPT_ADDR 0x91000
%define PD_ADDR   0x92000

%define CODE32_SEL 0x08
%define DATA32_SEL 0x10
%define CODE64_SEL 0x18
%define DATA64_SEL 0x20

start:
    cli
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00
    push cs
    pop ax
    mov ds, ax
    mov es, ax
    mov [BOOT_DRIVE], dl

    ; debug 'A' (VGA)
    mov ah, 0x0E
    mov al, 'A'
    int 0x10
    ; also send to serial
    mov al, 'A'
    call print_serial_char

    ; Build GDT descriptor with fixed physical base
    mov ebx, BASE_PHYS

    ; GDTR uses physical base of our GDT in stage2 image (descriptors flat base=0)
    mov dword [gdt_descriptor + 2], gdt_start + BASE_PHYS
    mov word [gdt_descriptor], gdt_end - gdt_start - 1

    ; Far pointers patched with absolute linear addresses
    mov dword [pm_far_ptr], pm_entry + BASE_PHYS

    call enable_a20
    call load_kernel

    ; debug 'B' (VGA)
    mov ah, 0x0E
    mov al, 'B'
    int 0x10
    ; also send to serial
    mov al, 'B'
    call print_serial_char

    ; Setup GDT and enter protected mode
    lgdt [gdt_descriptor]
    ; enable PE and far jump immediately
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    db 0x66
    jmp far [pm_far_ptr]

; ---------------- 32-bit protected mode ----------------
[BITS 32]
pm_entry:
    mov ax, DATA32_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x9F000

    mov dx, 0x3F8
    mov al, 'C'
    out dx, al

    call setup_paging32

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov dx, 0x3F8
    mov al, 'E'
    out dx, al

    ; Load PML4
    mov eax, PML4_ADDR
    mov cr3, eax

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov dx, 0x3F8
    mov al, 'F'
    out dx, al

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    mov dx, 0x3F8
    mov al, 'G'
    out dx, al
    
    ; Far-jump to long mode using absolute address
    mov eax, lm_entry + BASE_PHYS
    push dword CODE64_SEL
    push dword eax
    o32 retf

; ---------------- 64-bit long mode ----------------
[BITS 64]
lm_entry:
    mov dx, 0x3F8
    mov al, 'L'
    out dx, al
    
    ; Load 64-bit data segment
    mov ax, DATA64_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ; set a safe 64-bit stack pointer - higher up to avoid conflicts
    mov rsp, 0x200000     ; 2MB mark, just past our identity mapping

    ; Copy kernel from load buffer to 0x00100000
    mov dx, 0x3F8
    mov al, 'S'    ; start copy
    out dx, al

    mov rsi, (KERNEL_LOAD_SEG << 4) + KERNEL_LOAD_OFF
    mov rdi, 0x00100000
    
    mov al, 's'
    out dx, al
    
    ; read kernel_sectors using RIP-relative addressing
    mov ecx, dword [rel kernel_sectors]
    mov eax, ecx
    shl eax, 9                ; *512 -> bytes
    xor rdx, rdx
    mov ecx, eax
    shr ecx, 3                ; qword count
    mov al, 'q'
    out dx, al
    rep movsq
    mov al, 'Q'
    out dx, al
    mov ecx, eax
    and ecx, 7
    mov al, 'b'
    out dx, al
    rep movsb
    mov al, 'R'
    out dx, al

    ; indicate kernel copy complete
    mov dx, 0x3F8
    mov al, 'D'
    out dx, al

    ; Ensure stack is 16-byte aligned before jumping to kernel
    and rsp, ~0xF
    
    mov rax, 0x00100000
    jmp rax

; ---------------- Support routines ----------------
[BITS 16]
load_kernel:
    pusha
    ; Build DAP for BIOS INT 13h extensions
    mov ax, [kernel_sectors]
    mov [dap_count], ax
    mov eax, [kernel_lba]
    mov [dap_lba], eax
    mov word [dap_offset], KERNEL_LOAD_OFF
    mov word [dap_segment], KERNEL_LOAD_SEG

    mov si, dap
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error
    popa
    ret

disk_error:
    cli
    hlt
    jmp disk_error

; print a byte to COM1 for headless debugging
print_serial_char:
    push dx
    push ax
    mov dx, 0x3F8
.wait_txe2:
    in al, 0x3FD
    test al, 0x20
    jz .wait_txe2
    pop ax
    out dx, al
    pop dx
    ret

enable_a20:
    in al, 0x92
    or al, 00000010b
    out 0x92, al
    ret

; ---------------- Data ----------------
BOOT_DRIVE db 0

; BIOS disk address packet (EBIOS)
dap:
    db 0x10
    db 0
dap_count   dw 0
dap_offset  dw 0
dap_segment dw 0
dap_lba     dq 0

; GDT
gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF     ; 32-bit code
    dq 0x00CF92000000FFFF     ; 32-bit data
    dq 0x00AF9A000000FFFF     ; 64-bit code
    dq 0x00AF92000000FFFF     ; 64-bit data
gdt_end:

; GDTR structure (limit (word), base (dword)) - will be filled at runtime
gdt_descriptor:
    dw 0
    dd 0

; Build paging structures (32-bit) into low memory
[BITS 32]
setup_paging32:
    mov edi, PML4_ADDR
    xor eax, eax
    mov ecx, (4096*3)/4
    rep stosd

    ; pml4[0] -> PDPT
    mov dword [PML4_ADDR], PDPT_ADDR | 0x3
    mov dword [PML4_ADDR+4], 0
    ; pdpt[0] -> PD
    mov dword [PDPT_ADDR], PD_ADDR | 0x3
    mov dword [PDPT_ADDR+4], 0
    
    ; Identity map first 32MB using 2MB pages (pd[0] through pd[15])
    ; This ensures VGA buffer at 0xB8000 and kernel space are covered
    mov edi, PD_ADDR
    mov eax, 0x00000083      ; Present, RW, PS (2MB pages)
    mov ecx, 16              ; 16 entries = 32MB
.loop:
    mov [edi], eax
    mov dword [edi+4], 0
    add eax, 0x00200000      ; Next 2MB
    add edi, 8
    loop .loop
    
    ret
[BITS 16]

pm_far_ptr:
    dd pm_entry
    dw CODE32_SEL

; Metadata marker patched by Makefile
meta_marker db 'META'
kernel_lba dd 0
kernel_sectors dd 0
stage2_sectors dd 0