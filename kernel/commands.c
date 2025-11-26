#include <stddef.h>
#include "../include/commands.h"

extern const command_t CMD_HELP;
extern const command_t CMD_CLEAR;
extern const command_t CMD_WHOAMI;
extern const command_t CMD_USERS;
extern const command_t CMD_ADDUSER;
extern const command_t CMD_ECHO;
extern const command_t CMD_TIME;
extern const command_t CMD_LS;
extern const command_t CMD_PWD;
extern const command_t CMD_VERSION;
extern const command_t CMD_REBOOT;
extern const command_t CMD_SAVE;
extern const command_t CMD_LOAD;

static const command_t *command_table[] = {
    &CMD_HELP,
    &CMD_CLEAR,
    &CMD_WHOAMI,
    &CMD_USERS,
    &CMD_ADDUSER,
    &CMD_ECHO,
    &CMD_TIME,
    &CMD_LS,
    &CMD_PWD,
    &CMD_VERSION,
    &CMD_REBOOT,
    &CMD_SAVE,
    &CMD_LOAD,
};

const command_t *const *commands_get_list(size_t *count) {
    if (count) *count = sizeof(command_table) / sizeof(command_table[0]);
    return command_table;
}
