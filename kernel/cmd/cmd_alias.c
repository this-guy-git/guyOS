#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "path_utils.h"
#include <stddef.h>

#define MAX_ALIASES 16

static shell_alias_t aliases[MAX_ALIASES];
static size_t alias_count = 0;

// Expose a getter so shell can resolve aliases
const shell_alias_t *shell_aliases(size_t *count) {
    if (count) *count = alias_count;
    return aliases;
}

static void alias_load_file(void) {
    alias_count = 0;
    uint32_t sys = 0; bool is_dir = false;
    if (!fat_resolve_path(fat_get_root_cluster(), "/sys", &sys, &is_dir) || !is_dir) return;
    uint8_t buf[512];
    size_t got = 0;
    if (!fat_read_file_at(sys, "alias", buf, sizeof(buf), &got)) return;
    buf[got] = 0;
    size_t i = 0;
    while (i < got && alias_count < MAX_ALIASES) {
        char real[16] = {0}, alias[16] = {0};
        size_t r = 0, a = 0;
        while (i < got && buf[i] != '=' && buf[i] != '\n' && r + 1 < sizeof(real)) real[r++] = (char)buf[i++];
        if (i >= got || buf[i] != '=') { while (i < got && buf[i] != '\n') i++; if (i<got) i++; continue; }
        i++; // skip '='
        while (i < got && buf[i] != '\n' && a + 1 < sizeof(alias)) alias[a++] = (char)buf[i++];
        if (i < got && buf[i] == '\n') i++;
        if (r > 0 && a > 0) {
            for (size_t k = 0; k < r+1; k++) aliases[alias_count].real[k] = real[k];
            for (size_t k = 0; k < a+1; k++) aliases[alias_count].alias[k] = alias[k];
            alias_count++;
        }
    }
}

static void alias_save_file(void) {
    uint32_t root = fat_get_root_cluster();
    uint32_t sys = fat_ensure_dir_at(root, "sys");
    if (sys == 0) return;
    char out[512];
    size_t w = 0;
    for (size_t i = 0; i < alias_count; i++) {
        const shell_alias_t *al = &aliases[i];
        for (size_t j = 0; al->real[j] && w + 1 < sizeof(out); j++) out[w++] = al->real[j];
        if (w + 1 < sizeof(out)) out[w++] = '=';
        for (size_t j = 0; al->alias[j] && w + 1 < sizeof(out); j++) out[w++] = al->alias[j];
        if (w + 1 < sizeof(out)) out[w++] = '\n';
    }
    fat_write_file_at(sys, "alias", (const uint8_t *)out, w);
}

static void alias_add(const char *real, const char *alias) {
    if (!real || !alias || !real[0] || !alias[0]) return;
    // overwrite if alias exists
    for (size_t i = 0; i < alias_count; i++) {
        if (aliases[i].alias[0] && aliases[i].alias[0] == alias[0]) {
            size_t j = 0; while (real[j] && j < sizeof(aliases[i].real)-1) { aliases[i].real[j]=real[j]; j++; } aliases[i].real[j]=0;
            return;
        }
    }
    if (alias_count >= MAX_ALIASES) return;
    size_t j = 0; while (real[j] && j < sizeof(aliases[alias_count].real)-1) { aliases[alias_count].real[j]=real[j]; j++; } aliases[alias_count].real[j]=0;
    j = 0; while (alias[j] && j < sizeof(aliases[alias_count].alias)-1) { aliases[alias_count].alias[j]=alias[j]; j++; } aliases[alias_count].alias[j]=0;
    alias_count++;
}

static void alias_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    if (!arg1 || !arg1[0]) {
        // list aliases
        for (size_t i = 0; i < alias_count; i++) {
            shell_write(aliases[i].real);
            shell_write("=");
            shell_write_line(aliases[i].alias);
        }
        return;
    }
    // parse "real=alias"
    const char *eq = arg1;
    while (*eq && *eq != '=') eq++;
    if (*eq != '=') {
        shell_write_line("alias: usage: alias real=alias");
        return;
    }
    char real[16], alias[16];
    size_t r = 0, a = 0;
    const char *p = arg1;
    while (p < eq && r + 1 < sizeof(real)) real[r++] = *p++;
    real[r] = 0;
    p = eq + 1;
    while (*p && a + 1 < sizeof(alias)) alias[a++] = *p++;
    alias[a] = 0;
    alias_add(real, alias);
    alias_save_file();
    shell_write("alias: mapped ");
    shell_write(alias);
    shell_write(" -> ");
    shell_write_line(real);
}

const command_t CMD_ALIAS = {
    .name = "alias",
    .help = "alias: define or list command aliases (real=alias)",
    .handler = alias_handler
};

// Expose loader for shell startup
void shell_alias_load(void) {
    alias_load_file();
}
