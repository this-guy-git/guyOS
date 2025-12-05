#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/ver.h"

static void cmd_version_run(const char *arg1, const char *arg2) {
    (void)arg1; (void)arg2;
    shell_write("guyOS ");
    shell_write_line(version);
    shell_write("gsh ");
    shell_write_line(shellver);
    shell_write("gkern ");
    shell_write_line(kernelver);
    shell_write("gscript ");
    shell_write_line(gs_ver);
}

const command_t CMD_VERSION = {
    .name = "version",
    .handler = cmd_version_run,
    .help = "version: show OS version",
};
