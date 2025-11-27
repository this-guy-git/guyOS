#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void mkuserdir_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    if (!arg1 || !arg1[0]) {
        shell_write_line("mkuserdir: usage: mkuserdir <username>");
        return;
    }

    // Use public shell API wrapper so we don't rely on static internals.
    shell_ensure_user_dirs(arg1);
    shell_write("mkuserdir: ensure_user_dirs called for '"); shell_write(arg1); shell_write_line("'");
}

const command_t CMD_MKUSERDIR = {
    .name = "mkuserdir",
    .help = "mkuserdir <user>: ensure /usr/<user>/ and subfolders exist (debug)",
    .handler = mkuserdir_handler
};
