#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../include/hal.h"

#define ATA_IO_BASE 0x1F0
#define ATA_CTRL    0x3F6
#define TIMEOUT_LIMIT 1000000  // Arbitrary timeout counter

static void ata_delay(void) {
    inb(ATA_CTRL);
    inb(ATA_CTRL);
    inb(ATA_CTRL);
    inb(ATA_CTRL);
}

static void debug_serial(char c) {
    while (!(inb(0x3FD) & 0x20)) {}
    outb(0x3F8, (uint8_t)c);
}

static bool ata_pio_rw28(bool write, uint32_t lba, void *buf) {
    debug_serial('1');  // Entered ata_pio_rw28
    
    uint16_t *data = (uint16_t *)buf;
    outb(ATA_CTRL, 0x00); // nIEN cleared
    outb(ATA_IO_BASE + 6, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay();
    
    debug_serial('2');  // Setup done
    
    outb(ATA_IO_BASE + 2, 1);                  // sector count
    outb(ATA_IO_BASE + 3, (uint8_t)(lba & 0xFF));
    outb(ATA_IO_BASE + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_IO_BASE + 5, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_IO_BASE + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_IO_BASE + 7, write ? 0x30 : 0x20);

    debug_serial('3');  // Command sent, about to wait
    
    // wait for DRQ with timeout
    uint8_t status;
    uint32_t timeout = TIMEOUT_LIMIT;
    do {
        status = inb(ATA_IO_BASE + 7);
        if (--timeout == 0) {
            debug_serial('T');  // Timeout waiting for BSY to clear
            return false;
        }
    } while (status & 0x80);
    
    debug_serial('4');  // BSY cleared
    
    if (!(status & 0x08)) {
        debug_serial('E');  // DRQ not set - error
        return false;
    }
    
    debug_serial('5');  // DRQ set, about to transfer

    if (write) {
        for (int i = 0; i < 256; i++) {
            outw(ATA_IO_BASE, data[i]);
        }
    } else {
        for (int i = 0; i < 256; i++) {
            data[i] = inw(ATA_IO_BASE);
        }
    }
    
    debug_serial('6');  // Transfer complete
    
    ata_delay();
    return true;
}

bool disk_read_sector(uint32_t lba, void *buf) {
    debug_serial('R');  // disk_read_sector called
    bool result = ata_pio_rw28(false, lba, buf);
    debug_serial(result ? 'r' : 'X');  // 'r' = success, 'X' = failure
    return result;
}

bool disk_write_sector(uint32_t lba, const void *buf) {
    debug_serial('W');  // disk_write_sector called
    bool result = ata_pio_rw28(true, lba, (void *)buf);
    debug_serial(result ? 'w' : 'X');  // 'w' = success, 'X' = failure
    return result;
}