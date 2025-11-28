#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/hal.h"
#include <stdbool.h>

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t v) {
    return ((v >> 4) * 10) + (v & 0x0F);
}

static void cmd_time_run(const char *arg1, const char *arg2) {
    (void)arg1; (void)arg2;

    // Wait for update to finish
    while (cmos_read(0x0A) & 0x80) {}

    uint8_t sec = cmos_read(0x00);
    uint8_t min = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t mon = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);
    uint8_t statusB = cmos_read(0x0B);

    bool binary = (statusB & 0x04) != 0;
    bool hour24 = (statusB & 0x02) != 0;

    if (!binary) {
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        // Hour can have 12/24 flag in high bit if 12h mode
        hour = bcd_to_bin(hour & 0x7F);
        day = bcd_to_bin(day);
        mon = bcd_to_bin(mon);
        year = bcd_to_bin(year);
    }

    if (!hour24) {
        // 12-hour format: high bit indicates PM
        bool pm = (hour & 0x80) != 0;
        hour &= 0x7F;
        hour = (uint8_t)((hour % 12) + (pm ? 12 : 0));
    }

    char buf[32];
    int idx = 0;
    buf[idx++] = '2'; buf[idx++] = '0';
    buf[idx++] = '0' + (year / 10);
    buf[idx++] = '0' + (year % 10);
    buf[idx++] = '-';
    buf[idx++] = '0' + (mon / 10);
    buf[idx++] = '0' + (mon % 10);
    buf[idx++] = '-';
    buf[idx++] = '0' + (day / 10);
    buf[idx++] = '0' + (day % 10);
    buf[idx++] = ' ';
    buf[idx++] = '0' + (hour / 10);
    buf[idx++] = '0' + (hour % 10);
    buf[idx++] = ':';
    buf[idx++] = '0' + (min / 10);
    buf[idx++] = '0' + (min % 10);
    buf[idx++] = ':';
    buf[idx++] = '0' + (sec / 10);
    buf[idx++] = '0' + (sec % 10);
    buf[idx] = 0;

    shell_write_line(buf);
}

const command_t CMD_TIME = {
    .name = "time",
    .handler = cmd_time_run,
    .help = "time: show BIOS RTC time (YYYY-MM-DD HH:MM:SS)",
};
