#include "graphics.h"
#include "keyboard.h"
#include "mouse.h"

extern void kernel_main();

void kernel_main() {
    graphics_init();
    keyboard_init();
    mouse_init();

    graphics_clear_screen(COLOR_BLACK);
    graphics_draw_taskbar();
    graphics_draw_window(50, 50, 300, 200, "guyOS Window");

    while (1) {
        keyboard_handle_events();
        mouse_handle_events();

        // Redraw taskbar and window to prevent mouse artifacts (simple double buffering would be better)
        graphics_draw_taskbar();
        graphics_draw_window(50, 50, 300, 200, "guyOS Window");

        // Draw mouse cursor at current position
        int mx = mouse_get_x();
        int my = mouse_get_y();
        graphics_draw_mouse_cursor(mx, my);
    }
}
