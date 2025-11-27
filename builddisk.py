#!/usr/bin/env python3
"""
Build the final guyOS disk image by combining bootloader, kernel, and FAT partition
"""
import sys
from pathlib import Path

def pad_to_sector(data):
    """Pad data to 512-byte boundary"""
    if len(data) % 512 == 0:
        return data
    return data + b'\x00' * (512 - (len(data) % 512))

def main():
    if len(sys.argv) != 7:
        print("Usage: build_disk.py <stage1> <stage2> <kernel> <fat_img> <fat_offset> <total_size>")
        sys.exit(1)
    
    stage1_path = Path(sys.argv[1])
    stage2_path = Path(sys.argv[2])
    kernel_path = Path(sys.argv[3])
    fat_path = Path(sys.argv[4])
    fat_offset = int(sys.argv[5])
    total_size = int(sys.argv[6])
    
    # Read and pad components
    stage1 = pad_to_sector(stage1_path.read_bytes())
    stage2 = pad_to_sector(stage2_path.read_bytes())
    kernel = pad_to_sector(kernel_path.read_bytes())
    fat = fat_path.read_bytes()
    
    # Build disk image
    disk = stage1 + stage2 + kernel
    
    # Pad to FAT partition offset
    pad_size = fat_offset - len(disk)
    if pad_size > 0:
        disk = disk + b'\x00' * pad_size
    
    # Insert FAT partition at offset
    disk = disk[:fat_offset] + fat
    
    # Pad to total disk size
    if len(disk) < total_size:
        disk = disk + b'\x00' * (total_size - len(disk))
    
    # Write output
    output_path = Path('build/guyos.img')
    output_path.write_bytes(disk)
    
    # Report
    print(f'Disk image created:')
    print(f'  Stage1+Stage2+Kernel: {len(stage1)+len(stage2)+len(kernel)} bytes')
    print(f'  FAT partition offset: {fat_offset} bytes (sector {fat_offset//512})')
    print(f'  FAT partition size: {len(fat)} bytes')
    print(f'  Total disk size: {len(disk)} bytes')

if __name__ == '__main__':
    main()