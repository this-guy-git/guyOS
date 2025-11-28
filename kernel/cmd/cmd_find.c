#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ATTR_DIRECTORY 0x10

static size_t str_len(const char *s) { size_t n=0; while (s && s[n]) n++; return n; }
static bool str_contains(const char *hay, const char *needle) {
    if (!hay || !needle || !*needle) return false;
    size_t hlen = str_len(hay), nlen = str_len(needle);
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        while (j < nlen && hay[i+j] == needle[j]) j++;
        if (j == nlen) return true;
    }
    return false;
}

static const char *g_needle = 0;
static char g_path[256];
static size_t g_base_len = 0;

static void find_walk(uint32_t dir_cluster, size_t path_len);

static void find_cb(const dirent_t *ent) {
    if (!ent) return;
    char name[64];
    name_from_83(ent, name);
    // skip dots
    if (name[0] == '.') return;

    size_t base = g_base_len;
    size_t idx = base;
    if (idx && idx < sizeof(g_path)-1 && g_path[idx-1] != '/') g_path[idx++] = '/';
    size_t nlen = str_len(name);
    if (idx + nlen >= sizeof(g_path)) return;
    for (size_t i = 0; i < nlen; i++) g_path[idx + i] = name[i];
    g_path[idx + nlen] = 0;

    if (str_contains(name, g_needle)) {
        shell_write_line(g_path);
    }

    if (ent->attr & 0x10) { // directory
        uint32_t child = ((uint32_t)ent->first_cluster_hi << 16) | ent->first_cluster_lo;
        find_walk(child, idx + nlen);
    }
}

static void find_walk(uint32_t dir_cluster, size_t path_len) {
    g_base_len = path_len;
    fat_list_dir_verbose(dir_cluster, find_cb);
}

static void find_handler(const char *arg1, const char *arg2) {
    (void)arg2;
    if (!arg1 || !arg1[0]) {
        shell_write_line("find: usage: find <name-substring>");
        return;
    }
    g_needle = arg1;
    // start from cwd
    size_t l = 0;
    const char *cwd = shell_get_cwd();
    while (cwd[l] && l + 1 < sizeof(g_path)) { g_path[l] = cwd[l]; l++; }
    g_path[l] = 0;
    uint32_t start = shell_current_dir_cluster();
    find_walk(start, l);
}

const command_t CMD_FIND = {
    .name = "find",
    .help = "find: search for names under cwd (substring match)",
    .handler = find_handler
};
