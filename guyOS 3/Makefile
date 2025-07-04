# Tools
CC = i386-elf-gcc
LD = i386-elf-ld
NASM = nasm
OBJCOPY = i386-elf-objcopy
GRUB_MKRESCUE = grub-mkrescue

# Flags
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -nostdlib -Isrc
LDFLAGS = -m elf_i386 -T linker.ld

# Directories
SRC_DIR = src
BUILD_DIR = build/boot
ISO_DIR = iso/boot/grub

# Source files
SRC_C = $(wildcard $(SRC_DIR)/*.c)
OBJ_C = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_C))
ASM_SRC = $(SRC_DIR)/multiboot_header.asm
ASM_OBJ = $(BUILD_DIR)/multiboot_header.o

# Output files
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
ISO_IMAGE = os.iso

# Default target
all: iso

# Build ISO image
iso: $(KERNEL_BIN)
	mkdir -p $(ISO_DIR)
	cp $(KERNEL_BIN) iso/boot/kernel.bin
	cp grub.cfg $(ISO_DIR)/
	$(GRUB_MKRESCUE) -o $(ISO_IMAGE) iso

# Compile C source files to object files
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble the multiboot header
$(ASM_OBJ): $(ASM_SRC) | $(BUILD_DIR)
	$(NASM) -f elf $< -o $@

# Link the kernel ELF
$(KERNEL_ELF): $(ASM_OBJ) $(OBJ_C)
	$(LD) $(LDFLAGS) -o $@ $^

# Convert ELF to flat binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# Clean build and output files
clean:
	rm -rf build iso $(ISO_IMAGE)

.PHONY: all clean iso
