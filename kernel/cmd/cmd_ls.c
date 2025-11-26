#include "../../include/commands.h"
#include "../../include/shell_api.h"

static const char *fs_layout[] = {
    "/",
    "/usr",
    "/usr/<user>",
    "/usr/<user>/downloads",
    "/usr/<user>/documents",
    "/shell",
    "/shell/cmd",
    "/vital",
    "/vital/kernel",
    "/vital/bootloader",
    0
};

static void cmd_ls_run(const char *arg1, const char *arg2) {
    for (const char **p = fs_layout; *p; p++) {
        shell_write_line(*p);
    }
}

const command_t CMD_LS = {
    .name = "ls",
    .handler = cmd_ls_run,
    .help = "ls: list virtual filesystem layout",
};
