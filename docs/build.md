# Build & Run Guide

## Prerequisites
- x86_64-elf toolchain (`gcc`, `ld`, `objcopy`)
- `nasm`, `python3`, `mkfs.fat`
- QEMU: `qemu-system-x86_64`

## Quick Build
```bash
make clean
make
make run   # boots QEMU with the built disk image
```

## What `make` Does
1. Assemble bootloader (`boot/stage1.asm`, `boot/stage2.asm`).
2. Build kernel (`kernel/*.c`, `kernel/cmd/*.c`, `boot.asm`).
3. Create FAT image (`mkfs.fat`), populate filesystem (`buildfs.py`).
4. Assemble disk image with bootloader + FAT partition (`builddisk.py`).
5. `make run` launches QEMU with `build/guyos.img`.

## Image Layout
- Disk: `build/guyos.img`
- FAT partition offset: 8 MB (see `Makefile` and `FAT_PART_LBA`).
- Stubs for commands are placed in `/shell/cmd` by `buildfs.py`.

## Common Issues
- **Toolchain missing**: ensure `x86_64-elf-gcc`/`ld` are in PATH.
- **QEMU GUI errors**: try `-display sdl/win32` or `-display none`.
- **Missing commands**: re-run `buildfs.py` or ensure stubs exist in `/shell/cmd`.

## Customizing QEMU
- Default flags in `Makefile`: `-m 256M -serial stdio -hda build/guyos.img`
- Add `-display sdl`/`win32`/`none` as needed.

## Cleaning
```bash
make clean
```
Removes build artifacts and unmounts temporary mount points if used.
