#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_load_run(const char *arg1, const char *arg2) {
    if (shell_load_accounts()) {
        shell_write_line("accounts reloaded from disk.");
    } else {
        shell_write_line("load failed.");
    }
}

const command_t CMD_LOAD = {
    .name = "load",
    .handler = cmd_load_run,
    .help = "load: reload accounts",
};
