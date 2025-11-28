#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "path_utils.h"
#include <stddef.h>
#include <stdint.h>

static const char hexchars[] = "0123456789ABCDEF";

static void print_hex_line(uint32_t offset, const uint8_t *buf, size_t n) {
    char line[80];
    int idx = 0;
    // offset
    for (int i = 7; i >= 0; i--) {
        line[idx++] = hexchars[(offset >> (i * 4)) & 0xF];
    }
    line[idx++] = ':';
    line[idx++] = ' ';
    for (size_t i = 0; i < 16; i++) {
        if (i < n) {
            line[idx++] = hexchars[(buf[i] >> 4) & 0xF];
            line[idx++] = hexchars[buf[i] & 0xF];
        } else {
            line[idx++] = ' ';
            line[idx++] = ' ';
        }
        line[idx++] = ' ';
    }
    line[idx] = 0;
    shell_write_line(line);
}

static void hexdump_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    if (!arg1 || !arg1[0]) {
        shell_write_line("hexdump: missing file operand");
        shell_write_line("Usage: hexdump FILE");
        return;
    }

    uint32_t dir_cluster = 0;
    const char *leaf = 0;
    if (!split_path_parent(arg1, shell_current_dir_cluster(), &dir_cluster, &leaf)) {
        shell_write("hexdump: invalid path '"); shell_write(arg1); shell_write_line("'");
        return;
    }

    uint8_t buf[1024];
    size_t got = 0;
    if (!fat_read_file_at(dir_cluster, leaf, buf, sizeof(buf), &got)) {
        shell_write("hexdump: cannot read '"); shell_write(arg1); shell_write_line("'");
        return;
    }

    uint32_t off = 0;
    size_t pos = 0;
    while (pos < got) {
        size_t chunk = (got - pos) > 16 ? 16 : (got - pos);
        print_hex_line(off, buf + pos, chunk);
        pos += chunk;
        off += (uint32_t)chunk;
    }
}

const command_t CMD_HEXDUMP = {
    .name = "hexdump",
    .help = "hexdump: display file bytes in hex",
    .handler = hexdump_handler
};
