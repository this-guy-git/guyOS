#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_pwd_run(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;
    shell_write_line(shell_get_cwd());
}

const command_t CMD_PWD = {
    .name = "pwd",
    .handler = cmd_pwd_run,
    .help = "pwd: print working directory",
};