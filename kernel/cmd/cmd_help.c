#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_help_run(const char *arg1, const char *arg2) {
    size_t n = 0;
    const command_t *const *cmds = commands_get_list(&n);
    shell_write_line("available commands:");
    for (size_t i = 0; i < n; i++) {
        shell_write("- ");
        shell_write_line(cmds[i]->help);
    }
}

const command_t CMD_HELP = {
    .name = "help",
    .handler = cmd_help_run,
    .help = "help: list commands from shell/cmd/*.gxe",
};
