# guyOS Filesystem Structure

## Overview
guyOS uses a FAT32 filesystem for compatibility and simplicity. The filesystem is persistent across reboots and can be accessed from the host system.

## Directory Structure

```
/
├── usr/                   User home directories
│   └── <username>/        Individual user directories
│       ├── downloads/     User downloads
│       └── documents/     User documents
├── shell/                 Shell components
│   └── cmd/               Command binaries
│       └── <cmdname>/     Individual command files (8.3 short names)
└── vital/                 System critical files
    ├── kernel/            Kernel binaries
    └── bootloader/        Bootloader files

```

## Implementation Details

### FAT32 Layout
- **Partition offset**: 8MB from start of disk
- **Partition size**: 100MB (configurable)
- **Sector size**: 512 bytes
- **Cluster size**: 512 bytes (1 sector per cluster)

### File Operations
The filesystem supports:
- Creating directories (`mkdir`)
- Listing directory contents (`ls`)
- Reading files (`fat_read_file`)
- Writing files (`fat_write_file`)

### User Directories
When a user is created, the system:
1. Creates `/usr/<username>/`
2. Creates `/usr/<username>/downloads/`
3. Creates `/usr/<username>/documents/`

### System Files
- Kernel binaries are stored in `/vital/kernel/`
- Bootloader stages are stored in `/vital/bootloader/`
- Command implementations go in `/shell/cmd/<cmdname>/` (short-name friendly: e.g. `mkuserdi` for `mkuserdir`, `fixuserd` for `fixuserdirs`)

## Building the Filesystem

### Prerequisites
```bash
sudo apt-get install dosfstools  # For mkfs.fat
```

### Build Process
```bash
make clean
make
```

The build process:
1. Creates a FAT32 image with `mkfs.fat`
2. Runs `buildfs.py` to mount and populate the filesystem
3. Copies kernel and bootloader files to appropriate locations
4. Creates the directory structure
5. Unmounts and integrates the FAT image into the disk image

### Manual Filesystem Access
You can mount the FAT partition from your host system:

```bash
# Extract FAT partition from disk image
dd if=build/guyos.img of=fat_partition.img bs=512 skip=16384 count=204800

# Mount it
mkdir -p /tmp/guyos_mount
sudo mount -o loop fat_partition.img /tmp/guyos_mount

# Make changes...

# Unmount
sudo umount /tmp/guyos_mount

# Write back to disk image
dd if=fat_partition.img of=build/guyos.img bs=512 seek=16384 conv=notrunc
```

## Future Enhancements

### Planned Features
1. **Path Navigation**: Full path support (e.g., `/usr/alice/downloads/file.txt`)
2. **CD Command**: Change directory support
3. **File Manager**: Visual file browser
4. **Permissions**: Basic file permissions and ownership
5. **Installation**: Install guyOS to a real disk or USB drive

### Installer Implementation
The installer will:
1. Detect target disk
2. Partition the disk (MBR with one FAT32 partition)
3. Format the partition
4. Copy bootloader to MBR and partition boot sector
5. Copy kernel and system files to `/vital/`
6. Create base directory structure
7. Set up initial user account

## Current Limitations

1. **No subdirectory navigation**: Currently can only create/access directories in root
2. **8.3 filename format**: Limited to DOS-style short names (being addressed)
3. **Single-cluster directories**: Directories limited to one cluster size
4. **No fragmentation handling**: Files should be written in one operation
5. **Basic error handling**: Limited error recovery

## Development Roadmap

### Phase 1 (Current)
- Basic FAT32 read/write
- Directory creation
- File listing
- Persistent storage

### Phase 2 (Next)
- Full path support
- CD command
- User home directory management
- Improved filename support (long names)

### Phase 3 (Future)
- File permissions
- Disk installer
- Multi-directory operations
- File search

### Phase 4 (Advanced)
- Multiple partition support
- Other filesystem support (ext2/3)
- Network filesystem support
- Virtual filesystem layer

## Testing

### Verify Filesystem
```bash
# Build and run
make clean run

# In guyOS shell
ls                    # List root directory
mkdir test            # Create test directory
ls                    # Verify it appears
```

### Check from Host
```bash
# Mount the FAT partition
sudo mount -o loop,offset=$((8*1024*1024)) build/guyos.img /tmp/guyos_mount
ls -la /tmp/guyos_mount
sudo umount /tmp/guyos_mount
```

## Troubleshooting

### "Failed to list directory"
- Filesystem may not be initialized
- Check FAT partition offset in Makefile
- Verify disk image integrity

### "Failed to create directory"
- Directory may already exist
- Filesystem may be full
- Check cluster allocation in FAT

### Build script fails
- Ensure `dosfstools` is installed
- Verify sudo access for mounting
- Check available disk space

### Mount permission denied
- Run with sudo: `sudo python3 build_fs.py ...`
- Check if `/tmp/guyos_mount` is already mounted
- Verify loop device availability

## References

- FAT32 Specification: [Microsoft FAT specification](https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf)
- OSDev FAT Wiki: [osdev.org/FAT](https://wiki.osdev.org/FAT)
- Bootloader Integration: See `boot/stage1.asm` and `boot/stage2.asm`
