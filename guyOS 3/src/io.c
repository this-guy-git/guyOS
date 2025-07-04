// io.c
#include "os.h"

unsigned short* VIDEO_MEM = (unsigned short*)0xB8000;

void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void vga_write_reg(unsigned short port, unsigned char val) {
    outb(port, val);
}

void vga_set_cursor(unsigned short pos) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void vga_enable_cursor(void) {
    outb(0x3D4, 0x0A); outb(0x3D5, 13);
    outb(0x3D4, 0x0B); outb(0x3D5, 15);
}

void vga_disable_cursor(void) {
    vga_write_reg(0x3D4, 0x0A);
    vga_write_reg(0x3D5, 0x20);
}
