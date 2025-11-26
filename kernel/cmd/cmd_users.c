#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cmd_users_run(const char *arg1, const char *arg2) {
    shell_list_users();
}

const command_t CMD_USERS = {
    .name = "users",
    .handler = cmd_users_run,
    .help = "users: list users",
};
