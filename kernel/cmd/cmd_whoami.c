#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_whoami_run(const char *arg1, const char *arg2) {
    shell_write("you are ");
    shell_write_line(shell_current_user());
}

const command_t CMD_WHOAMI = {
    .name = "whoami",
    .handler = cmd_whoami_run,
    .help = "whoami: show current user",
};
