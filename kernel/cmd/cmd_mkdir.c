#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "path_utils.h"

static void mkdir_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    
    if (!arg1 || arg1[0] == 0) {
        shell_write_line("mkdir: missing operand");
        shell_write_line("Usage: mkdir DIRECTORY");
        return;
    }

    uint32_t parent_cluster = 0;
    const char *leaf = 0;
    if (!split_path_parent(arg1, shell_current_dir_cluster(), &parent_cluster, &leaf)) {
        shell_write("mkdir: invalid path '");
        shell_write(arg1);
        shell_write_line("'");
        return;
    }

    uint32_t new_cluster = 0;
    if (fat_mkdir_at(parent_cluster, leaf, &new_cluster)) {
        shell_write("mkdir: created directory '");
        shell_write(arg1);
        shell_write_line("'");
    } else {
        shell_write("mkdir: cannot create directory '");
        shell_write(arg1);
        shell_write_line("': File exists or disk full");
    }
}

const command_t CMD_MKDIR = {
    .name = "mkdir",
    .help = "mkdir: create a directory",
    .handler = mkdir_handler
};
