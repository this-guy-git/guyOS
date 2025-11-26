#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_time_run(const char *arg1, const char *arg2) {
    shell_write_line("time: not yet implemented (needs RTC/u64 ticks)");
}

const command_t CMD_TIME = {
    .name = "time",
    .handler = cmd_time_run,
    .help = "time: (stub) show uptime",
};
