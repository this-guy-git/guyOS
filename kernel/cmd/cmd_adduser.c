#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_adduser_run(const char *arg1, const char *arg2) {
    if (arg1 && *arg1 && arg2 && *arg2) {
        if (shell_add_user(arg1, arg2)) shell_write_line("user added.");
        else shell_write_line("failed to add user (exists or full).");
    } else {
        shell_write_line("usage: adduser <name> <pass>");
    }
}

const command_t CMD_ADDUSER = {
    .name = "adduser",
    .handler = cmd_adduser_run,
    .help = "adduser <name> <pass>: add user",
};
