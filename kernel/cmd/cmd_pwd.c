#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_pwd_run(const char *arg1, const char *arg2) {
    shell_write_line("/");
}

const command_t CMD_PWD = {
    .name = "pwd",
    .handler = cmd_pwd_run,
    .help = "pwd: show current path (root)",
};
