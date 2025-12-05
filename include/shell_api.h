#ifndef SHELL_API_H
#define SHELL_API_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Output functions
void shell_write(const char *s);
void shell_write_line(const char *s);
void shell_redraw(void);

// Ensure user dirs (public wrapper)
void shell_ensure_user_dirs(const char *username);

// User management
bool shell_add_user(const char *name, const char *pass);
void shell_list_users(void);
const char *shell_current_user(void);
bool shell_save_accounts(void);
bool shell_load_accounts(void);

// Directory management
bool shell_chdir(const char *path);
void shell_set_cwd(const char *path);
const char *shell_get_cwd(void);
uint32_t shell_current_dir_cluster(void);
// Raw key input (returns ASCII or control codes; see shell keyboard map)
int shell_getch(void);
// Prompt helpers (exposed so apps/commands can reuse shell line input)
size_t shell_read_line(char *buf, size_t max, bool echo, const char *prompt, const char *help_msg);
size_t shell_prompt(char *buf, size_t max, const char *prompt);
// Current shell title (e.g., changes when an app like tedit is active)
extern const char *shell_title;
void shell_set_title(const char *title);
const char *shell_get_title(void);
// Move terminal cursor back one character (for rendering overlays)
void shell_cursor_backspace(void);
// Set terminal color (VGA attribute) and reset to default body color
void shell_set_color(uint8_t color);
void shell_reset_color(void);
// Screen helpers (rows/cols/body area) and clear body
void shell_get_dimensions(size_t *rows, size_t *cols, size_t *body_top, size_t *body_bottom);
void shell_clear_body(void);
// System info getters
const char *shell_cpu_brand(void);
const char *shell_cpu_vendor(void);
uint64_t shell_mem_total_bytes(void); // may be 0 if unknown
// Alias helpers
void shell_alias_load(void);
typedef struct {
    char alias[16];
    char real[16];
} shell_alias_t;
const shell_alias_t *shell_aliases(size_t *count);

#endif
