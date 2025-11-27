#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void fixuserdirs_handler(const char *arg1, const char *arg2) {
    (void)arg1; (void)arg2;
    shell_write_line("fixuserdirs: scanning accounts and ensuring user dirs...");
    // shell provides shell_ensure_user_dirs
    extern void shell_ensure_user_dirs(const char *username);
    // We'll try each known user via shell_list_users (internal access)
    // But include a simple approach: call shell_ensure_user_dirs for all accounts up to MAX_USERS
    // We can't access accounts[] here directly, so rely on shell API to list users then try creating.
    // For now, accept a specific username as arg1
    if (arg1 && arg1[0]) {
        shell_write("ensuring user dirs for: "); shell_write_line(arg1);
        shell_ensure_user_dirs(arg1);
        shell_write_line("done");
        return;
    }
    shell_write_line("usage: fixuserdirs <username>\n(helps if adduser failed earlier)");
}

const command_t CMD_FIXUSERDIRS = {
    .name = "fixuserdirs",
    .help = "fixuserdirs <user>: ensure /usr/<user>/ and subfolders exist (debug)",
    .handler = fixuserdirs_handler
};
