#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "../../include/disk.h"
#include <stdint.h>

static void fstest_handler(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;
    
    shell_write_line("Testing filesystem...");
    
    // Test disk read at various locations
    uint8_t buf[512];
    
    // Test sector 0 (MBR/stage1)
    shell_write_line("Reading sector 0 (MBR)...");
    if (disk_read_sector(0, buf)) {
        if (buf[510] == 0x55 && buf[511] == 0xAA) {
            shell_write_line("  Valid boot signature at sector 0");
        }
    } else {
        shell_write_line("  Failed to read sector 0");
        return;
    }
    
    // Test sector 16384 (should be FAT partition)
    uint32_t fat_lba = 16384; // 8MB offset
    
    shell_write("Reading sector at LBA ");
    char lba_str[12];
    uint32_t lba = fat_lba;
    int i = 0;
    if (lba == 0) {
        lba_str[i++] = '0';
    } else {
        char temp[12];
        int j = 0;
        while (lba > 0) {
            temp[j++] = '0' + (lba % 10);
            lba /= 10;
        }
        while (j > 0) {
            lba_str[i++] = temp[--j];
        }
    }
    lba_str[i] = 0;
    shell_write(lba_str);
    shell_write_line("...");
    
    if (!disk_read_sector(fat_lba, buf)) {
        shell_write_line("Failed to read sector!");
        return;
    }
    
    shell_write_line("Sector read successfully");
    
    // Show first 16 bytes in hex
    shell_write("First 16 bytes: ");
    const char *hexchars = "0123456789ABCDEF";
    for (int k = 0; k < 16; k++) {
        char hex[3];
        hex[0] = hexchars[buf[k] >> 4];
        hex[1] = hexchars[buf[k] & 0xF];
        hex[2] = ' ';
        shell_write(hex);
    }
    shell_write_line("");
    
    // Check boot signature
    if (buf[510] == 0x55 && buf[511] == 0xAA) {
        shell_write_line("Valid boot signature found (0x55AA)");
    } else {
        shell_write("Invalid boot signature: ");
        char hex[5];
        uint8_t b1 = buf[510];
        uint8_t b2 = buf[511];
        hex[0] = hexchars[b1 >> 4];
        hex[1] = hexchars[b1 & 0xF];
        hex[2] = hexchars[b2 >> 4];
        hex[3] = hexchars[b2 & 0xF];
        hex[4] = 0;
        shell_write_line(hex);
        
        // Try other common offsets
        shell_write_line("Trying alternative offsets...");
        uint32_t offsets[] = {2048, 4096, 8192, 32768, 0};
        for (int o = 0; offsets[o] != 0; o++) {
            if (disk_read_sector(offsets[o], buf)) {
                if (buf[510] == 0x55 && buf[511] == 0xAA && buf[0] == 0xEB) {
                    shell_write("  Found FAT boot sector at LBA ");
                    lba = offsets[o];
                    i = 0;
                    if (lba == 0) {
                        lba_str[i++] = '0';
                    } else {
                        char temp[12];
                        int j = 0;
                        while (lba > 0) {
                            temp[j++] = '0' + (lba % 10);
                            lba /= 10;
                        }
                        while (j > 0) {
                            lba_str[i++] = temp[--j];
                        }
                    }
                    lba_str[i] = 0;
                    shell_write_line(lba_str);
                }
            }
        }
        return;
    }
    
    // Check OEM name
    shell_write("OEM Name: ");
    char oem[9];
    for (int j = 0; j < 8; j++) {
        oem[j] = buf[3 + j];
    }
    oem[8] = 0;
    shell_write_line(oem);
    
    // Check bytes per sector
    uint16_t bytes_per_sector = *(uint16_t *)(buf + 11);
    shell_write("Bytes per sector: ");
    char bps_str[8];
    uint16_t bps = bytes_per_sector;
    i = 0;
    if (bps == 0) {
        bps_str[i++] = '0';
    } else {
        char temp[8];
        int j = 0;
        while (bps > 0) {
            temp[j++] = '0' + (bps % 10);
            bps /= 10;
        }
        while (j > 0) {
            bps_str[i++] = temp[--j];
        }
    }
    bps_str[i] = 0;
    shell_write_line(bps_str);
    
    if (bytes_per_sector == 512) {
        shell_write_line("Filesystem appears valid!");
    } else {
        shell_write_line("Warning: Unexpected sector size");
    }
    
    // Try to initialize FAT
    shell_write_line("Testing FAT initialization...");
    if (fat_init(fat_lba)) {
        shell_write_line("FAT initialization successful!");
        shell_write("Root cluster: ");
        uint32_t root = fat_get_root_cluster();
        char root_str[12];
        i = 0;
        if (root == 0) {
            root_str[i++] = '0';
        } else {
            char temp[12];
            int j = 0;
            while (root > 0) {
                temp[j++] = '0' + (root % 10);
                root /= 10;
            }
            while (j > 0) {
                root_str[i++] = temp[--j];
            }
        }
        root_str[i] = 0;
        shell_write_line(root_str);
    } else {
        shell_write_line("FAT initialization failed!");
    }
}

const command_t CMD_FSTEST = {
    .name = "fstest",
    .help = "fstest: test filesystem initialization",
    .handler = fstest_handler
};