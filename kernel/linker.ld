ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text : ALIGN(4K) {
        *(.text*)
        *(.rodata*)
    }

    .data : ALIGN(4K) {
        *(.data*)
    }

    __bss_start = .;
    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss*)
    }
    __bss_end = .;
}
