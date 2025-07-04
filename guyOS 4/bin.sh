make clean
make
mkdir -p isodir/boot/grub
cp kernel.elf isodir/boot/
cp boot/grub.cfg isodir/boot/grub/
grub-mkrescue -o guyos.iso isodir
qemu-system-i386 -cdrom guyos.iso
