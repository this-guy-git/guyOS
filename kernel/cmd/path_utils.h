#ifndef CMD_PATH_UTILS_H
#define CMD_PATH_UTILS_H

#include "../../include/fat.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Resolve a path into its parent directory cluster and leaf name.
// - Uses cwd_cluster for relative paths, root for absolute.
// - Returns false if the path is empty or the parent cannot be resolved.
static inline bool split_path_parent(const char *path, uint32_t cwd_cluster, uint32_t *parent_out, const char **leaf_out) {
    if (!path || !path[0]) return false;

    const char *last_slash = 0;
    for (const char *p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }

    uint32_t base = (path[0] == '/') ? fat_get_root_cluster() : cwd_cluster;

    // No slash: parent is base, leaf is whole path
    if (!last_slash) {
        if (parent_out) *parent_out = base;
        if (leaf_out) *leaf_out = path;
        return true;
    }

    const char *leaf = last_slash + 1;
    if (!leaf || leaf[0] == 0) return false; // path ends with slash -> invalid for file/dir creation

    // Parent is root if slash is the first character
    if (last_slash == path && path[0] == '/') {
        if (parent_out) *parent_out = fat_get_root_cluster();
        if (leaf_out) *leaf_out = leaf;
        return true;
    }

    // Copy parent substring and resolve it
    char parent_path[128];
    size_t len = (size_t)(last_slash - path);
    if (len >= sizeof(parent_path)) len = sizeof(parent_path) - 1;
    for (size_t i = 0; i < len; i++) parent_path[i] = path[i];
    parent_path[len] = 0;

    uint32_t dir_cluster = base;
    bool is_dir = false;
    if (len > 0) {
        if (!fat_resolve_path(base, parent_path, &dir_cluster, &is_dir) || !is_dir) return false;
    }
    if (parent_out) *parent_out = dir_cluster;
    if (leaf_out) *leaf_out = leaf;
    return true;
}

#endif // CMD_PATH_UTILS_H
