#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_echo_run(const char *arg1, const char *arg2) {
    if (arg1) shell_write_line(arg1);
}

const command_t CMD_ECHO = {
    .name = "echo",
    .handler = cmd_echo_run,
    .help = "echo <text>: print text",
};
