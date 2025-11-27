# Commands Reference

Built-in commands available in the guyOS shell. Commands are registered in `kernel/commands.c` and implemented under `kernel/cmd/`.

## Core
- `help` — List available commands.
- `clear` — Clear the screen.
- `whoami` — Show current user.
- `users` — List users.
- `adduser <name> <pass>` — Create a new user.
- `logout` — Exit to login prompt.
- `halt` — Halt the system.

## Navigation & Files
- `pwd` — Print current working directory.
- `cd <path>` — Change directory.
- `ls [-v] [path]` — List directory contents (verbose with `-v`).
- `mkdir <dir>` — Create directory (respects cwd).
- `touch <file>` — Create empty file.
- `cat <file>` — Display file contents.

## User Directories
- `mkuserdir <user>` — Ensure `/usr/<user>` exists.
- `fixuserdirs <user>` — Repair/ensure user directories.

## System & Diagnostics
- `fstest` — FAT filesystem test.
- `time` — Stub (RTC not implemented).
- `version` — Show version.
- `reboot` — Reboot.

## Editor
- `tedit <file>` — Tiny editor (nano-like). Controls: `Ctrl+S` save, `Ctrl+Q` quit, arrows/Home/End move, Backspace/Delete, Enter newline.

## Notes
- Command availability is gated by stub files under `/shell/cmd/<name>` on the FAT partition.
- Paths are relative to cwd unless absolute (`/path`).
- FAT supports 8.3 and basic long-name handling; very long names may truncate.
