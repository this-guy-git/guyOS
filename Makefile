SHELL := /bin/bash

CC ?= x86_64-elf-gcc
LD ?= x86_64-elf-ld
NASM ?= nasm
OBJCOPY ?= objcopy
PYTHON ?= python3

BUILD_DIR := build
BOOT_DIR := boot
KERNEL_DIR := kernel

STAGE1_BIN := $(BUILD_DIR)/stage1.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
CMD_SRCS := $(wildcard kernel/cmd/*.c)
CMD_OBJS := $(patsubst kernel/%.c,$(BUILD_DIR)/%.o,$(CMD_SRCS))
DISK_IMG := $(BUILD_DIR)/guyos.img
FAT_IMG := $(BUILD_DIR)/fat.img
ISO_IMG := $(BUILD_DIR)/guyos.iso
ISO_INSTALL := $(BUILD_DIR)/guyos-install.iso
ISO_STAGE := $(BUILD_DIR)/iso_install

DISK_SIZE ?= 134217728  # 128MB
FAT_OFFSET ?= 8388608    # 8MB offset
FAT_SIZE_KB ?= 102400    # 100MB FAT (increased for filesystem)

QEMU ?= qemu-system-x86_64
QEMU_FLAGS := -m 256M -serial stdio -hda $(DISK_IMG)

# Paths for isolinux-based installer ISO (override if different on your distro)
ISOLINUX_BIN ?= /usr/lib/ISOLINUX/isolinux.bin
LDLINUX_C32 ?= /usr/lib/syslinux/modules/bios/ldlinux.c32
MEMDISK ?= /usr/lib/syslinux/memdisk

CFLAGS := -ffreestanding -fno-stack-protector -fno-pic -m64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -O2 -Wall -Wextra -Wno-unused-parameter -g -Iinclude
LDFLAGS := -nostdlib -z max-page-size=0x1000

all: $(DISK_IMG)

run: $(DISK_IMG)
	$(QEMU) $(QEMU_FLAGS)

iso: $(ISO_IMG)
iso-install: $(ISO_INSTALL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/cmd

$(STAGE2_BIN): $(BOOT_DIR)/stage2.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

$(STAGE1_BIN): $(BOOT_DIR)/stage1.asm $(STAGE2_BIN) | $(BUILD_DIR)
	SECT=$$($(PYTHON) -c "import math, os; size=os.path.getsize('$(STAGE2_BIN)'); print(max(1,(size+511)//512))"); \
	$(NASM) -f bin -DSTAGE2_SECTORS=$$SECT -o $@ $<

# Note: Added cmd_touch.c and cmd_cat.c to the compilation
$(KERNEL_ELF): $(KERNEL_DIR)/linker.ld $(KERNEL_DIR)/boot.asm $(KERNEL_DIR)/kernel.c $(KERNEL_DIR)/shell.c $(KERNEL_DIR)/commands.c $(KERNEL_DIR)/disk.c $(KERNEL_DIR)/fat.c $(CMD_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/kernel.c -o $(BUILD_DIR)/kernel.o
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/shell.c -o $(BUILD_DIR)/shell.o
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/commands.c -o $(BUILD_DIR)/commands.o
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/disk.c -o $(BUILD_DIR)/disk.o
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/fat.c -o $(BUILD_DIR)/fat.o
	$(NASM) -f elf64 $(KERNEL_DIR)/boot.asm -o $(BUILD_DIR)/boot.o
	@for f in $(CMD_SRCS); do \
		obj=$(BUILD_DIR)/$${f#kernel/}; obj=$${obj%.c}.o; \
		mkdir -p $$(dirname $$obj); \
		$(CC) $(CFLAGS) -c $$f -o $$obj; \
	done
	$(LD) $(LDFLAGS) -T $(KERNEL_DIR)/linker.ld -o $@ $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/commands.o $(BUILD_DIR)/disk.o $(BUILD_DIR)/fat.o $(CMD_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

KERNEL_META: $(KERNEL_BIN) $(STAGE2_BIN)
	$(PYTHON) -c "import math, os, struct; stage2_path=r'$(STAGE2_BIN)'; kernel_path=r'$(KERNEL_BIN)'; stage2_size=os.path.getsize(stage2_path); kernel_size=os.path.getsize(kernel_path); stage2_sectors=max(1,(stage2_size+511)//512); kernel_sectors=max(1,(kernel_size+511)//512); kernel_lba=1+stage2_sectors; data=open(stage2_path,'rb').read(); marker=b'META'; idx=data.find(marker); assert idx!=-1,'META marker not found in stage2.bin'; f=open(stage2_path,'r+b'); f.seek(idx+len(marker)); f.write(struct.pack('<I',kernel_lba)); f.write(struct.pack('<I',kernel_sectors)); f.write(struct.pack('<I',stage2_sectors)); f.close(); print(f'stage2 sectors={stage2_sectors}, kernel sectors={kernel_sectors}, kernel_lba={kernel_lba}')"
	@touch $(BUILD_DIR)/KERNEL_META

$(FAT_IMG): | $(BUILD_DIR)
	@if ! command -v mkfs.fat >/dev/null 2>&1; then echo "mkfs.fat not found (install dosfstools)"; exit 1; fi
	mkfs.fat -F32 -s 1 -S 512 -n GUYOS -C $(FAT_IMG) $(FAT_SIZE_KB)

# Build filesystem structure after creating FAT image
FS_BUILT: $(FAT_IMG) $(KERNEL_BIN) $(STAGE1_BIN) $(STAGE2_BIN)
	@echo "Building filesystem structure..."
	$(PYTHON) buildfs.py $(FAT_IMG) $(BUILD_DIR)
	@touch $(BUILD_DIR)/FS_BUILT

$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) FS_BUILT KERNEL_META
	@echo "Building disk image..."
	$(PYTHON) builddisk.py $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(FAT_IMG) $(FAT_OFFSET) $(DISK_SIZE)

$(ISO_IMG): $(DISK_IMG) $(FAT_IMG)
	@echo "Creating ISO with disk and FAT images..."
	@mkdir -p $(BUILD_DIR)/iso_root
	@cp $(DISK_IMG) $(BUILD_DIR)/iso_root/
	@cp $(FAT_IMG) $(BUILD_DIR)/iso_root/
	@genisoimage -quiet -o $(ISO_IMG) -V GUYOS -iso-level 3 $(BUILD_DIR)/iso_root
	@rm -rf $(BUILD_DIR)/iso_root

$(ISO_INSTALL): $(DISK_IMG)
	@echo "Creating bootable installer ISO (isolinux + memdisk)..."
	@rm -rf $(ISO_STAGE)
	@mkdir -p $(ISO_STAGE)/isolinux
	@cp $(ISOLINUX_BIN) $(ISO_STAGE)/isolinux/isolinux.bin
	@cp $(LDLINUX_C32) $(ISO_STAGE)/isolinux/ldlinux.c32
	@cp $(MEMDISK) $(ISO_STAGE)/isolinux/memdisk
	@cp $(DISK_IMG) $(ISO_STAGE)/isolinux/guyos.img
	@printf "DEFAULT guyos\nPROMPT 0\nTIMEOUT 50\n\nLABEL guyos\n  KERNEL memdisk\n  APPEND harddisk initrd=guyos.img\n" > $(ISO_STAGE)/isolinux/isolinux.cfg
	@genisoimage -quiet -o $(ISO_INSTALL) \
		-b isolinux/isolinux.bin -c isolinux/boot.cat \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-V GUYOS-INSTALL \
		$(ISO_STAGE)
	@rm -rf $(ISO_STAGE)

clean:
	rm -f $(BUILD_DIR)/*.bin $(BUILD_DIR)/*.o $(BUILD_DIR)/*.elf $(BUILD_DIR)/*.img $(BUILD_DIR)/KERNEL_META $(BUILD_DIR)/FS_BUILT $(BUILD_DIR)/cmd/*.o
	@if [ -d "/tmp/guyos_mount" ]; then \
		if mountpoint -q /tmp/guyos_mount 2>/dev/null; then \
			sudo umount /tmp/guyos_mount 2>/dev/null || true; \
		fi; \
	fi

.PHONY: all run clean iso iso-install
