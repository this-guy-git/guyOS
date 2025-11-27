#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_version_run(const char *arg1, const char *arg2) {
    shell_write_line("guyOS kernel shell v0.0.3");
}

const command_t CMD_VERSION = {
    .name = "version",
    .handler = cmd_version_run,
    .help = "version: show OS version",
};
