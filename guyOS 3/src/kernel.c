// kernel.c
#include "os.h"

void kernel_main() {
    clear_screen();
    print_string("Welcome to guyOS CLI!\n> ");

    char input_buffer[128];
    int input_pos = 0;

    while (1) {
        int sc = kbd_getchar();
        char c = scancode_to_ascii(sc);
        if (c == '\b') {
            if (input_pos > 0) {
                input_pos--;
                put_char('\b');
            }
        } else if (c == '\n') {
            put_char('\n');
            input_buffer[input_pos] = 0;
            run_command(input_buffer);
            input_pos = 0;
            print_string("> ");
        } else if (c >= 32 && c <= 126 && input_pos < 127) {
            input_buffer[input_pos++] = c;
            put_char(c);
        }
    }
}
