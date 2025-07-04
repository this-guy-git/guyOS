bits 16
org 0x7C00

start:
    ; Set up stack and segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Clear screen
    mov ax, 0x0003
    int 0x10

    ; Print bootloader marker ('B')
    mov ah, 0x0E
    mov al, 'B'
    int 0x10

    ; Load kernel (2 sectors) to 0x7E00
    mov ah, 0x02
    mov al, 2
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov bx, 0x7E00
    int 0x13
    jc disk_error

    ; Jump to kernel
    jmp 0x0000:0x7E00

disk_error:
    mov si, error_msg
    call print
    hlt

print:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret

error_msg db "Disk error!", 0

times 510-($-$$) db 0
dw 0xAA55

; ===== KERNEL =====
kernel_start:
    ; Print kernel marker ('K')
    mov ah, 0x0E
    mov al, 'K'
    int 0x10

    ; Print welcome message
    mov si, welcome_msg
    call print

    ; Initialize variables
    mov byte [cursor_pos], 2
    mov byte [cmd_length], 0

    ; Show initial prompt
    mov si, prompt
    call print

    ; Main input loop
.input_loop:
    ; Get key (blocking)
    xor ah, ah
    int 0x16

    ; Handle special keys
    cmp al, 0x0D    ; Enter
    je .handle_enter
    cmp al, 0x08    ; Backspace
    je .handle_backspace

    ; Regular character - echo and store
    call .store_char
    jmp .input_loop

.handle_enter:
    ; Move to next line (CR+LF)
    mov ah, 0x0E
    mov al, 0x0D    ; Carriage return
    int 0x10
    mov al, 0x0A    ; Line feed
    int 0x10
    
    ; Show new prompt
    mov si, prompt
    call print
    
    ; Reset command state
    mov byte [cursor_pos], 2
    mov byte [cmd_length], 0
    jmp .input_loop

.handle_backspace:
    ; Only backspace if we have characters to delete (beyond prompt)
    cmp byte [cursor_pos], 2
    jle .input_loop

    ; Move cursor back
    mov ah, 0x0E
    mov al, 0x08    ; Backspace
    int 0x10
    mov al, ' '     ; Space
    int 0x10
    mov al, 0x08    ; Backspace again
    int 0x10

    dec byte [cursor_pos]
    dec byte [cmd_length]
    jmp .input_loop

.store_char:
    ; Check buffer limit
    cmp byte [cmd_length], MAX_CMD_LEN
    jge .input_loop

    ; Store character in buffer
    movzx bx, [cmd_length]
    mov [current_cmd + bx], al
    inc byte [cmd_length]

    ; Echo character
    mov ah, 0x0E
    int 0x10
    inc byte [cursor_pos]
    ret

; Constants
MAX_CMD_LEN equ 40

; Data section
cursor_pos db 0
cmd_length db 0

current_cmd times MAX_CMD_LEN db 0

welcome_msg db 0x0D, 0x0A, "Terminal OS", 0x0D, 0x0A, 0
prompt db "> ", 0

; Pad to 3 sectors (1536 bytes)
times 1536-($-$$) db 0