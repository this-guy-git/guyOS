# guyOS Overview

This document summarizes the major pieces of guyOS as shipped in this repo.

## Boot & Kernel
- **Bootloader**: two-stage loader (`boot/stage1.asm`, `boot/stage2.asm`) that loads the kernel image.
- **Kernel**: freestanding x86_64 kernel built from `kernel/` sources (`kernel.c`, `shell.c`, `fat.c`, `disk.c`, commands in `kernel/cmd/`).
- **Makefile**: builds the bootloader, kernel, FAT partition image, and full disk image; runs QEMU (`make run`).

## Shell
- Text-mode shell rendered via VGA; prompt shows `user @ guyOS/<cwd>`.
- Commands are built-ins defined in `kernel/cmd/` and registered in `kernel/commands.c`.
- Title bar text is stored in `shell_title`; interactive apps like `tedit` temporarily change it.

## Filesystem
- FAT32 partition at an 8 MB offset (see `Makefile` and `FAT_PART_LBA` in `shell.c`).
- FAT handling: `kernel/fat.c` supports directory traversal, file read/write, mkdir, and 8.3 + basic LFN.
- Directory layout: see `docs/filesystem.md`. Key paths: `/usr/<user>/downloads`, `/usr/<user>/documents`, `/shell/cmd`, `/vital`.
- Command availability is gated by stub files in `/shell/cmd` (e.g., `tedit`).

## Input/Output
- VGA text output in `shell.c` (`terminal_*` helpers).
- Keyboard input via `keyboard_getch` in `shell.c`; exposes `shell_getch` for apps.
- Serial debug helpers sprinkled through `fat.c` and `shell.c` for tracing.

## Users & Accounts
- Accounts stored as `accounts.bin` under `/usr` (see `shell_save_accounts` / `shell_load_accounts` in `shell.c`).
- Basic user management commands: `adduser`, `users`, `whoami`, etc.
- User home is `/usr/<user>`; shell starts in that directory after login.

## Commands (Built-ins)
- Core: `help`, `clear`, `whoami`, `users`, `adduser`, `logout`, `halt`, `pwd`, `cd`, `ls`, `mkdir`, `touch`, `cat`.
- FS utilities: `mkuserdir`, `fixuserdirs`, `fstest`.
- System: `time` (stub), `version`, `reboot`.
- Editor: `tedit` (see `docs/tedit.md`).

## Editor (tedit)
- Minimal nano-like editor in `kernel/cmd/cmd_tedit.c`.
- Controls: `Ctrl+S` save, `Ctrl+Q` quit (warns if dirty), arrows/Home/End move, Backspace/Delete edit, Enter newline.
- Line numbers, inline underscore cursor; viewport scrolls to keep cursor visible.
- Docs: `docs/tedit.md`.

## Known Limitations
- No multitasking or userland processes; everything runs in kernel space.
- No search/replace in `tedit`; buffer limited to 8 KB.
- FAT implementation is basic (no long-name write, limited robustness).
- Input line editing in shell is minimal (no history by default).

## Building & Running
- Prereqs: x86_64-elf toolchain, `mkfs.fat`, `python3`, `nasm`, QEMU.
- Typical flow:
  1. `make clean`
  2. `make`
  3. `make run` (boots QEMU with the built disk image)

## Troubleshooting
- **Missing commands**: ensure `/shell/cmd/<name>` stub files exist on the FAT partition.
- **Title not updating**: apps should restore `shell_title`; shell resets it after commands.
- **FAT write warnings**: see debug output on serial/stdout; transient sector failures may retry.
- **QEMU GUI issues**: use `-display sdl/win32` or `-display curses` if GTK fails.
