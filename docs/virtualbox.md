# VirtualBox Usage (ISO/VDI)

guyOS builds a raw disk image (`build/guyos.img`) with a custom bootloader and FAT32 partition. VirtualBox cannot boot that directly as an ISO, but you can use it via VDI conversion or attach the raw image.

## Option 1: Convert to VDI
1. Build the disk image:
   ```bash
   make clean
   make
   ```
2. Convert to VDI (VBoxManage is installed with VirtualBox):
   ```bash
   VBoxManage convertfromraw build/guyos.img build/guyos.vdi --format VDI
   ```
3. In VirtualBox:
   - Create a new VM (type: Other/Unknown 64-bit).
   - Attach `build/guyos.vdi` as the virtual disk.
   - Boot.

## Option 2: Use raw disk image
1. Build the disk image (`make`).
2. In VirtualBox, attach `build/guyos.img` as a raw disk:
   ```bash
   VBoxManage internalcommands createrawvmdk -filename build/guyos.vmdk -rawdisk "$PWD/build/guyos.img"
   ```
3. Attach `build/guyos.vmdk` to the VM and boot.

## Notes
- No ISO is produced because the boot path uses a custom MBR + FAT32 layout, not El Torito.
- Ensure the VM firmware is set to BIOS/Legacy (not EFI) to match the bootloader.
- You can still run under QEMU with `make run` for quick testing.
