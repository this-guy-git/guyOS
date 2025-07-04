#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

// Color definitions (ARGB format)
#define COLOR_BLACK    0xFF000000
#define COLOR_WHITE    0xFFFFFFFF
#define COLOR_GRAY     0xFF808080
#define COLOR_DARKGRAY 0xFF202020
#define COLOR_BLUE     0xFF0000FF
#define COLOR_WINDOW_BG 0xFF404040
#define COLOR_TASKBAR  0xFF202020


#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

void graphics_init();
void graphics_clear_screen(uint32_t color);
void graphics_put_pixel(int x, int y, uint32_t color);
void graphics_draw_rect(int x, int y, int w, int h, uint32_t color);
void graphics_draw_window(int x, int y, int w, int h, const char* title);
void graphics_draw_taskbar();
void graphics_draw_mouse_cursor(int x, int y);

#endif
