# Installer ISO Plan

This project currently builds a raw disk image (`build/guyos.img`) with an MBR bootloader and a FAT32 partition at an 8 MB offset. To ship a bootable installer ISO, we need a CD/ISO boot path and a small installer program that writes the raw image to a target disk/USB.

## Goals
- Produce a bootable ISO (El Torito) that runs on BIOS/Legacy firmware.
- Bundle the existing `guyos.img` as the payload.
- Provide a minimal installer that writes `guyos.img` to a target disk (warning first), verifies, and reboots.

## Proposed ISO Layout
- Bootloader: isolinux/syslinux or GRUB for El Torito BIOS boot.
- Files:
  - `/guyos.img` — the full HDD image (payload).
  - `/initrd` or installer binary — a small installer to flash the payload to the chosen disk.
  - Boot config (`isolinux.cfg` or GRUB config) to start the installer automatically.

## Installer Flow
1. ISO boots via El Torito into the installer (runs in 16/32-bit real/protected mode or a tiny kernel).
2. Installer scans disks and prompts for a target (default to the first non-boot disk/USB).
3. Warns that the target will be fully overwritten.
4. Writes `guyos.img` to the target (raw, preserving the MBR + 8 MB FAT offset).
5. Optionally verifies a checksum.
6. Prompts to power off or reboot; user then boots from the installed disk.

## Make Targets (to add)
- `make iso-install`:
  - Builds `guyos.img` as usual.
  - Copies an El Torito bootloader (isolinux + memdisk) and config into a staging dir.
  - Drops `guyos.img` into the ISO root.
  - Generates `build/guyos-install.iso` with `genisoimage` using El Torito options.

## Tooling
- `xorriso` or `genisoimage` + `isohybrid` (for hybrid ISO/USB).
- isolinux/syslinux or GRUB2 BIOS modules for El Torito.

## Notes & Caveats
- Only BIOS/Legacy path is considered here. UEFI would need a separate `EFI/BOOT` path.
- Payload size = size of `guyos.img`; ISO will be at least that big.
- The installer must run outside the current kernel (likely a tiny real/protected-mode program or a small Linux-based initrd); not yet implemented.
- Current `make iso-install` uses isolinux + memdisk to boot `guyos.img` directly; it behaves like booting the raw disk image (no interactive installer yet).
