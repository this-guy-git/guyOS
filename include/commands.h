#pragma once
#include <stddef.h>

typedef struct {
    const char *name;
    void (*handler)(const char *arg1, const char *arg2);
    const char *help;
} command_t;

const command_t *const *commands_get_list(size_t *count);
#include <stddef.h>
