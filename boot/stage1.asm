; stage1: MBR loader, BIOS, loads stage2 from disk to 0x8000
; STAGE2_SECTORS is filled by Makefile
[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [BOOT_DRIVE], dl

    ; debug marker '1' (VGA)
    mov al, '1'
    call print_char
    ; also send to serial for headless logging
    mov al, '1'
    call print_serial_char

    ; Load stage2 using BIOS INT13h extensions to 0x8000:0000
    mov si, disk_address_packet
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc boot_fail

    ; debug marker '2' (VGA)
    mov al, '2'
    call print_char
    ; also send to serial for headless logging
    mov al, '2'
    call print_serial_char

    jmp 0x8000:0000

boot_fail:
    mov si, fail_msg
    call print
    hlt

print:
    mov ah, 0x0E
.loop:
    lodsb
    or al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

print_char:
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    ret

; print a single character to COM1 (0x3F8) -- waits until THR empty
print_serial_char:
    push dx
    push ax        ; save character (in AL)
    mov dx, 0x3F8
.wait_txe:
    in al, 0x3FD    ; read LSR into AL (overwrites AL temporarily)
    test al, 0x20  ; Transmitter Holding Register Empty
    jz .wait_txe
    pop ax         ; restore original character into AL
    out dx, al
    pop dx
    ret

disk_address_packet:
    db 0x10           ; size
    db 0x00           ; reserved
    dw STAGE2_SECTORS ; sectors to read
    dw 0x0000         ; offset
    dw 0x8000         ; segment
    dq 1              ; LBA start for stage2

fail_msg db "Stage1 load error",0

BOOT_DRIVE db 0

times 510-($-$$) db 0
dw 0xAA55
