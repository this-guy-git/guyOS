ENTRY(kernel_main)

SECTIONS
{
    . = 0x100000;

    .text ALIGN(4K) : {
        KEEP(*(.multiboot))  /* THIS MUST MATCH your ASM section */
        *(.text*)
    }

    .rodata ALIGN(4K) : { *(.rodata*) }
    .data ALIGN(4K) : { *(.data*) }
    .bss ALIGN(4K) : { *(.bss*) *(COMMON) }
}
