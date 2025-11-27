#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"

// Simple string length
static int str_len(const char *s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

// Simple string compare for prefix
static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++;
        prefix++;
    }
    return 1;
}

static void echo_handler(const char *arg1, const char *arg2) {
    if (!arg1 || arg1[0] == 0) {
        shell_write_line("");
        return;
    }
    
    // Check for redirect: echo text > filename
    // For simplicity, arg1 is text, arg2 might be ">" or filename after ">"
    // We'll look for pattern: echo <text> > <filename>
    // This is a simple implementation - real shell parsing is more complex
    
    // Just echo the text for now
    shell_write(arg1);
    if (arg2 && arg2[0]) {
        shell_write(" ");
        shell_write(arg2);
    }
    shell_write_line("");
}

const command_t CMD_ECHO = {
    .name = "echo",
    .help = "echo: display a line of text",
    .handler = echo_handler
};