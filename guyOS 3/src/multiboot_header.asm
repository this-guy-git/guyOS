; multiboot_header.asm
section .multiboot
    align 4
    dd 0x1BADB002              ; magic number
    dd 0x00010003              ; flags
    dd -(0x1BADB002 + 0x00010003) ; checksum
