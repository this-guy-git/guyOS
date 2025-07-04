#include "graphics.h"

volatile uint32_t* framebuffer = (uint32_t*)0xE0000000; // We'll map to this in real setup

// Dummy color macros (ARGB)
#define COLOR_BLACK   0xFF000000
#define COLOR_WHITE   0xFFFFFFFF
#define COLOR_GRAY    0xFF808080
#define COLOR_DARKGRAY 0xFF202020
#define COLOR_BLUE    0xFF0000FF
#define COLOR_WINDOW_BG 0xFF404040
#define COLOR_TASKBAR 0xFF202020

void graphics_init() {
    // Normally set VESA mode here - simplified: assume framebuffer at 0xE0000000
    // Your bootloader or GRUB should set video mode for you
}

void graphics_clear_screen(uint32_t color) {
    for (int y = 0; y < SCREEN_HEIGHT; y++)
        for (int x = 0; x < SCREEN_WIDTH; x++)
            graphics_put_pixel(x, y, color);
}

void graphics_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
        return;
    framebuffer[y * SCREEN_WIDTH + x] = color;
}

void graphics_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            graphics_put_pixel(xx, yy, color);
}

void graphics_draw_window(int x, int y, int w, int h, const char* title) {
    // Background
    graphics_draw_rect(x, y, w, h, COLOR_WINDOW_BG);

    // Borders
    graphics_draw_rect(x, y, w, 3, COLOR_WHITE);           // top
    graphics_draw_rect(x, y + h - 3, w, 3, COLOR_WHITE);   // bottom
    graphics_draw_rect(x, y, 3, h, COLOR_WHITE);           // left
    graphics_draw_rect(x + w - 3, y, 3, h, COLOR_WHITE);   // right

    // TODO: Draw title text (skip for now)
}

void graphics_draw_taskbar() {
    graphics_draw_rect(0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, 30, COLOR_TASKBAR);
}

void graphics_draw_mouse_cursor(int x, int y) {
    // Simple white 5x5 square cursor
    graphics_draw_rect(x, y, 5, 5, COLOR_WHITE);
}
