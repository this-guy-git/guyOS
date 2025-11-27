# Shell Internals

## Overview
The shell runs entirely in kernel space and renders a text UI via VGA. It presents a prompt `user @ guyOS/<cwd> >` and dispatches built-in commands.

## Prompt & Title
- Prompt built in `shell_loop` (see `shell.c`).
- Title text is `shell_title` (default `guyOS kernel shell`). Interactive apps (e.g., `tedit`) can temporarily change it.

## Input Handling
- `keyboard_getch` in `shell.c` reads scancodes, handles Shift/Ctrl, and returns ASCII or extended codes:
  - `KEY_EXT_UP/DOWN/LEFT/RIGHT/HOME/END/DELETE`
  - `KEY_F1`, `KEY_F5`, `'\n'`, `'\b'`
- `shell_getch` exposes raw key reads to commands/apps.
- `read_line` provides a minimal line editor for the prompt (no history by default).

## Output
- VGA text functions in `shell.c` (`terminal_write`, `terminal_putc`, `terminal_redraw`).
- Public API: `shell_write`, `shell_write_line`, `shell_redraw`, `shell_cursor_backspace`.

## Commands
- Registered in `kernel/commands.c` as an array of `command_t`.
- Each command lives under `kernel/cmd/cmd_<name>.c` and exports `CMD_<NAME>`.
- Command availability is gated by stub files in `/shell/cmd` on the FAT partition.

## Users & Accounts
- Accounts stored as `/usr/accounts.bin` (see `shell_save_accounts`/`shell_load_accounts`).
- Home directory: `/usr/<user>`, set on login.
- User helper commands: `adduser`, `users`, `whoami`, `mkuserdir`, `fixuserdirs`.

## Filesystem Access
- `shell_current_dir_cluster()` returns the cwd cluster.
- `shell_chdir` uses `fat_resolve_path` and updates `cwd`/`cwd_cluster`.
- FAT operations provided by `kernel/fat.c` (read/write, mkdir, list, ensure).

## Apps (e.g., tedit)
- Interactive apps can:
  - Set `shell_title` while running, restore on exit.
  - Use `shell_getch` for raw input.
  - Use `shell_write`/`shell_redraw` to paint their UI.

## Extending the Shell
- Add a new command file under `kernel/cmd/` exporting `CMD_FOO`.
- Declare `extern const command_t CMD_FOO;` and add to `command_table` in `kernel/commands.c`.
- Ensure a stub exists under `/shell/cmd/foo` on the FAT partition.
