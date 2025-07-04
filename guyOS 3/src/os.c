// os.c
#include "os.h"

unsigned int cursor_pos = 0;

void clear_screen(void) {
    for (int i = 0; i < MAX_ROWS * MAX_COLS; i++) {
        VIDEO_MEM[i] = (TEXT_COLOR << 8) | ' ';
    }
    cursor_pos = 0;
    vga_set_cursor(cursor_pos);
    vga_enable_cursor();
}

void put_char(char c) {
    if (c == '\n') {
        cursor_pos += MAX_COLS - (cursor_pos % MAX_COLS);
    } else if (c == '\b') {
        if (cursor_pos > 0) cursor_pos--;
        VIDEO_MEM[cursor_pos] = (TEXT_COLOR << 8) | ' ';
    } else {
        VIDEO_MEM[cursor_pos++] = (TEXT_COLOR << 8) | c;
    }

    if (cursor_pos >= MAX_ROWS * MAX_COLS) {
        cursor_pos = 0;
    }

    vga_set_cursor(cursor_pos);
}

void print_string(const char* str) {
    int i = 0;
    while (str[i]) {
        put_char(str[i++]);
    }
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, unsigned int n) {
    for (unsigned int i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == 0 || s2[i] == 0)
            return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
    return 0;
}

int kbd_getchar(void) {
    unsigned char scancode;

    do {
        scancode = inb(0x60);
    } while (scancode & 0x80);

    unsigned char last_scancode;
    do {
        last_scancode = inb(0x60);
    } while (!(last_scancode & 0x80));

    return scancode;
}

char scancode_to_ascii(unsigned char sc) {
    const char map[128] = { /* same map as before */ };
    return (sc < 128) ? map[sc] : 0;
}
