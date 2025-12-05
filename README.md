<div align="center">
<img src="https://github.com/user-attachments/assets/69c44cc6-470a-435d-be5f-1627fd4c80f7"
  width="500"
/>
  
# guyOS
</div>

[![Build](https://img.shields.io/github/actions/workflow/status/this-guy-git/guyOS/build.yml?branch=master&label=build&style=flat&color=7C4EE1&labelColor=222222)](https://github.com/this-guy-git/guyOS/actions/workflows/build.yml)
[![Stars](https://img.shields.io/github/stars/this-guy-git/guyOS?style=flat&color=7C4EE1&labelColor=222222)](https://github.com/this-guy-git/guyOS/stargazers)
[![Issues](https://img.shields.io/github/issues/this-guy-git/guyOS?style=flat&color=7C4EE1&labelColor=222222)](https://github.com/this-guy-git/guyOS/issues)
[![Last Commit](https://img.shields.io/github/last-commit/this-guy-git/guyOS?style=flat&color=7C4EE1&labelColor=222222)](https://github.com/this-guy-git/guyOS/commits/master)
[![License](https://img.shields.io/badge/license-GPLv3-lightgrey?style=flat&color=7C4EE1&labelColor=222222)](#license)

guyOS is a small x86_64 hobby OS with a text-mode shell (gsh), a FAT32 filesystem, and built-in commands including a tiny editor (`tedit`). This repo builds a bootable disk image and runs it in QEMU.

## Table of Contents
- [Features](#features)
- [Repo Layout](#repo-layout)
- [Prerequisites](#prerequisites)
- [Build & Run](#build--run)
- [Filesystem Layout](#filesystem-layout)
- [tedit (Tiny Editor)](#tedit-tiny-editor)
- [Known Limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [More Docs](#more-docs)
- [License](#license)

## Features
- Text-mode shell gsh with prompt `user @ guyOS/<cwd>`.
- FAT32 filesystem (8 MB offset) with basic file/dir ops; 8.3 + simple LFN handling.
- Built-in commands: `help`, `clear`, `whoami`, `users`, `adduser`, `logout`, `halt`, `pwd`, `cd`, `ls`, `mkdir`, `touch`, `cat`, `mkuserdir`, `fixuserdirs`, `fstest`, `time` (RTC-backed), `version`, `reboot`, `tedit`, and `gxe` for running `.gxe` apps.
- User accounts persisted in `/usr/accounts.bin`; per-user homes under `/usr/<user>`.
- Minimal nano-like editor `tedit` (see [docs/tedit.md](docs/tedit.md)).

## Repo Layout
- `boot/` - Stage1/Stage2 bootloader.
- `kernel/` - Kernel sources (gkern: `kernel.c`, `shell.c`, `fat.c`, `disk.c`, `commands.c`) and command implementations under `kernel/cmd/`.
- `include/` - Public headers (`commands.h`, `shell_api.h`, `fat.h`, etc.).
- `docs/` - Documentation (`overview.md`, `filesystem.md`, `tedit.md`, `commands.md`).
- `buildfs.py`, `builddisk.py` - Helpers to populate the FAT partition and assemble the disk image.
- `Makefile` - Build/run targets.

## Prerequisites
- x86_64-elf toolchain (`gcc`, `ld`, `objcopy`).
- `nasm`, `python3`, `mkfs.fat`.
- QEMU for running: `qemu-system-x86_64`.

## Build & Run
```bash
make clean
make
make run   # boots QEMU with the built disk image
# optional: make iso   # packages guyos.img and fat.img into build/guyos.iso
# optional: make iso-install   # builds a bootable installer ISO (isolinux+memdisk) in build/guyos-install.iso
```

## Filesystem Layout
- Root FAT partition at 8 MB offset.
- Key paths: `/usr/<user>/downloads`, `/usr/<user>/documents`, `/shell/cmd` (command stubs), `/vital` (kernel/bootloader).
- See [docs/filesystem.md](docs/filesystem.md) for full layout.

## tedit (Tiny Editor)
- Minimal nano-like editor; controls: `Ctrl+S` save, `Ctrl+Q` quit, arrows/Home/End move, Backspace/Delete edit, Enter newline.
- Line numbers, inline underscore cursor; 8 KB buffer.
- Docs: [docs/tedit.md](docs/tedit.md).

## Known Limitations
- No multitasking/userland; everything runs in kernel space.
- No search/replace in `tedit`; no syntax highlighting.
- FAT support is basic; limited robustness for large/fragmented files.

## Troubleshooting
- **Missing commands**: ensure `/shell/cmd/<name>` stubs exist on the FAT partition.
- **Title not updating**: apps must restore `shell_title`; the shell resets it after commands.
- **QEMU GUI issues**: try `-display sdl/win32` or `-display none` if GTK fails.

## More Docs
- [docs/overview.md](docs/overview.md) - OS summary.
- [docs/filesystem.md](docs/filesystem.md) - FAT layout and directories.
- [docs/tedit.md](docs/tedit.md) - Tiny editor usage.
- [docs/commands.md](docs/commands.md) - Built-in commands and usage.
- [docs/shell.md](docs/shell.md) - Shell internals and APIs.
- [docs/build.md](docs/build.md) - Build/run details.
- [docs/virtualbox.md](docs/virtualbox.md) - Running under VirtualBox (VDI/raw).
- [docs/installer.md](docs/installer.md) - Installer ISO plan and workflow.

## License
GPL-3.0 (see `LICENSE`).
## GScript
- Tiny scripting language -> `.gxe` bytecode, run with `gxe` in gsh.
- Features: print/println, clear/title, input -> last_input, 16 string slots, slot_set, load_file/save_file (newline text), EXIT.
- Build: `python3 gs/compiler.py <input.gs> <output.gxe>`; place outputs in `gxe/` so `make` copies to `/shell/gxe`.
- Examples: `gs/examples/gsedit_demo.gs`, `input_echo.gs`, `banner.gs`, `menu2.gs`, `versions.gs`.
- See `docs/gscript.md` for full guide.
