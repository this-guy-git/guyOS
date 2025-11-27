#!/usr/bin/env python3
"""
Build guyOS filesystem structure on FAT32 image
"""
import os
import sys
import subprocess
import shutil
from pathlib import Path

def run(cmd):
    """Run command and check result"""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error: {result.stderr}")
        sys.exit(1)
    return result.stdout

def create_directory_structure(mount_point):
    """Create the guyOS directory structure"""
    dirs = [
        "usr",
        "shell",
        "shell/cmd",
        "vital",
        "vital/kernel",
        "vital/bootloader",
    ]
    
    for d in dirs:
        path = mount_point / d
        path.mkdir(parents=True, exist_ok=True)
        print(f"Created: {path}")

def short_cmd_name(long_name: str) -> str:
    aliases = {
        "mkuserdir": "mkuserdi",
        "fixuserdirs": "fixuserd",
    }
    if long_name in aliases:
        return aliases[long_name]
    # default: truncate to 8 characters for 8.3 compatibility
    return long_name[:8]

def create_command_files(mount_point):
    """Drop placeholder command files under /shell/cmd (8.3 friendly names) based on kernel/cmd sources."""
    cmd_dir = mount_point / "shell" / "cmd"
    cmd_dir.mkdir(parents=True, exist_ok=True)

    source_dir = Path("kernel/cmd")
    commands = []
    for cfile in source_dir.glob("cmd_*.c"):
        name = cfile.stem.replace("cmd_", "")
        commands.append(name)

    for long_name in sorted(set(commands)):
        fname = short_cmd_name(long_name)
        dest = cmd_dir / fname
        with open(dest, 'w') as f:
            f.write(f"builtin command stub for {long_name}\n")
        print(f"Created command stub: {dest}")

def copy_system_files(mount_point, build_dir):
    """Copy kernel and bootloader files to filesystem"""
    
    # Copy kernel files
    kernel_dest = mount_point / "vital" / "kernel"
    kernel_files = [
        build_dir / "kernel.elf",
        build_dir / "kernel.bin",
    ]
    for f in kernel_files:
        if f.exists():
            shutil.copy2(f, kernel_dest)
            print(f"Copied {f.name} to {kernel_dest}")
    
    # Copy bootloader files
    bootloader_dest = mount_point / "vital" / "bootloader"
    bootloader_files = [
        build_dir / "stage1.bin",
        build_dir / "stage2.bin",
    ]
    for f in bootloader_files:
        if f.exists():
            shutil.copy2(f, bootloader_dest)
            print(f"Copied {f.name} to {bootloader_dest}")

def create_readme(mount_point):
    """Create a README file in root"""
    readme = mount_point / "README.TXT"
    with open(readme, 'w') as f:
        f.write("guyOS Filesystem\n")
        f.write("================\n\n")
        f.write("Directory Structure:\n")
        f.write("  /usr          - User home directories\n")
        f.write("  /shell        - Shell and command files\n")
        f.write("  /shell/cmd    - Command binaries\n")
        f.write("  /vital        - System critical files\n")
        f.write("  /vital/kernel - Kernel binaries\n")
        f.write("  /vital/bootloader - Bootloader files\n")
    print(f"Created README at {readme}")

def main():
    if len(sys.argv) < 3:
        print("Usage: build_fs.py <fat_image> <build_dir>")
        sys.exit(1)
    
    fat_image = Path(sys.argv[1])
    build_dir = Path(sys.argv[2])
    
    if not fat_image.exists():
        print(f"Error: FAT image {fat_image} does not exist")
        sys.exit(1)
    
    if not build_dir.exists():
        print(f"Error: Build directory {build_dir} does not exist")
        sys.exit(1)
    
    # Create temporary mount point
    mount_point = Path("/tmp/guyos_mount")
    mount_point.mkdir(exist_ok=True)
    
    # Check if already mounted and unmount if necessary
    try:
        result = subprocess.run(["mountpoint", "-q", str(mount_point)], 
                              capture_output=True)
        if result.returncode == 0:
            print("Unmounting existing mount...")
            subprocess.run(["sudo", "umount", str(mount_point)], 
                         capture_output=True)
    except FileNotFoundError:
        # mountpoint command not found, try to unmount anyway
        subprocess.run(["sudo", "umount", str(mount_point)], 
                     capture_output=True, stderr=subprocess.DEVNULL)
    
    try:
        # Get current user UID and GID
        import pwd
        uid = os.getuid()
        gid = os.getgid()
        
        # Mount the FAT image
        print(f"Mounting {fat_image} to {mount_point}")
        run(["sudo", "mount", "-o", f"loop,uid={uid},gid={gid}", 
             str(fat_image), str(mount_point)])
        
        # Create directory structure
        create_directory_structure(mount_point)
        create_command_files(mount_point)
        
        # Copy system files
        copy_system_files(mount_point, build_dir)
        
        # Create README
        create_readme(mount_point)
        
        # Sync to ensure everything is written
        run(["sync"])
        
        print("\nFilesystem structure created successfully!")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
    finally:
        # Unmount
        print("Unmounting...")
        result = subprocess.run(["sudo", "umount", str(mount_point)], 
                              capture_output=True, text=True)
        if result.returncode != 0 and "not mounted" not in result.stderr:
            print(f"Warning: unmount failed: {result.stderr}")
        print("Done!")

if __name__ == "__main__":
    main()
