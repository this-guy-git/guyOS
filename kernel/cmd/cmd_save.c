#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_save_run(const char *arg1, const char *arg2) {
    if (shell_save_accounts()) {
        shell_write_line("accounts saved to disk.");
    } else {
        shell_write_line("save failed.");
    }
}

const command_t CMD_SAVE = {
    .name = "save",
    .handler = cmd_save_run,
    .help = "save: persist accounts",
};
