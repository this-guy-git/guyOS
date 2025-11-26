#include <stdint.h>
#include <stdbool.h>
#include "../include/hal.h"
#include "../include/shell.h"

static void serial_putc(char c) {
    while (!(inb(0x3FD) & 0x20)) {
        io_wait();
    }
    outb(0x3F8, (uint8_t)c);
}

void kmain(void) {
    serial_putc('K');  // 'K' from kmain
    serial_putc('1');  // Made it past first serial call
    
    // Test: can it write to VGA memory?
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    serial_putc('2');  // About to write to VGA
    
    vga[0] = 0x0F41;  // White 'A' on black
    serial_putc('3');  // VGA write succeeded
    
    vga[1] = 0x0F42;  // White 'B' on black
    serial_putc('4');  // Second VGA write succeeded
    
    // Now try calling shell_start
    serial_putc('5');  // About to call shell_start
    shell_start();
    serial_putc('6');  // Returned from shell_start (shouldn't happen)
    
    // Infinite loop
    while(1) {
        __asm__ volatile("hlt");
    }
}