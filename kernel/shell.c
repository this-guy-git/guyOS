#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../include/hal.h"
#include "../include/commands.h"
#include "../include/shell_api.h"
#include "../include/disk.h"
#include "../include/fat.h"
#include <stdint.h>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef __SIZE_TYPE__ size_t;

static void debug_serial(char c) {
    while (!(inb(0x3FD) & 0x20)) {}
    outb(0x3F8, (uint8_t)c);
}

static void *memcpy(void *dst, const void *src, size_t n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

static void *memset(void *dst, int v, size_t n) {
    u8 *d = (u8 *)dst;
    while (n--) *d++ = (u8)v;
    return dst;
}

static size_t strlen(const char *s) {
    size_t n = 0;
    while (s && *s++) n++;
    return n;
}

static int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) {
        a++; b++; n--;
    }
    return n ? (unsigned char)*a - (unsigned char)*b : 0;
}

static char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while (*src) *d++ = *src++;
    *d = 0;
    return dst;
}

// ---------------- VGA text output ----------------
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static inline u8 vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return (u8)(fg | bg << 4);
}

static inline u16 vga_entry(char c, u8 color) {
    return (u16)c | (u16)color << 8;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const size_t BODY_TOP = 2;
static const size_t BODY_BOTTOM = 23;
static volatile u16 *vga_buffer = (u16 *)0xB8000;

static size_t term_row = BODY_TOP;
static size_t term_col = 0;
static u8 term_color;

static const u8 COLOR_HEADER = 0x1F;   // white on blue
static const u8 COLOR_BODY = 0x0F;     // white on black
static const u8 COLOR_ACCENT = 0x0B;   // cyan on black
static const u8 COLOR_FOOTER = 0x70;   // black on light grey

static void update_cursor(void) {
    u16 pos = (u16)(term_row * VGA_WIDTH + term_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

static void terminal_setcolor(u8 color) {
    term_color = color;
}

static void terminal_putentryat(char c, u8 color, size_t x, size_t y) {
    vga_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

static void clear_row(size_t row, u8 color) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_putentryat(' ', color, x, row);
    }
}

static void terminal_clear_body(void) {
    for (size_t y = BODY_TOP; y <= BODY_BOTTOM; y++) {
        clear_row(y, COLOR_BODY);
    }
    term_row = BODY_TOP;
    term_col = 0;
    update_cursor();
}

static void terminal_scroll(void) {
    for (size_t y = BODY_TOP; y < BODY_BOTTOM; y++) {
        memcpy((void *)(vga_buffer + y * VGA_WIDTH),
               (const void *)(vga_buffer + (y + 1) * VGA_WIDTH),
               VGA_WIDTH * 2);
    }
    clear_row(BODY_BOTTOM, COLOR_BODY);
    term_row = BODY_BOTTOM;
    term_col = 0;
    update_cursor();
}

static void terminal_newline(void) {
    term_col = 0;
    if (term_row < BODY_BOTTOM) {
        term_row++;
    } else {
        terminal_scroll();
    }
    update_cursor();
}

static void terminal_putc(char c) {
    if (c == '\n') {
        terminal_newline();
        return;
    }
    terminal_putentryat(c, term_color, term_col, term_row);
    term_col++;
    if (term_col >= VGA_WIDTH) {
        terminal_newline();
    }
    update_cursor();
}

static void terminal_backspace(void) {
    if (term_col == 0) {
        if (term_row > BODY_TOP) {
            term_row--;
            term_col = VGA_WIDTH - 1;
        } else {
            return;
        }
    } else {
        term_col--;
    }
    terminal_putentryat(' ', term_color, term_col, term_row);
    update_cursor();
}

static void terminal_write(const char *s) {
    while (*s) terminal_putc(*s++);
}

static void terminal_writeln(const char *s) {
    terminal_write(s);
    terminal_putc('\n');
}

void shell_write(const char *s) { terminal_write(s); }
void shell_write_line(const char *s) { terminal_writeln(s); }

static void draw_bar(size_t row, u8 color, const char *left, const char *right) {
    clear_row(row, color);
    size_t lx = 1;
    const char *p = left;
    while (*p && lx < VGA_WIDTH) terminal_putentryat(*p++, color, lx++, row);
    size_t rx = VGA_WIDTH;
    size_t rlen = strlen(right);
    if (rlen + 1 < VGA_WIDTH) rx = VGA_WIDTH - rlen - 1;
    p = right;
    size_t idx = 0;
    while (p[idx] && (rx + idx) < VGA_WIDTH) {
        terminal_putentryat(p[idx], color, rx + idx, row);
        idx++;
    }
}

static void draw_chrome_with_footer(const char *user, const char *footer_left) {
    const char *title = "guyOS kernel shell";
    char right[40];
    strcpy(right, "user: ");
    if (user) strcpy(right + 6, user); else strcpy(right + 6, "<none>");
    terminal_setcolor(COLOR_HEADER);
    draw_bar(0, COLOR_HEADER, title, right);
    terminal_setcolor(COLOR_FOOTER);
    draw_bar(VGA_HEIGHT - 1, COLOR_FOOTER, footer_left, "vm-ready");
    terminal_setcolor(COLOR_BODY);
    clear_row(1, COLOR_BODY);
    terminal_clear_body();
    update_cursor();
}

static void draw_chrome(const char *user) {
    draw_chrome_with_footer(user, "F1 help | F5 clear | halt to stop");
}

// ---------------- Keyboard ----------------
static const char keymap[128] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\','z','x','c','v','b','n','m',',','.','/', 0, '*',
    0, ' ', 0,
};

static const char keymap_shift[128] = {
    0,  0, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|','Z','X','C','V','B','N','M','<','>','?', 0, '*',
    0, ' ', 0,
};

static bool shift_down = false;

enum {
    KEY_F1 = 0xF1,
    KEY_F5 = 0xF5
};

static int keyboard_getch(void) {
    for (;;) {
        while ((inb(0x64) & 1) == 0) {
            io_wait();
        }
        u8 sc = inb(0x60);
        if (sc == 0x2A || sc == 0x36) { shift_down = true; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift_down = false; continue; }
        if (sc & 0x80) continue; // key release
        if (sc == 0x3B) return KEY_F1;
        if (sc == 0x3F) return KEY_F5;
        if (sc == 0x1C) return '\n';
        if (sc == 0x0E) return '\b';
        char c = shift_down ? keymap_shift[sc] : keymap[sc];
        if (c) return c;
    }
}

static size_t read_line(char *buf, size_t max, bool echo, const char *prompt_prefix, const char *current_user, const char *help_msg) {
    size_t len = 0;
    while (len + 1 < max) {
        int key = keyboard_getch();
        if (key == KEY_F1) {
            terminal_putc('\n');
            if (help_msg) terminal_writeln(help_msg);
            else terminal_writeln("commands: help, clear (F5), whoami, users, adduser <u> <p>, logout, halt");
            if (prompt_prefix) terminal_write(prompt_prefix);
            continue;
        }
        if (key == KEY_F5) {
            draw_chrome(current_user ? current_user : "<login>");
            if (prompt_prefix) terminal_write(prompt_prefix);
            len = 0;
            continue;
        }
        char c = (char)key;
        if (c == '\n') {
            terminal_putc('\n');
            buf[len] = 0;
            return len;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                terminal_backspace();
            }
            continue;
        }
        if (c == 0) continue;
        buf[len++] = c;
        if (echo) terminal_putc(c);
        else terminal_putc('*');
    }
    buf[len] = 0;
    terminal_putc('\n');
    return len;
}

// ---------------- Accounts + shell ----------------
#define MAX_USERS 8

typedef struct {
    char username[16];
    char password[16];
} account_t;

static account_t accounts[MAX_USERS] = {0};
static size_t account_count = 0;
static char current_user_name[32] = "<none>";
static const char ACC_FILE_NAME[11] = "ACCOUNTSBIN";
static const uint32_t FAT_PART_LBA = 16384; // 8MB offset (must match Makefile)

bool shell_save_accounts(void) {
    uint8_t buf[512];
    for (size_t i = 0; i < sizeof(buf); i++) buf[i] = 0;
    buf[0] = (uint8_t)account_count;
    size_t offset = 1;
    for (size_t i = 0; i < account_count && offset + 32 <= 512; i++) {
        for (size_t j = 0; j < 16 && offset < 512; j++) buf[offset++] = (uint8_t)accounts[i].username[j];
        for (size_t j = 0; j < 16 && offset < 512; j++) buf[offset++] = (uint8_t)accounts[i].password[j];
    }
    return fat_write_file(ACC_FILE_NAME, buf, sizeof(buf));
}

bool shell_load_accounts(void) {
    uint8_t buf[512];
    size_t got = 0;
    if (!fat_read_file(ACC_FILE_NAME, buf, sizeof(buf), &got)) {
        account_count = 0;
        return false;
    }
    size_t cnt = buf[0];
    if (cnt == 0 || cnt > MAX_USERS) {
        account_count = 0;
        return false;
    }
    account_count = 0;
    size_t offset = 1;
    for (size_t i = 0; i < cnt && offset + 32 <= got; i++) {
        account_t *acc = &accounts[account_count++];
        for (size_t j = 0; j < 16; j++) acc->username[j] = (char)buf[offset++];
        for (size_t j = 0; j < 16; j++) acc->password[j] = (char)buf[offset++];
    }
    if (account_count > 0) strcpy(current_user_name, accounts[0].username);
    return true;
}
static int find_user(const char *name) {
    for (size_t i = 0; i < account_count; i++) {
        if (strncmp(accounts[i].username, name, 15) == 0) return (int)i;
    }
    return -1;
}

static bool add_user(const char *name, const char *pass) {
    if (account_count >= MAX_USERS) return false;
    if (find_user(name) >= 0) return false;
    account_t *acc = &accounts[account_count++];
    strcpy(acc->username, name);
    strcpy(acc->password, pass);
    return true;
}

bool shell_add_user(const char *name, const char *pass) {
    return add_user(name, pass);
}

static void list_users(void) {
    for (size_t i = 0; i < account_count; i++) {
        terminal_write("- ");
        terminal_writeln(accounts[i].username);
    }
}

void shell_list_users(void) {
    list_users();
}

const char *shell_current_user(void) {
    return current_user_name;
}

void shell_redraw(void) {
    draw_chrome(current_user_name);
}

static bool login(char *out_user) {
    char user[32];
    char pass[32];
    terminal_setcolor(COLOR_BODY);
    terminal_writeln("login required");
    terminal_writeln("available users:");
    list_users();
    const char *login_help = "Login: enter username then password. F1 shows this, F5 clears.";
    terminal_write("user> ");
    read_line(user, sizeof(user), true, "user> ", "<login>", login_help);
    terminal_write("pass> ");
    read_line(pass, sizeof(pass), false, "pass> ", "<login>", login_help);

    int idx = find_user(user);
    if (idx >= 0 && strcmp(accounts[idx].password, pass) == 0) {
        strcpy(out_user, accounts[idx].username);
        strcpy(current_user_name, accounts[idx].username);
        terminal_writeln("welcome.");
        return true;
    }
    terminal_writeln("invalid credentials.");
    return false;
}

static void handle_command(char *line, const char *current_user, bool *logout_requested) {
    while (*line == ' ') line++;
    if (*line == 0) return;
    char *cmd = line;
    char *arg1 = 0;
    char *arg2 = 0;
    for (char *p = line; *p; p++) {
        if (*p == ' ') { *p = 0; arg1 = p + 1; break; }
    }
    if (arg1) {
        while (*arg1 == ' ') arg1++;
        for (char *p = arg1; *p; p++) {
            if (*p == ' ') { *p = 0; arg2 = p + 1; break; }
        }
        if (arg2) while (*arg2 == ' ') arg2++;
    }

    size_t n = 0;
    const command_t *const *cmds = commands_get_list(&n);
    for (size_t i = 0; i < n; i++) {
        if (strcmp(cmd, cmds[i]->name) == 0) {
            if (cmds[i]->handler) cmds[i]->handler(arg1, arg2);
            return;
        }
    }

    if (strcmp(cmd, "logout") == 0) {
        *logout_requested = true;
    } else if (strcmp(cmd, "halt") == 0) {
        terminal_writeln("halting.");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    } else {
        terminal_writeln("unknown command.");
    }
}

static void shell_loop(const char *user) {
    char line[128];
    bool logout = false;
    while (!logout) {
        char prompt[64];
        size_t idx = 0;
        for (const char *p = user; *p && idx < sizeof(prompt) - 1; p++) prompt[idx++] = *p;
        if (idx < sizeof(prompt) - 1) prompt[idx++] = ' ';
        const char tag[] = "@ guyOS> ";
        for (size_t i = 0; i < sizeof(tag) - 1 && idx < sizeof(prompt) - 1; i++) prompt[idx++] = tag[i];
        prompt[idx] = 0;

        terminal_setcolor(COLOR_ACCENT);
        terminal_write(user);
        terminal_setcolor(COLOR_BODY);
        terminal_write(" @ guyOS> ");
        read_line(line, sizeof(line), true, prompt, user, "Shell: commands: help, clear (F5), whoami, users, adduser <u> <p>, logout, halt");
        handle_command(line, user, &logout);
    }
}

void shell_start(void) {
    debug_serial('a');  // Entered shell_start

    fat_init(FAT_PART_LBA);
    
    debug_serial('b');  // About to load accounts
    shell_load_accounts();
    debug_serial('c');  // Loaded accounts
    
    for (;;) {
        debug_serial('d');  // Top of main loop
        
        if (account_count == 0) {
            debug_serial('e');  // No accounts, setup mode
            draw_chrome_with_footer("<setup>", "Setup: create first user | F1 help | F5 clear");
            debug_serial('f');  // Drew chrome
            
            terminal_writeln("No accounts found. Create an admin user.");
            debug_serial('g');  // Wrote message
            
            char nu[32], np[32];
            terminal_write("new user> ");
            debug_serial('h');  // About to read line
            read_line(nu, sizeof(nu), true, "new user> ", "<setup>", "Setup: pick a username; F5 clears.");
            debug_serial('i');  // Read username
            
            terminal_write("new pass> ");
            read_line(np, sizeof(np), false, "new pass> ", "<setup>", "Setup: enter password; F5 clears.");
            debug_serial('j');  // Read password
            
            if (add_user(nu, np)) {
                terminal_writeln("Account created.");
            } else {
                terminal_writeln("Failed to create account (maybe full?).");
            }
            debug_serial('k');  // User added
        }
        
        debug_serial('l');  // About to draw login chrome
        draw_chrome_with_footer("<login>", "Login: F1 help | F5 clear | halt to stop");
        debug_serial('m');  // Drew login chrome
        
        char user[32];
        debug_serial('n');  // About to login
        while (!login(user)) {
            terminal_write("> ");
        }
        debug_serial('o');  // Logged in
        
        draw_chrome(user);
        terminal_writeln("Welcome to guyOS kernel shell.");
        terminal_writeln("Type help to explore. (Accounts are in-memory for now.)");
        debug_serial('p');  // About to enter shell loop
        shell_loop(user);
        debug_serial('q');  // Exited shell loop (logout)
    }
}
