; commands.asm - Command processing system for guyOS
; This file contains all command implementations and the command dispatcher

; Command processing entry point
; Input: current_cmd buffer contains the command string
; Output: executes the command or shows error
process_command:
    ; Skip if empty command
    cmp byte [cmd_length], 0
    je .done
    
    ; Null terminate the command for easier processing
    movzx bx, byte [cmd_length]
    mov byte [current_cmd + bx], 0
    
    ; Check for echo command first (special case since it has arguments)
    call check_echo
    test al, al
    jz .done            ; Echo was executed
    
    ; Try exact match commands
    mov si, current_cmd
    
    ; Check for "help" command
    mov di, cmd_help
    call compare_strings
    test al, al
    jnz .try_clear
    call show_help
    jmp .done
    
.try_clear:
    mov di, cmd_clear
    call compare_strings
    test al, al
    jnz .try_time
    call clear_screen
    jmp .done

.try_time:
    mov di, cmd_time
    call compare_strings
    test al, al
    jnz .try_reboot
    call show_time
    jmp .done

.try_reboot:
    mov di, cmd_reboot
    call compare_strings
    test al, al
    jnz .try_info
    call reboot_system
    jmp .done

.try_info:
    mov di, cmd_info
    call compare_strings
    test al, al
    jnz .try_mem
    call show_info
    jmp .done

.try_mem:
    mov di, cmd_mem
    call compare_strings
    test al, al
    jnz .try_neofetch
    call show_memory
    jmp .done

.try_neofetch:
    mov di, cmd_neofetch
    call compare_strings
    test al, al
    jnz .unknown_command
    call show_neofetch
    jmp .done

.unknown_command:
    mov si, msg_unknown
    call print
    mov si, current_cmd
    call print
    mov si, msg_try_help
    call print

.done:
    ret

; Check if command starts with "echo" and handle it
; Returns: AL = 0 if echo was handled, AL = 1 if not echo
check_echo:
    mov si, current_cmd
    mov di, cmd_echo
    mov cx, 4           ; Length of "echo"
    
    ; Compare first 4 characters
.check_loop:
    mov al, [si]
    mov ah, [di]
    
    ; Convert to lowercase
    cmp al, 'A'
    jb .skip_lower1
    cmp al, 'Z'
    ja .skip_lower1
    add al, 32
.skip_lower1:
    
    cmp ah, 'A'
    jb .skip_lower2
    cmp ah, 'Z'
    ja .skip_lower2
    add ah, 32
.skip_lower2:
    
    cmp al, ah
    jne .not_echo
    
    inc si
    inc di
    loop .check_loop
    
    ; Check if next character is space or end of string
    mov al, [si]
    cmp al, 0
    je .is_echo         ; Just "echo" with nothing after
    cmp al, ' '
    je .is_echo         ; "echo " with something after
    jmp .not_echo       ; Something like "echotest"
    
.is_echo:
    call echo_command
    xor al, al          ; Return 0 (handled)
    ret
    
.not_echo:
    mov al, 1           ; Return 1 (not echo)
    ret

; SI = string1, DI = string2
; Returns: AL = 0 if equal, non-zero if different
compare_strings:
    push si
    push di
.compare_loop:
    mov al, [si]
    mov ah, [di]
    
    ; Convert to lowercase for case-insensitive comparison
    cmp al, 'A'
    jb .skip_lower1
    cmp al, 'Z'
    ja .skip_lower1
    add al, 32
.skip_lower1:
    
    cmp ah, 'A'
    jb .skip_lower2
    cmp ah, 'Z'
    ja .skip_lower2
    add ah, 32
.skip_lower2:
    
    cmp al, ah
    jne .not_equal
    
    ; Check if we reached end of both strings
    test al, al
    jz .equal
    
    inc si
    inc di
    jmp .compare_loop
    
.equal:
    xor al, al
    jmp .done
.not_equal:
    mov al, 1
.done:
    pop di
    pop si
    ret

; HELP command
show_help:
    mov si, msg_help_header
    call print
    mov si, msg_help_commands
    call print
    ret

; CLEAR command
clear_screen:
    mov ax, 0x0003
    int 0x10
    ; Print bootloader and kernel markers to show system is still running
    mov ah, 0x0E
    mov al, 'B'
    int 0x10
    mov al, 'K'
    int 0x10
    mov si, msg_cleared
    call print
    ret

; ECHO command
echo_command:
    ; Find start of text after "echo"
    mov si, current_cmd
    
    ; Skip past "echo"
.skip_echo:
    lodsb
    cmp al, 0
    je .no_text
    cmp al, ' '
    jne .skip_echo
    
    ; Skip any additional spaces
.skip_spaces:
    lodsb
    cmp al, ' '
    je .skip_spaces
    cmp al, 0
    je .no_text
    
    ; We found the first non-space character
    ; Back up one position since we already read it
    dec si
    
    ; Print the remaining text
    call print
    jmp .done
    
.no_text:
    ; Just print a newline if no text provided
.done:
    mov si, newline
    call print
    ret

; TIME command (simple implementation)
show_time:
    mov si, msg_time
    call print
    
    ; Get system time from BIOS
    mov ah, 0x02
    int 0x1A
    jc .time_error
    
    ; CH = hours (BCD), CL = minutes (BCD), DH = seconds (BCD)
    ; Convert and display hours
    mov al, ch
    call print_bcd
    mov al, ':'
    mov ah, 0x0E
    int 0x10
    
    ; Display minutes
    mov al, cl
    call print_bcd
    mov al, ':'
    mov ah, 0x0E
    int 0x10
    
    ; Display seconds
    mov al, dh
    call print_bcd
    
    mov si, newline
    call print
    ret

.time_error:
    mov si, msg_time_error
    call print
    ret

; Convert BCD to ASCII and print
print_bcd:
    push ax
    ; Print tens digit
    mov ah, al
    shr ah, 4
    add ah, '0'
    push ax
    mov al, ah
    mov ah, 0x0E
    int 0x10
    pop ax
    
    ; Print ones digit
    and al, 0x0F
    add al, '0'
    mov ah, 0x0E
    int 0x10
    pop ax
    ret

; REBOOT command
reboot_system:
    mov si, msg_rebooting
    call print
    
    ; Wait a moment
    mov cx, 0xFFFF
.wait:
    loop .wait
    
    ; Triple fault to reboot
    cli
    lidt [invalid_idt]
    int 3

invalid_idt:
    dw 0
    dd 0

; INFO command
show_info:
    mov si, msg_info_header
    call print
    mov si, msg_info_details
    call print
    ret

; MEM command (show available memory)
show_memory:
    mov si, msg_mem_header
    call print
    
    ; Get conventional memory size (BIOS)
    int 0x12
    ; AX = KB of conventional memory
    
    ; Convert to string and display
    call print_number
    mov si, msg_mem_kb
    call print
    ret

; NEOFETCH command
show_neofetch:
    ; ASCII art logo (simplified)
    mov si, neofetch_logo_line1
    call print
    mov si, neofetch_logo_line2
    call print
    mov si, neofetch_logo_line3
    call print
    mov si, neofetch_logo_line4
    call print
    
    ; System information
    mov si, neofetch_os_line
    call print
    
    ; Get and display memory
    mov si, neofetch_mem_line
    call print
    int 0x12        ; Get conventional memory in AX (KB)
    call print_number
    mov si, neofetch_mem_kb
    call print
    
    ; Display time
    mov si, neofetch_time_line
    call print
    mov ah, 0x02    ; Get RTC time
    int 0x1A
    jc .time_error
    
    ; Display hours
    mov al, ch      ; Hours in BCD
    call print_bcd
    mov al, ':'
    mov ah, 0x0E
    int 0x10
    
    ; Display minutes
    mov al, cl      ; Minutes in BCD
    call print_bcd
    mov al, ':'
    mov ah, 0x0E
    int 0x10
    
    ; Display seconds
    mov al, dh      ; Seconds in BCD
    call print_bcd
    mov si, newline
    call print
    ret
    
.time_error:
    mov si, msg_time_error
    call print
    ret

; Print number in AX as decimal
print_number:
    push ax
    push bx
    push cx
    push dx
    
    mov bx, 10
    xor cx, cx
    
    ; Handle zero case
    test ax, ax
    jnz .convert_loop
    mov al, '0'
    mov ah, 0x0E
    int 0x10
    jmp .done
    
.convert_loop:
    xor dx, dx
    div bx
    add dl, '0'
    push dx
    inc cx
    test ax, ax
    jnz .convert_loop
    
.print_loop:
    pop dx
    mov al, dl
    mov ah, 0x0E
    int 0x10
    loop .print_loop
    
.done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Command strings
cmd_help db "help", 0
cmd_clear db "clear", 0
cmd_echo db "echo", 0
cmd_time db "time", 0
cmd_reboot db "reboot", 0
cmd_info db "info", 0
cmd_mem db "mem", 0
cmd_neofetch db "neofetch", 0

; Messages
msg_unknown db "Unknown command: ", 0
msg_try_help db 0x0D, 0x0A, "Type 'help' for available commands.", 0x0D, 0x0A, 0

msg_help_header db "Available commands:", 0x0D, 0x0A, 0
msg_help_commands db "  help     - Show this help", 0x0D, 0x0A
                 db "  clear    - Clear screen", 0x0D, 0x0A
                 db "  echo     - Echo text", 0x0D, 0x0A
                 db "  time     - Show current time", 0x0D, 0x0A
                 db "  info     - System information", 0x0D, 0x0A
                 db "  mem      - Show memory info", 0x0D, 0x0A
                 db "  reboot   - Restart system", 0x0D, 0x0A
                 db "  neofetch - Display system info", 0x0D, 0x0A, 0

msg_cleared db "Screen cleared.", 0x0D, 0x0A, 0

msg_time db "Current time: ", 0
msg_time_error db "Error reading system time", 0x0D, 0x0A, 0

msg_rebooting db "Rebooting system...", 0x0D, 0x0A, 0

msg_info_header db "guyOS v1.0", 0x0D, 0x0A, 0
msg_info_details db "A simple 16-bit operating system", 0x0D, 0x0A
                db "Features: Command line interface", 0x0D, 0x0A
                db "Author: this-guy-git", 0x0D, 0x0A, 0

msg_mem_header db "Memory Information:", 0x0D, 0x0A
              db "Conventional memory: ", 0
msg_mem_kb db " KB", 0x0D, 0x0A, 0

; Neofetch messages
neofetch_logo_line1 db "              /~~\ /~~\", 0x0D, 0x0A, 0
neofetch_logo_line2 db "/~~||   |\  /|    |`--.", 0x0D, 0x0A, 0
neofetch_logo_line3 db "\__| \_/| \/  \__/ \__/", 0x0D, 0x0A, 0
neofetch_logo_line4 db "\__|     _/            ", 0x0D, 0x0A, 0

neofetch_os_line    db 0x0D, 0x0A, "OS: guyOS v1.0", 0x0D, 0x0A, 0
neofetch_mem_line   db "Memory: ", 0
neofetch_mem_kb     db " KB", 0x0D, 0x0A, 0
neofetch_time_line  db "Time: ", 0

newline db 0x0D, 0x0A, 0