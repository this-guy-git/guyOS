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

DISK_SIZE ?= 134217728  # 128MB
FAT_OFFSET ?= 8388608    # 8MB offset
FAT_SIZE_KB ?= 32768     # 32MB FAT

QEMU ?= qemu-system-x86_64
QEMU_FLAGS := -machine q35 -cpu qemu64 -m 256M -serial stdio -drive format=raw,file=$(DISK_IMG)

CFLAGS := -ffreestanding -fno-stack-protector -fno-pic -m64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -O2 -Wall -Wextra -Wno-unused-parameter -g -Iinclude
LDFLAGS := -nostdlib -z max-page-size=0x1000

all: $(DISK_IMG)

run: $(DISK_IMG)
	$(QEMU) $(QEMU_FLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(STAGE2_BIN): $(BOOT_DIR)/stage2.asm | $(BUILD_DIR)
	$(NASM) -f bin -o $@ $<

$(STAGE1_BIN): $(BOOT_DIR)/stage1.asm $(STAGE2_BIN) | $(BUILD_DIR)
	SECT=$$($(PYTHON) -c "import math, os; size=os.path.getsize('$(STAGE2_BIN)'); print(max(1,(size+511)//512))"); \
	$(NASM) -f bin -DSTAGE2_SECTORS=$$SECT -o $@ $<

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

$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(FAT_IMG) KERNEL_META
	$(PYTHON) -c "from pathlib import Path; pad=lambda b: b if len(b)%512==0 else b+b'\\x00'*(512-(len(b)%512)); \
stage1=pad(Path('$(STAGE1_BIN)').read_bytes()); stage2=pad(Path('$(STAGE2_BIN)').read_bytes()); kernel=pad(Path('$(KERNEL_BIN)').read_bytes()); fat=pad(Path('$(FAT_IMG)').read_bytes()); \
offset=$(FAT_OFFSET); disk=stage1+stage2+kernel; \
disk = disk if len(disk)>=offset else disk + b'\\x00'*(offset-len(disk)); \
disk = (disk[:offset]+fat+disk[offset+len(fat):]) if len(disk)>=offset+len(fat) else (disk + b'\\x00'*((offset+len(fat))-len(disk)) + fat); \
disk = disk if len(disk)>=$(DISK_SIZE) else disk + b'\\x00'*($(DISK_SIZE)-len(disk)); \
Path('$(DISK_IMG)').write_bytes(disk); print(f'Disk written: {len(disk)} bytes, FAT offset {offset}, FAT size {len(fat)}')"

clean:
	rm -f $(BUILD_DIR)/*.bin $(BUILD_DIR)/*.o $(BUILD_DIR)/*.elf $(BUILD_DIR)/*.img $(BUILD_DIR)/KERNEL_META $(BUILD_DIR)/cmd/*

.PHONY: all run clean
