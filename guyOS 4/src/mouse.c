#include "mouse.h"

static int mouse_x = 320;
static int mouse_y = 240;

void mouse_init() {
    mouse_x = 320;
    mouse_y = 240;
}

void mouse_handle_events() {
    static int dx = 1;
    static int dy = 1;

    mouse_x += dx;
    mouse_y += dy;

    if (mouse_x > 635 || mouse_x < 0) dx = -dx;
    if (mouse_y > 475 || mouse_y < 0) dy = -dy;
}

int mouse_get_x() {
    return mouse_x;
}

int mouse_get_y() {
    return mouse_y;
}
