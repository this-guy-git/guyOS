section .multiboot
    align 4
    dd 0x1BADB002           ; magic number
    dd 0x00010003           ; flags
    dd 0xE5423FFB ; checksum

section .text
    global _start
    extern kernel_main      ; <<< add this line!

_start:
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
