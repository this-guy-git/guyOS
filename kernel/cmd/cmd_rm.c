#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "path_utils.h"

static void rm_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    if (!arg1 || !arg1[0]) {
        shell_write_line("rm: missing operand");
        shell_write_line("Usage: rm FILE");
        return;
    }
    uint32_t dir_cluster = 0;
    const char *leaf = 0;
    if (!split_path_parent(arg1, shell_current_dir_cluster(), &dir_cluster, &leaf)) {
        shell_write("rm: invalid path '");
        shell_write(arg1);
        shell_write_line("'");
        return;
    }
    if (fat_delete_file_at(dir_cluster, leaf)) {
        shell_write("rm: removed '");
        shell_write(arg1);
        shell_write_line("'");
    } else {
        shell_write("rm: failed to remove '");
        shell_write(arg1);
        shell_write_line("'");
    }
}

const command_t CMD_RM = {
    .name = "rm",
    .help = "rm: remove a file",
    .handler = rm_handler
};
