#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_clear_run(const char *arg1, const char *arg2) {
    shell_redraw();
}

const command_t CMD_CLEAR = {
    .name = "clear",
    .handler = cmd_clear_run,
    .help = "clear: redraw the screen (F5 also clears)",
};
