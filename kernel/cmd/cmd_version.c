#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/ver.h"

static void cmd_version_run(const char *arg1, const char *arg2) {
    (void)arg1; (void)arg2;
    shell_write("guyOS kernel shell ");
    shell_write_line(version);
}

const command_t CMD_VERSION = {
    .name = "version",
    .handler = cmd_version_run,
    .help = "version: show OS version",
};
