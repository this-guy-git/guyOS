#include "../../include/commands.h"
#include "../../include/shell_api.h"

static void cd_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    
    if (!arg1 || arg1[0] == 0) {
        // cd with no args - go to home (root for now)
        if (!shell_chdir("/")) {
            shell_write_line("cd: failed to change to root");
        }
        return;
    }
    
    if (!shell_chdir(arg1)) {
        shell_write("cd: ");
        shell_write(arg1);
        shell_write_line(": No such directory");
    }
}

const command_t CMD_CD = {
    .name = "cd",
    .help = "cd: change directory",
    .handler = cd_handler
};
