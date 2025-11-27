#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "path_utils.h"

static void touch_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    
    if (!arg1 || arg1[0] == 0) {
        shell_write_line("touch: missing file operand");
        shell_write_line("Usage: touch FILE");
        return;
    }

    uint32_t dir_cluster = 0;
    const char *leaf = 0;
    if (!split_path_parent(arg1, shell_current_dir_cluster(), &dir_cluster, &leaf)) {
        shell_write("touch: invalid path '");
        shell_write(arg1);
        shell_write_line("'");
        return;
    }

    uint8_t empty_data[1] = {0};

    for (int attempt = 0; attempt < 3; attempt++) {
        if (fat_write_file_at(dir_cluster, leaf, empty_data, 0)) {
            shell_write("touch: created file '");
            shell_write(arg1);
            shell_write_line("'");
            return;
        }
    }
    shell_write("touch: cannot create file '");
    shell_write(arg1);
    shell_write_line("'");
}

const command_t CMD_TOUCH = {
    .name = "touch",
    .help = "touch: create an empty file",
    .handler = touch_handler
};
