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
extern const command_t CMD_MKDIR;
extern const command_t CMD_CD;
extern const command_t CMD_FSTEST;
extern const command_t CMD_PWD;
extern const command_t CMD_VERSION;
extern const command_t CMD_REBOOT;
extern const command_t CMD_MKUSERDIR;
extern const command_t CMD_FIXUSERDIRS;
extern const command_t CMD_TOUCH;
extern const command_t CMD_CAT;
extern const command_t CMD_TEDIT;
extern const command_t CMD_HEXDUMP;
extern const command_t CMD_CP;
extern const command_t CMD_FIND;
extern const command_t CMD_RM;
extern const command_t CMD_ALIAS;
extern const command_t CMD_FETCH;
extern const command_t CMD_GXE;

static const command_t *command_table[] = {
    &CMD_HELP,
    &CMD_CLEAR,
    &CMD_WHOAMI,
    &CMD_USERS,
    &CMD_ADDUSER,
    &CMD_ECHO,
    &CMD_TIME,
    &CMD_LS,
    &CMD_MKDIR,
    &CMD_MKUSERDIR,
    &CMD_CD,
    &CMD_PWD,
    &CMD_TOUCH,
    &CMD_CAT,
    &CMD_TEDIT,
    &CMD_HEXDUMP,
    &CMD_CP,
    &CMD_FIND,
    &CMD_RM,
    &CMD_ALIAS,
    &CMD_FETCH,
    &CMD_GXE,
    &CMD_FSTEST,
    &CMD_FIXUSERDIRS,
    &CMD_VERSION,
    &CMD_REBOOT,
};

const command_t *const *commands_get_list(size_t *count) {
    if (count) *count = sizeof(command_table) / sizeof(command_table[0]);
    return command_table;
}
