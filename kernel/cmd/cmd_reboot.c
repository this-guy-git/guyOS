#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_reboot_run(const char *arg1, const char *arg2) {
    shell_write_line("rebooting...");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

const command_t CMD_REBOOT = {
    .name = "reboot",
    .handler = cmd_reboot_run,
    .help = "reboot: halt CPU (stub for reset)",
};
