#ifndef SHELL_API_H
#define SHELL_API_H

#include <stdbool.h>
#include <stdint.h>

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
// Current shell title (e.g., changes when an app like tedit is active)
extern const char *shell_title;
// Move terminal cursor back one character (for rendering overlays)
void shell_cursor_backspace(void);
// Alias helpers
void shell_alias_load(void);
typedef struct {
    char alias[16];
    char real[16];
} shell_alias_t;
const shell_alias_t *shell_aliases(size_t *count);

#endif
