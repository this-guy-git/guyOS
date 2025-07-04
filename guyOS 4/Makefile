CC=i386-elf-gcc
AS=nasm
CFLAGS=-m32 -nostdlib -ffreestanding -O2 -Wall
LDFLAGS=-T linker.ld

OBJ=multiboot_header.o kernel.o graphics.o keyboard.o mouse.o

all: kernel.elf

multiboot_header.o: src/multiboot_header.asm
	$(AS) -f elf32 src/multiboot_header.asm -o multiboot_header.o

kernel.o: src/kernel.c src/graphics.h src/keyboard.h src/mouse.h
	$(CC) $(CFLAGS) -c src/kernel.c -o kernel.o

graphics.o: src/graphics.c src/graphics.h
	$(CC) $(CFLAGS) -c src/graphics.c -o graphics.o

keyboard.o: src/keyboard.c src/keyboard.h
	$(CC) $(CFLAGS) -c src/keyboard.c -o keyboard.o

mouse.o: src/mouse.c src/mouse.h
	$(CC) $(CFLAGS) -c src/mouse.c -o mouse.o

kernel.elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o kernel.elf $(OBJ)

clean:
	rm -f *.o kernel.elf
