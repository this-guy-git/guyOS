#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "path_utils.h"

static void cat_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    
    if (!arg1 || arg1[0] == 0) {
        shell_write_line("cat: missing file operand");
        shell_write_line("Usage: cat FILE");
        return;
    }
    
    uint32_t dir_cluster = 0;
    const char *leaf = 0;
    if (!split_path_parent(arg1, shell_current_dir_cluster(), &dir_cluster, &leaf)) {
        shell_write("cat: invalid path '");
        shell_write(arg1);
        shell_write_line("'");
        return;
    }

    uint8_t buffer[4096];
    size_t bytes_read = 0;
    
    if (fat_read_file_at(dir_cluster, leaf, buffer, sizeof(buffer), &bytes_read)) {
        // Print the file contents
        for (size_t i = 0; i < bytes_read; i++) {
            char c = (char)buffer[i];
            if (c == '\n') {
                shell_write_line("");
            } else if (c >= 32 && c < 127) {
                char str[2] = {c, 0};
                shell_write(str);
            }
        }
        shell_write_line("");
    } else {
        shell_write("cat: ");
        shell_write(arg1);
        shell_write_line(": No such file");
    }
}

const command_t CMD_CAT = {
    .name = "cat",
    .help = "cat: display file contents",
    .handler = cat_handler
};
