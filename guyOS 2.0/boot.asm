bits 16
org 0x7C00

start:
    ; Save boot drive number (BIOS passes it in DL)
    mov [boot_drive], dl
    
    ; Set up stack and segments
    cli                 ; Disable interrupts during setup
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti                 ; Re-enable interrupts

    ; Clear screen
    mov ax, 0x0003
    int 0x10

    ; Print bootloader marker ('B')
    mov ah, 0x0E
    mov al, 'B'
    int 0x10

    ; Load kernel (4 sectors) to 0x7E00 - increased to accommodate commands
    ; Try multiple times and different drive numbers
    mov cx, 3           ; Retry count
    
load_retry:
    ; First try with boot drive (stored in DL by BIOS)
    push cx
    mov ah, 0x02        ; Read sectors function
    mov al, 4           ; Number of sectors to read (increased from 2)
    mov ch, 0           ; Cylinder 0
    mov cl, 2           ; Start from sector 2 (sector 1 is bootloader)
    mov dh, 0           ; Head 0
    mov dl, [boot_drive] ; Use boot drive from BIOS
    mov bx, 0x7E00      ; Load address
    int 0x13
    pop cx
    jnc load_success    ; Jump if no carry (success)
    
    ; If that failed, try floppy drive
    push cx
    mov ah, 0x02
    mov al, 4
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0x00        ; Floppy drive A:
    mov bx, 0x7E00
    int 0x13
    pop cx
    jnc load_success
    
    ; Reset disk system and try again
    mov ah, 0x00        ; Reset disk system
    mov dl, 0x00        ; Drive 0 (floppy)
    int 0x13
    
    loop load_retry     ; Try again
    jmp disk_error      ; All attempts failed

load_success:
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
boot_drive db 0

times 510-($-$$) db 0
dw 0xAA55

; ===== KERNEL =====
kernel_start:
    ; Ensure interrupts are enabled
    sti
    
    ; Print kernel marker ('K')
    mov ah, 0x0E
    mov al, 'K'
    int 0x10

    ; Print welcome message
    mov si, welcome_msg
    call print

    ; Initialize variables
    mov byte [cursor_pos], 2    ; Position after prompt
    mov byte [cmd_length], 0    ; Length of current command
    mov byte [cursor_offset], 0 ; Cursor position within command

    ; Show initial prompt
    mov si, prompt
    call print

    ; Main input loop
input_loop:
    ; Get key (blocking) - use BIOS keyboard service
    mov ah, 0x00        ; Get keystroke function
    int 0x16
    
    ; AL = ASCII character, AH = scan code
    ; Check if it's an extended key (AH contains scan code)
    cmp al, 0x00        ; Extended key has AL = 0
    je extended_key
    
    ; Handle special ASCII keys
    cmp al, 0x0D        ; Enter (CR)
    je handle_enter
    cmp al, 0x08        ; Backspace
    je handle_backspace
    cmp al, 0x09        ; Tab - ignore for now
    je input_loop
    cmp al, 0x1B        ; Escape - ignore for now
    je input_loop
    
    ; Only accept printable ASCII (32-126)
    cmp al, 32
    jb input_loop
    cmp al, 126
    ja input_loop
    
    ; Store and print character
    call store_char
    jmp input_loop

extended_key:
    ; Handle extended keys using scan codes in AH
    cmp ah, 0x4B        ; Left arrow scan code
    je handle_left
    cmp ah, 0x4D        ; Right arrow scan code
    je handle_right
    cmp ah, 0x48        ; Up arrow - ignore for now
    je input_loop
    cmp ah, 0x50        ; Down arrow - ignore for now
    je input_loop
    jmp input_loop      ; Ignore other extended keys

handle_left:
    ; Only move left if not at start
    cmp byte [cursor_offset], 0
    je input_loop
    
    ; Move cursor left visually using backspace
    mov ah, 0x0E
    mov al, 0x08        ; Backspace character
    int 0x10
    
    dec byte [cursor_offset]
    dec byte [cursor_pos]
    jmp input_loop

handle_right:
    ; Only move right if not at end
    mov al, [cursor_offset]
    cmp al, [cmd_length]
    jae input_loop
    
    ; Print character at current position (moves cursor right)
    movzx bx, byte [cursor_offset]
    mov al, [current_cmd + bx]
    mov ah, 0x0E
    int 0x10
    
    inc byte [cursor_offset]
    inc byte [cursor_pos]
    jmp input_loop

handle_enter:
    ; Move to next line
    mov ah, 0x0E
    mov al, 0x0D        ; CR
    int 0x10
    mov al, 0x0A        ; LF
    int 0x10
    
    ; Process the command before showing new prompt
    call process_command
    
    ; Show new prompt
    mov si, prompt
    call print
    
    ; Reset command state
    mov byte [cursor_pos], 2
    mov byte [cmd_length], 0
    mov byte [cursor_offset], 0
    
    ; Clear command buffer
    mov di, current_cmd
    mov cx, MAX_CMD_LEN + 1
    xor al, al
    rep stosb
    
    jmp input_loop

handle_backspace:
    ; Only backspace if not at start
    cmp byte [cursor_offset], 0
    je input_loop

    ; Visual backspace: move cursor left, print space, move left again
    mov ah, 0x0E
    mov al, 0x08        ; Backspace
    int 0x10
    
    ; Shift characters left in buffer
    movzx bx, byte [cursor_offset]
    dec bx              ; Move to position we're deleting
    mov si, current_cmd
    add si, bx
    inc si              ; Source: next character
    mov di, si
    dec di              ; Dest: current position
    movzx cx, byte [cmd_length]
    sub cx, bx
    dec cx              ; Number of chars to move
    jcxz .skip_shift
    rep movsb
.skip_shift:
    
    ; Update counters
    dec byte [cursor_offset]
    dec byte [cursor_pos]
    dec byte [cmd_length]
    
    ; Clear the last character in buffer
    movzx bx, byte [cmd_length]
    mov byte [current_cmd + bx], 0
    
    ; Reprint line from cursor position
    call reprint_line
    jmp input_loop

store_char:
    ; Check buffer limit
    cmp byte [cmd_length], MAX_CMD_LEN
    jge .store_done

    ; If not at end, make space for new character
    mov bl, [cursor_offset]
    cmp bl, [cmd_length]
    je .append_char

    ; Shift characters right to make space
    movzx si, byte [cmd_length]    ; Start from end
    mov di, si
    inc di                         ; Move to new end position
    add si, current_cmd
    add di, current_cmd
    movzx cx, byte [cmd_length]
    sub cl, [cursor_offset]        ; Number of chars to move
    jcxz .append_char
    std                            ; Move backwards
    rep movsb
    cld                            ; Reset direction flag

.append_char:
    ; Store character in buffer
    movzx bx, byte [cursor_offset]
    mov [current_cmd + bx], al
    
    ; Print character
    mov ah, 0x0E
    int 0x10
    
    ; If not at end, reprint remaining characters
    mov al, [cursor_offset]
    cmp al, [cmd_length]
    je .update_length
    
    call reprint_remaining

.update_length:
    inc byte [cmd_length]
    inc byte [cursor_offset]
    inc byte [cursor_pos]

.store_done:
    ret

reprint_line:
    ; Save current position for later restoration
    push ax
    push bx
    push cx
    push dx
    
    ; Print remaining characters from cursor position
    movzx si, byte [cursor_offset]
    add si, current_cmd
    movzx cx, byte [cmd_length]
    sub cl, [cursor_offset]
    jcxz .clear_extra
    
.print_loop:
    lodsb
    mov ah, 0x0E
    int 0x10
    loop .print_loop

.clear_extra:
    ; Print an extra space to clear any leftover character
    mov ah, 0x0E
    mov al, ' '
    int 0x10
    
    ; Move cursor back to correct position
    movzx cx, byte [cmd_length]
    sub cl, [cursor_offset]
    inc cx              ; +1 for the extra space we printed
    jcxz .done
.move_back:
    mov ah, 0x0E
    mov al, 0x08        ; Backspace
    int 0x10
    loop .move_back

.done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

reprint_remaining:
    ; Save registers
    push ax
    push bx
    push cx
    push si
    
    ; Print characters after cursor position
    movzx si, byte [cursor_offset]
    inc si
    add si, current_cmd
    movzx cx, byte [cmd_length]
    sub cl, [cursor_offset]
    jcxz .move_back
    
.print_remaining:
    lodsb
    mov ah, 0x0E
    int 0x10
    loop .print_remaining
    
.move_back:
    ; Move cursor back to insertion point
    movzx cx, byte [cmd_length]
    sub cl, [cursor_offset]
    jcxz .done
.move_loop:
    mov ah, 0x0E
    mov al, 0x08
    int 0x10
    loop .move_loop

.done:
    pop si
    pop cx
    pop bx
    pop ax
    ret

; Constants
MAX_CMD_LEN equ 40

; Data section
cursor_pos db 0
cmd_length db 0
cursor_offset db 0

current_cmd times MAX_CMD_LEN+1 db 0  ; +1 for null terminator

welcome_msg db 0x0D, 0x0A, "guyOS v1.0", 0x0D, 0x0A, "Type 'help' for commands.", 0x0D, 0x0A, 0
prompt db "> ", 0

; Include the commands module
%include "commands.asm"

; Pad to 7 sectors (3584 bytes) to accommodate the command system
times 3584-($-$$) db 0