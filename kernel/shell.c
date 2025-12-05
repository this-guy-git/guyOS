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

// Forward declaration for current user (defined with init below)
static char current_user_name[32];

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

// helper to write to COM1 for capture; same format as in fat.c
#include "../include/hal.h"
static void serial_debug(const char *s) {
    if (!s) return;
    for (const char *p = s; *p; p++) outb(0x3F8, (uint8_t)*p);
}

static void terminal_write_uint32(uint32_t v) {
    char buf[12];
    int idx = 0;
    if (v == 0) { terminal_write("0"); return; }
    uint32_t t = v;
    char tmp[12];
    int ti = 0;
    while (t > 0 && ti < (int)sizeof(tmp)) { tmp[ti++] = '0' + (t % 10); t /= 10; }
    while (ti > 0) buf[idx++] = tmp[--ti];
    buf[idx] = 0;
    terminal_write(buf);
}

void shell_write(const char *s) { terminal_write(s); }
void shell_write_line(const char *s) { terminal_writeln(s); }
void shell_cursor_backspace(void) { terminal_backspace(); }
void shell_set_color(uint8_t color) { terminal_setcolor(color); }
void shell_reset_color(void) { terminal_setcolor(COLOR_BODY); }
void shell_get_dimensions(size_t *rows, size_t *cols, size_t *body_top, size_t *body_bottom) {
    if (rows) *rows = VGA_HEIGHT;
    if (cols) *cols = VGA_WIDTH;
    if (body_top) *body_top = BODY_TOP;
    if (body_bottom) *body_bottom = BODY_BOTTOM;
}
void shell_clear_body(void) { terminal_clear_body(); }

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

const char *shell_title = "gsh";
void shell_set_title(const char *title) { shell_title = title ? title : "gsh"; }
const char *shell_get_title(void) { return shell_title; }

static void draw_chrome_with_footer(const char *user, const char *footer_left) {
    const char *title = shell_title ? shell_title : "gsh";
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
static bool ctrl_down = false;

// Extended key codes we return for navigation
enum {
    KEY_EXT_BASE = 0x100,
    KEY_EXT_UP = KEY_EXT_BASE + 1,
    KEY_EXT_DOWN,
    KEY_EXT_LEFT,
    KEY_EXT_RIGHT,
    KEY_EXT_HOME,
    KEY_EXT_END,
    KEY_EXT_DELETE
};

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
        if (sc == 0x1D) { ctrl_down = true; continue; }
        if (sc == 0x9D) { ctrl_down = false; continue; }

        // Extended keys (arrows, home/end, delete)
        if (sc == 0xE0) {
            while ((inb(0x64) & 1) == 0) io_wait();
            sc = inb(0x60);
            if (sc == 0x48) return KEY_EXT_UP;
            if (sc == 0x50) return KEY_EXT_DOWN;
            if (sc == 0x4B) return KEY_EXT_LEFT;
            if (sc == 0x4D) return KEY_EXT_RIGHT;
            if (sc == 0x47) return KEY_EXT_HOME;
            if (sc == 0x4F) return KEY_EXT_END;
            if (sc == 0x53) return KEY_EXT_DELETE;
            continue;
        }
        if (sc & 0x80) continue; // key release
        if (sc == 0x3B) return KEY_F1;
        if (sc == 0x3F) return KEY_F5;
        if (sc == 0x1C) return '\n';
        if (sc == 0x0E) return '\b';
        char c = shift_down ? keymap_shift[sc] : keymap[sc];
        if (ctrl_down && c >= 1 && c <= 127) c = c & 0x1F;
        if (c) return c;
    }
}

int shell_getch(void) {
    return keyboard_getch();
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
size_t shell_read_line(char *buf, size_t max, bool echo, const char *prompt, const char *help_msg) {
    return read_line(buf, max, echo, prompt, current_user_name, help_msg);
}
size_t shell_prompt(char *buf, size_t max, const char *prompt) {
    return read_line(buf, max, true, prompt, current_user_name, 0);
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
static const char ACC_FILE_NAME[] = "accounts.bin";
static const uint32_t FAT_PART_LBA = 16384; // 8MB offset (must match Makefile)
// Use 8.3-safe subdirectory names to avoid truncation issues on our short-name-only FAT.
static const char USER_SUBDIR_DOWNLOADS[] = "download"; // maps from "downloads" input
static const char USER_SUBDIR_DOCUMENTS[] = "document"; // maps from "documents" input
static char cwd[128] = "/";
static uint32_t cwd_cluster = 0;
static uint32_t shell_cmd_cluster = 0;
static char home_path[64] = "/";
static char cpu_vendor[13] = "unknown";
static char cpu_brand[49] = "unknown";
static uint64_t mem_total = 0;

const char *shell_cpu_brand(void) { return cpu_brand; }
const char *shell_cpu_vendor(void) { return cpu_vendor; }
uint64_t shell_mem_total_bytes(void) { return mem_total; }

static void detect_cpu(void) {
    uint32_t eax, ebx, ecx, edx;
    eax = 0; ecx = 0;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax), "c"(ecx));
    ((uint32_t*)cpu_vendor)[0] = ebx;
    ((uint32_t*)cpu_vendor)[1] = edx;
    ((uint32_t*)cpu_vendor)[2] = ecx;
    cpu_vendor[12] = 0;
    eax = 0x80000000; ecx = 0;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax), "c"(ecx));
    if (eax >= 0x80000004) {
        uint32_t *p = (uint32_t *)cpu_brand;
        for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
            __asm__ volatile("cpuid" : "=a"(p[0]), "=b"(p[1]), "=c"(p[2]), "=d"(p[3]) : "a"(leaf));
            p += 4;
        }
        cpu_brand[48] = 0;
    }
}

static uint16_t cmos_read16(uint8_t reg) {
    outb(0x70, reg);
    uint8_t lo = inb(0x71);
    outb(0x70, reg + 1);
    uint8_t hi = inb(0x71);
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

static void detect_mem(void) {
    // Approximate total memory using CMOS registers. Good enough for fetch output.
    uint16_t ext_1m_16m_kb = cmos_read16(0x15);   // KB between 1MB-16MB
    uint16_t ext_above_16m_kb = cmos_read16(0x17); // KB above 16MB (legacy)
    uint16_t ext_above_16m_64k = cmos_read16(0x34); // 64KB blocks above 16MB (newer)

    uint64_t total = 1024 * 1024; // base 1MB
    if (ext_above_16m_64k) {
        total = 16ULL * 1024 * 1024 + (uint64_t)ext_above_16m_64k * 64ULL * 1024ULL;
    } else if (ext_above_16m_kb) {
        total = 16ULL * 1024 * 1024 + (uint64_t)ext_above_16m_kb * 1024ULL;
    } else if (ext_1m_16m_kb) {
        total += (uint64_t)ext_1m_16m_kb * 1024ULL;
    } else {
        total = 0; // unknown
    }
    mem_total = total;
}

// Show MOTD if /sys/motd exists
static void show_motd(void) {
    uint32_t sys = 0; bool is_dir = false;
    if (!fat_resolve_path(fat_get_root_cluster(), "/sys", &sys, &is_dir) || !is_dir) return;
    uint8_t buf[1024];
    size_t got = 0;
    if (!fat_read_file_at(sys, "motd", buf, sizeof(buf) - 1, &got)) return;
    buf[got] = 0;
    shell_write_line((const char *)buf);
}

static void build_path(const char *a, const char *b, const char *c, char *out, size_t out_sz) {
    size_t w = 0;
    const char *parts[3] = {a, b, c};
    for (int i = 0; i < 3; i++) {
        const char *p = parts[i];
        if (!p) continue;
        while (*p && w + 1 < out_sz) out[w++] = *p++;
    }
    out[w] = 0;
}

static const char *display_path(const char *cwd_path) {
    // Show "~" when cwd is at or inside home (e.g., ~/documents).
    static char buf[128];
    if (!cwd_path) return "/";
    if (home_path[0]) {
        size_t i = 0;
        while (home_path[i] && cwd_path[i] && home_path[i] == cwd_path[i]) i++;
        if (home_path[i] == 0 && (cwd_path[i] == 0 || cwd_path[i] == '/')) {
            size_t w = 0;
            buf[w++] = '/~';
            while (cwd_path[i] && w + 1 < sizeof(buf)) buf[w++] = cwd_path[i++];
            buf[w] = 0;
            return buf;
        }
    }
    return cwd_path;
}

static uint32_t ensure_dir_resolved(uint32_t parent_cluster, const char *name, const char *abs_path_hint) {
    uint32_t cl = fat_ensure_dir_at(parent_cluster, name);
    bool is_dir = false;
    uint32_t resolved = 0;
    if (cl != 0) {
        if (abs_path_hint && fat_resolve_path(fat_get_root_cluster(), abs_path_hint, &resolved, &is_dir) && is_dir) return resolved;
        if (fat_resolve_path(parent_cluster, name, &resolved, &is_dir) && is_dir) return resolved;
        return cl;
    }
    fat_mkdir_at(parent_cluster, name, &cl);
    if (cl != 0) {
        if (abs_path_hint && fat_resolve_path(fat_get_root_cluster(), abs_path_hint, &resolved, &is_dir) && is_dir) return resolved;
        if (fat_resolve_path(parent_cluster, name, &resolved, &is_dir) && is_dir) return resolved;
        return cl;
    }
    if (abs_path_hint && fat_resolve_path(fat_get_root_cluster(), abs_path_hint, &resolved, &is_dir) && is_dir) return resolved;
    return 0;
}

static void xor_buffer(uint8_t *buf, size_t len, uint8_t key) {
    for (size_t i = 0; i < len; i++) buf[i] ^= key;
}

static uint32_t ensure_usr_dir(void) {
    uint32_t usr_cluster = 0;
    bool is_dir = false;
    if (!fat_resolve_path(fat_get_root_cluster(), "/usr", &usr_cluster, &is_dir) || !is_dir) {
        usr_cluster = ensure_dir_resolved(fat_get_root_cluster(), "usr", "/usr");
        if (usr_cluster == 0) return 0;
    }
    return usr_cluster;
}

static const char *builtin_cmds[] = {
    "help","clear","whoami","users","adduser","echo","time","ls","mkdir",
    "mkuserdir","cd","pwd","touch","cat","fstest","fixuserdirs","version","reboot"
};

static void command_fs_name(const char *cmd, char *out, size_t out_sz) {
    // Map long names to 8.3-safe filenames to align with FAT short-name storage.
    const struct { const char *longn; const char *shortn; } aliases[] = {
        { "mkuserdir",  "mkuserdi" },
        { "fixuserdirs","fixuserd" },
    };
    for (size_t i = 0; i < sizeof(aliases)/sizeof(aliases[0]); i++) {
        if (strcmp(cmd, aliases[i].longn) == 0) {
            size_t j = 0; while (aliases[i].shortn[j] && j + 1 < out_sz) { out[j] = aliases[i].shortn[j]; j++; }
            out[j] = 0;
            return;
        }
    }
    size_t i = 0;
    while (cmd[i] && i < 8 && i + 1 < out_sz) {
        out[i] = cmd[i];
        i++;
    }
    out[i] = 0;
}

static void ensure_shell_cmd_dir(void) {
    uint32_t shell_cluster = fat_ensure_dir_at(fat_get_root_cluster(), "shell");
    if (shell_cluster == 0) {
        bool is_dir = false;
        fat_resolve_path(fat_get_root_cluster(), "/shell", &shell_cluster, &is_dir);
    }
    if (shell_cluster == 0) return;
    shell_cmd_cluster = fat_ensure_dir_at(shell_cluster, "cmd");
    if (shell_cmd_cluster == 0) {
        bool is_dir = false;
        fat_resolve_path(fat_get_root_cluster(), "/shell/cmd", &shell_cmd_cluster, &is_dir);
    }
    if (shell_cmd_cluster == 0) return;

    // Ensure each builtin has a stub file so runtime command availability matches filesystem.
    const char *stub = "builtin command stub\n";
    for (size_t i = 0; i < sizeof(builtin_cmds)/sizeof(builtin_cmds[0]); i++) {
        const char *name = builtin_cmds[i];
        char fsname[16];
        command_fs_name(name, fsname, sizeof(fsname));
        uint32_t tmp = 0; bool is_dir = false;
        char path[64]; int w = 0;
        const char *prefix = "/shell/cmd/";
        while (*prefix && w + 1 < (int)sizeof(path)) path[w++] = *prefix++;
        for (const char *p = fsname; *p && w + 1 < (int)sizeof(path); p++) path[w++] = *p;
        path[w] = 0;
        if (fat_resolve_path(fat_get_root_cluster(), path, &tmp, &is_dir) && !is_dir) continue;
        fat_write_file_at(shell_cmd_cluster, fsname, (const uint8_t *)stub, 0);
    }
}

static void ensure_user_dirs(const char *username) {
    uint32_t usr_cluster = ensure_usr_dir();
    if (usr_cluster == 0) {
        terminal_write("[debug] ensure_user_dirs: /usr missing and failed to create\n");
        serial_debug("[debug] ensure_user_dirs: /usr missing and failed to create\n");
        return;
    }
    // Ensure /usr/<username> exists and get its cluster directly
    uint32_t user_cluster = 0;
    char user_path[64];
    build_path("/usr/", username, 0, user_path, sizeof(user_path));
    user_cluster = ensure_dir_resolved(usr_cluster, username, user_path);
    if (user_cluster == 0) {
        terminal_write("[debug] ensure_user_dirs: failed to ensure /usr/"); terminal_write(username); terminal_write("\n");
        serial_debug("[debug] ensure_user_dirs: failed to ensure /usr/"); serial_debug(username); serial_debug("\n");
        return;
    }

    char downloads_path[80];
    build_path("/usr/", username, "/downloads", downloads_path, sizeof(downloads_path));
    uint32_t dl = ensure_dir_resolved(user_cluster, USER_SUBDIR_DOWNLOADS, downloads_path);
    if (dl == 0) dl = ensure_dir_resolved(user_cluster, USER_SUBDIR_DOWNLOADS, downloads_path); // retry short name
    if (dl == 0) dl = ensure_dir_resolved(user_cluster, "downloads", downloads_path); // plural fallback

    char documents_path[80];
    build_path("/usr/", username, "/documents", documents_path, sizeof(documents_path));
    uint32_t doc = ensure_dir_resolved(user_cluster, USER_SUBDIR_DOCUMENTS, documents_path);
    if (doc == 0) doc = ensure_dir_resolved(user_cluster, USER_SUBDIR_DOCUMENTS, documents_path); // retry short name
    if (doc == 0) doc = ensure_dir_resolved(user_cluster, "documents", documents_path); // plural fallback

    bool d1 = dl != 0;
    bool d2 = doc != 0;
    terminal_write("[debug] ensure_user_dirs: /usr/"); terminal_write(username);
    terminal_write(" -> "); terminal_write(USER_SUBDIR_DOWNLOADS); terminal_write(" "); terminal_write(d1 ? "ok " : "fail ");
    terminal_write(USER_SUBDIR_DOCUMENTS); terminal_write(" "); terminal_write(d2 ? "ok\n" : "fail\n");
    serial_debug("[debug] ensure_user_dirs: ensured subdirs for /usr/");
    serial_debug(username);
    serial_debug(" "); serial_debug(USER_SUBDIR_DOWNLOADS); serial_debug(":");
    serial_debug(d1 ? "ok" : "fail");
    serial_debug(" "); serial_debug(USER_SUBDIR_DOCUMENTS); serial_debug(":");
    serial_debug(d2 ? "ok" : "fail");
    serial_debug("\n");
}

// Public wrapper so other modules (commands) can ensure user dirs exist
void shell_ensure_user_dirs(const char *username) {
    ensure_user_dirs(username);
}

static void build_cwd_path(const char *current, const char *path, char *out, size_t out_sz) {
    // Build a normalized path string (no duplicate slashes, handles . and ..).
    const bool absolute = path && path[0] == '/';
    char parts[16][32];
    int part_count = 0;
    for (int i = 0; i < 16; i++) parts[i][0] = 0;

    // Seed with current path components if relative
    if (!absolute && current && current[0]) {
        const char *p = current;
        if (*p == '/') p++;
        while (*p) {
            size_t len = 0;
            while (p[len] && p[len] != '/') len++;
            if (len > 0 && part_count < 16) {
                size_t copy = len < sizeof(parts[0]) - 1 ? len : sizeof(parts[0]) - 1;
                for (size_t i = 0; i < copy; i++) parts[part_count][i] = p[i];
                parts[part_count][copy] = 0;
                part_count++;
            }
            p += len;
            if (*p == '/') p++;
        }
    }

    const char *p = path;
    if (p && *p == '/') p++;
    while (p && *p) {
        size_t len = 0;
        while (p[len] && p[len] != '/') len++;
        if (len == 1 && p[0] == '.') {
            // ignore
        } else if (len == 2 && p[0] == '.' && p[1] == '.') {
            if (part_count > 0) part_count--;
        } else if (len > 0 && part_count < 16) {
            size_t copy = len < sizeof(parts[0]) - 1 ? len : sizeof(parts[0]) - 1;
            for (size_t i = 0; i < copy; i++) parts[part_count][i] = p[i];
            parts[part_count][copy] = 0;
            part_count++;
        }
        p += len;
        if (*p == '/') p++;
    }

    size_t pos = 0;
    if (pos < out_sz) out[pos++] = '/';
    for (int i = 0; i < part_count && pos < out_sz - 1; i++) {
        size_t l = strlen(parts[i]);
        if (pos + l >= out_sz - 1) l = out_sz - 1 - pos;
        for (size_t j = 0; j < l; j++) out[pos++] = parts[i][j];
        if (i != part_count - 1 && pos < out_sz - 1) out[pos++] = '/';
    }
    out[pos] = 0;
}

bool shell_save_accounts(void) {
    uint8_t buf[512];
    for (size_t i = 0; i < sizeof(buf); i++) buf[i] = 0;
    buf[0] = (uint8_t)account_count;
    size_t offset = 1;
    for (size_t i = 0; i < account_count && offset + 32 <= 512; i++) {
        for (size_t j = 0; j < 16 && offset < 512; j++) buf[offset++] = (uint8_t)accounts[i].username[j];
        for (size_t j = 0; j < 16 && offset < 512; j++) buf[offset++] = (uint8_t)accounts[i].password[j];
    }
    xor_buffer(buf, sizeof(buf), 0xA5); // simple XOR obfuscation

    uint32_t usr_cluster = 0;
    for (int attempt = 0; attempt < 3 && usr_cluster == 0; attempt++) {
        usr_cluster = ensure_usr_dir();
    }
    if (usr_cluster == 0) {
        shell_write_line("[debug] shell_save_accounts: failed to ensure /usr");
        return false;
    }

    // Ensure per-user directories (/usr/<user>/download, document)
    for (size_t i = 0; i < account_count; i++) ensure_user_dirs(accounts[i].username);

    for (int attempt = 0; attempt < 3; attempt++) {
        if (fat_write_file_at(usr_cluster, ACC_FILE_NAME, buf, sizeof(buf))) return true;
    }
    shell_write_line("[debug] shell_save_accounts: failed to write accounts.bin after retries");
    return false;
}

bool shell_load_accounts(void) {
    uint8_t buf[512];
    size_t got = 0;
    uint32_t usr_cluster = 0;
    bool is_dir = false;
    if (!fat_resolve_path(fat_get_root_cluster(), "/usr", &usr_cluster, &is_dir) || !is_dir) {
        account_count = 0;
        return false;
    }

    if (!fat_read_file_at(usr_cluster, ACC_FILE_NAME, buf, sizeof(buf), &got)) {
        account_count = 0;
        return false;
    }
    xor_buffer(buf, sizeof(buf), 0xA5); // decrypt
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
        ensure_user_dirs(acc->username);
    }
    if (account_count > 0) strcpy(current_user_name, accounts[0].username);
    return true;
}

// CWD helpers (simple, no real path resolution)
bool shell_chdir(const char *path) {
    if (!path || !path[0]) return false;

    uint32_t start = (path[0] == '/') ? fat_get_root_cluster() : cwd_cluster;
    uint32_t cluster = 0;
    bool is_dir = false;
    if (!fat_resolve_path(start, path, &cluster, &is_dir) || !is_dir) return false;

    char new_cwd[sizeof(cwd)];
    build_cwd_path(cwd, path, new_cwd, sizeof(new_cwd));
    cwd_cluster = cluster;
    shell_set_cwd(new_cwd);
    return true;
}

void shell_set_cwd(const char *path) {
    size_t i = 0;
    if (!path) return;
    while (path[i] && i + 1 < sizeof(cwd)) {
        cwd[i] = path[i];
        i++;
    }
    cwd[i] = 0;
}

const char *shell_get_cwd(void) {
    return cwd;
}

uint32_t shell_current_dir_cluster(void) {
    return cwd_cluster;
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
    // Ensure per-user folders exist first (make sure filesystem entries are present)
    ensure_user_dirs(name);
    bool saved = shell_save_accounts();
    // Debug serial note
    if (saved) {
        serial_debug("[debug] add_user: accounts saved OK\n");
    } else {
        serial_debug("[debug] add_user: accounts failed to save\n");
    }
    // Debug: report creation attempt so we can observe effects at runtime
    terminal_write("[debug] add_user: ensure_user_dirs called for '"); terminal_write(name); terminal_write("'\n");
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

static bool command_file_present(const char *name) {
    if (shell_cmd_cluster == 0) ensure_shell_cmd_dir();
    if (shell_cmd_cluster == 0) return false;

    uint32_t cl = 0; bool is_dir = false;
    char path[64]; int w = 0;
    char fsname[16];
    command_fs_name(name, fsname, sizeof(fsname));
    // Resolve relative to shell_cmd_cluster to avoid failures if root lookup is stale.
    if (fat_resolve_path(shell_cmd_cluster, fsname, &cl, &is_dir)) return !is_dir;
    // As a fallback, try absolute path once (older images)
    const char *prefix = "/shell/cmd/";
    while (*prefix && w + 1 < (int)sizeof(path)) path[w++] = *prefix++;
    for (const char *p = fsname; *p && w + 1 < (int)sizeof(path); p++) path[w++] = *p;
    path[w] = 0;
    return fat_resolve_path(fat_get_root_cluster(), path, &cl, &is_dir) && !is_dir;
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
    // Resolve aliases: user types alias -> run real command
    char real_cmd[32];
    size_t rc = 0;
    while (cmd[rc] && rc + 1 < sizeof(real_cmd)) { real_cmd[rc] = cmd[rc]; rc++; }
    real_cmd[rc] = 0;
    size_t alias_count = 0;
    const shell_alias_t *als = shell_aliases(&alias_count);
    for (size_t i = 0; i < alias_count; i++) {
        if (strcmp(real_cmd, als[i].alias) == 0) {
            size_t j = 0;
            while (als[i].real[j] && j + 1 < sizeof(real_cmd)) { real_cmd[j] = als[i].real[j]; j++; }
            real_cmd[j] = 0;
            break;
        }
    }

    for (size_t i = 0; i < n; i++) {
        if (strcmp(real_cmd, cmds[i]->name) == 0) {
            bool file_missing = !command_file_present(cmds[i]->name);
            if (strcmp(real_cmd, cmd) == 0 && file_missing) {
                terminal_write("command missing in /shell/cmd/: ");
                terminal_writeln(cmd);
                // Run built-in anyway so shell stays usable.
            }
            if (cmds[i]->handler) cmds[i]->handler(arg1, arg2);
            // Reset title after any command returns; leave screen intact unless caller redraws
            shell_title = "gsh";
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
        const char tag[] = "@ guyOS";
        for (size_t i = 0; i < sizeof(tag) - 1 && idx < sizeof(prompt) - 1; i++) prompt[idx++] = tag[i];
        const char *cwd_str = display_path(shell_get_cwd());
        for (const char *p = cwd_str; p && *p && idx < sizeof(prompt) - 1; p++) prompt[idx++] = *p;
        if (idx < sizeof(prompt) - 3) {
            prompt[idx++] = ' ';
            prompt[idx++] = '>';
            prompt[idx++] = ' ';
        }
        prompt[idx] = 0;

        terminal_setcolor(COLOR_ACCENT);
        terminal_write(user);
        terminal_setcolor(COLOR_BODY);
        terminal_write(" @ guyOS");
        terminal_write(display_path(shell_get_cwd()));
        terminal_write("> ");
        read_line(line, sizeof(line), true, prompt, user, "Shell: commands: help, clear (F5), whoami, users, adduser <u> <p>, logout, halt");
        handle_command(line, user, &logout);
    }
}

void shell_start(void) {
    debug_serial('a');  // Entered shell_start

    fat_init(FAT_PART_LBA);
    cwd_cluster = fat_get_root_cluster();
    shell_set_cwd("/");
    ensure_shell_cmd_dir();
    shell_alias_load();
    detect_cpu();
    detect_mem();

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
        // Ensure user home exists and chdir into it
        ensure_user_dirs(user);
        char home[64];
        size_t idx = 0;
        const char *prefix = "/usr/";
        while (*prefix && idx + 1 < sizeof(home)) home[idx++] = *prefix++;
        for (const char *p = user; *p && idx + 1 < sizeof(home); p++) home[idx++] = *p;
        home[idx] = 0;
        shell_chdir(home);
        // Remember home path for prompt rendering
        size_t hi = 0; while (home[hi] && hi + 1 < sizeof(home_path)) { home_path[hi] = home[hi]; hi++; } home_path[hi] = 0;
        
        draw_chrome(user);
        terminal_writeln("Welcome to guyOS!");
        terminal_writeln("Type help to explore. Accounts are persisted to disk.");
        show_motd();
        debug_serial('p');  // About to enter shell loop
        shell_loop(user);
        debug_serial('q');  // Exited shell loop (logout)
    }
}
