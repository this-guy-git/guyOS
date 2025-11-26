#pragma once
#include <stdbool.h>
#include <stddef.h>

void shell_write(const char *s);
void shell_write_line(const char *s);
void shell_redraw(void);
const char *shell_current_user(void);
void shell_list_users(void);
bool shell_add_user(const char *name, const char *pass);
bool shell_save_accounts(void);
bool shell_load_accounts(void);
#include <stdbool.h>
