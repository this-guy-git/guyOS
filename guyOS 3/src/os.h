// os.h
#ifndef OS_H
#define OS_H

#include <stdint.h>

#define MAX_ROWS 25
#define MAX_COLS 80
#define COLOR(fg, bg) ((bg << 4) | (fg))
#define TEXT_COLOR COLOR(7, 0)

extern unsigned short* VIDEO_MEM;
extern unsigned int cursor_pos;

// IO functions
void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);
void vga_write_reg(unsigned short port, unsigned char val);
void vga_set_cursor(unsigned short pos);
void vga_enable_cursor(void);
void vga_disable_cursor(void);

// Kernel stuff
void clear_screen(void);
void put_char(char c);
void print_string(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, unsigned int n);
int kbd_getchar(void);
char scancode_to_ascii(unsigned char sc);

// Commands
void run_command(const char* cmd);
void reboot(void);
void shutdown(void);
void neofetch(void);

// CPU Info
void cpuid(int code, unsigned int* eax, unsigned int* ebx, unsigned int* ecx, unsigned int* edx);
void get_cpu_vendor(char *out);
void get_cpu_brand(char *out);

#endif
