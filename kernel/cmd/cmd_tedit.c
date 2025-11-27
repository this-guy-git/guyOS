#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "path_utils.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Minimal nano-like fullscreen editor.
// Controls: Ctrl+S save, Ctrl+Q quit (warn if dirty), arrows/home/end move,
// Backspace/Delete edit, Enter newline. No search or multi-buffer.

#define CTRL(x) ((x) & 0x1F)
#define KEY_UP    0x101
#define KEY_DOWN  0x102
#define KEY_LEFT  0x103
#define KEY_RIGHT 0x104
#define KEY_HOME  0x105
#define KEY_END   0x106
#define KEY_DEL   0x107

#define VIEW_ROWS 17  // leave one extra row under title/status for spacing
#define MAX_BUF   8192

static char buffer[MAX_BUF];
static size_t len = 0;
static size_t cursor = 0;
static size_t scroll_row = 0;
static uint32_t file_dir = 0;
static const char *file_leaf = 0;
static char file_name[64];
static bool dirty = false;

static void clamp_cursor(void) {
    if (cursor > len) cursor = len;
}

static void pos_to_rc(size_t pos, size_t *row, size_t *col) {
    size_t r = 0, c = 0;
    for (size_t i = 0; i < pos && i < len; i++) {
        if (buffer[i] == '\n') { r++; c = 0; }
        else c++;
    }
    if (row) *row = r;
    if (col) *col = c;
}

static size_t rc_to_pos(size_t target_row, size_t target_col) {
    size_t r = 0, c = 0;
    for (size_t i = 0; i <= len; i++) {
        if (r == target_row && c == target_col) return i;
        if (i >= len) return len;
        if (buffer[i] == '\n') { r++; c = 0; if (r > target_row) return i + 1; }
        else {
            c++;
            if (r == target_row && c > target_col) return i + 1;
        }
    }
    return len;
}

static size_t total_lines(void) {
    size_t lines = 1;
    for (size_t i = 0; i < len; i++) if (buffer[i] == '\n') lines++;
    return lines;
}

static size_t line_length(size_t row) {
    size_t r = 0, pos = 0;
    while (r < row && pos < len) {
        if (buffer[pos++] == '\n') r++;
    }
    size_t count = 0;
    while (pos + count < len && buffer[pos + count] != '\n') count++;
    return count;
}

static void render(const char *msg) {
    size_t cur_row, cur_col;
    pos_to_rc(cursor, &cur_row, &cur_col);
    const bool cursor_on = true; // keep cursor always visible since we lack a timer for real blinking

    // keep cursor in view
    if (cur_row < scroll_row) scroll_row = cur_row;
    if (cur_row >= scroll_row + VIEW_ROWS) scroll_row = cur_row - VIEW_ROWS + 1;

    shell_redraw();
    // Title
    shell_write("  GNU nano     File: ");
    shell_write(file_name);
    if (dirty) shell_write(" [Modified]");
    shell_write_line("");
    shell_write_line("");

    // Lines (viewport height)
    size_t pos = rc_to_pos(scroll_row, 0);
    size_t total = total_lines();
    for (size_t screen = 0; screen < VIEW_ROWS; screen++) {
        size_t buf_row = scroll_row + screen;
        if (buf_row >= total && pos >= len) {
            shell_write_line("~");
            continue;
        }
        size_t line_start = pos;
        while (pos < len && buffer[pos] != '\n') pos++;
        size_t line_end = pos;
        // line number padding to 3 chars
        char numbuf[8]; int nd=0; size_t ln = buf_row + 1;
        if (ln==0) numbuf[nd++]='0'; else { while (ln>0 && nd<7) { numbuf[nd++]='0'+(ln%10); ln/=10; } }
        while (nd<3) numbuf[nd++]=' ';
        while (nd>0) { char s[2]={numbuf[--nd],0}; shell_write(s); }
        shell_write(" ");

        for (size_t i = line_start; i < line_end; i++) {
            if (i == cursor && cursor_on) shell_write("_");
            char s[2] = { buffer[i], 0 };
            shell_write(s);
        }
        if (cursor == line_end && cursor_on) shell_write("_");
        shell_write_line("");
        if (pos < len && buffer[pos] == '\n') pos++; // skip newline
    }
    // Separator and status at bottom
    shell_write_line("");
    if (msg) shell_write_line(msg); else shell_write_line("");
    shell_write_line("^S Save   ^Q Quit");
    shell_write("Line ");
    size_t num = cur_row + 1; char digits[20]; int d = 0; if (num == 0) digits[d++]='0'; else { while (num>0){digits[d++]='0'+(num%10); num/=10;} }
    while (d--) { char s[2]={digits[d],0}; shell_write(s); }
    shell_write("  Col ");
    num = cur_col + 1; d=0; if (num==0) digits[d++]='0'; else { while(num>0){digits[d++]='0'+(num%10); num/=10;} }
    while (d--) { char s[2]={digits[d],0}; shell_write(s); }
    shell_write_line("");
}

static void insert_char(char c) {
    if (len + 1 >= sizeof(buffer)) return;
    for (size_t i = len; i > cursor; i--) buffer[i] = buffer[i - 1];
    buffer[cursor++] = c;
    len++;
    buffer[len] = 0;
    dirty = true;
}

static void delete_at_cursor(void) {
    if (cursor >= len) return;
    for (size_t i = cursor; i < len; i++) buffer[i] = buffer[i + 1];
    len--;
    buffer[len] = 0;
    dirty = true;
}

static void backspace_char(void) {
    if (cursor == 0) return;
    cursor--;
    delete_at_cursor();
}

static void load_file(void) {
    size_t bytes = 0;
    if (fat_read_file_at(file_dir, file_leaf, (uint8_t *)buffer, sizeof(buffer) - 1, &bytes)) {
        len = bytes;
        if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
        buffer[len] = 0;
    } else {
        len = 0;
        buffer[0] = 0;
    }
    cursor = 0;
    scroll_row = 0;
    dirty = false;
}

static void save_file(const char *fname) {
    if (fat_write_file_at(file_dir, file_leaf, (const uint8_t *)buffer, len)) {
        dirty = false;
        render("Saved");
    } else {
        render("Save failed");
    }
}

static void tedit_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    if (!arg1 || arg1[0] == 0) {
        shell_write_line("edit: missing file operand");
        shell_write_line("Usage: edit FILE");
        return;
    }
    // Switch title to app name while editing
    extern const char *shell_title;
    const char *prev_title = shell_title;
    shell_title = "tedit";

    if (!split_path_parent(arg1, shell_current_dir_cluster(), &file_dir, &file_leaf)) {
        shell_write("edit: invalid path '"); shell_write(arg1); shell_write_line("'");
        shell_title = prev_title;
        return;
    }
    // copy name for display
    size_t n = 0; while (arg1[n] && n + 1 < sizeof(file_name)) { file_name[n] = arg1[n]; n++; } file_name[n] = 0;

    load_file();
    render(NULL);

    bool quit_warn = false;
    for (;;) {
        int key = shell_getch();
        if (key == 0) continue;

        if (key == CTRL('Q')) {
            if (dirty && !quit_warn) { render("Unsaved changes! Ctrl+Q again to quit"); quit_warn = true; continue; }
            shell_title = prev_title;
            shell_redraw(); // refresh header when leaving app
            return;
        }
        quit_warn = false;

        if (key == CTRL('S')) { save_file(file_name); continue; }

        if (key == KEY_UP) {
            size_t row, col; pos_to_rc(cursor, &row, &col);
            if (row > 0) { size_t new_row = row - 1; size_t ll = line_length(new_row); if (col > ll) col = ll; cursor = rc_to_pos(new_row, col); }
            render(NULL); continue;
        }
        if (key == KEY_DOWN) {
            size_t row, col; pos_to_rc(cursor, &row, &col);
            size_t tl = total_lines();
            if (row + 1 < tl) { size_t new_row = row + 1; size_t ll = line_length(new_row); if (col > ll) col = ll; cursor = rc_to_pos(new_row, col); }
            render(NULL); continue;
        }
        if (key == KEY_LEFT) { if (cursor > 0) cursor--; render(NULL); continue; }
        if (key == KEY_RIGHT) { if (cursor < len) cursor++; render(NULL); continue; }
        if (key == KEY_HOME || key == CTRL('A')) { size_t row,col; pos_to_rc(cursor,&row,&col); cursor = rc_to_pos(row,0); render(NULL); continue; }
        if (key == KEY_END || key == CTRL('E')) { size_t row,col; pos_to_rc(cursor,&row,&col); cursor = rc_to_pos(row, line_length(row)); render(NULL); continue; }

        if (key == KEY_DEL || key == CTRL('D')) { delete_at_cursor(); render(NULL); continue; }
        if (key == '\b' || key == 127 || key == CTRL('H')) { backspace_char(); render(NULL); continue; }
        if (key == '\n' || key == '\r') { insert_char('\n'); render(NULL); continue; }

        if (key >= 32 && key < 127) { insert_char((char)key); render(NULL); continue; }
        // ignore others
    }
    shell_title = prev_title;
}

const command_t CMD_TEDIT = {
    .name = "tedit",
    .help = "tedit: tiny editor (Ctrl+S save, Ctrl+Q quit)",
    .handler = tedit_handler
};
