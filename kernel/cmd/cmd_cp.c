#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include "path_utils.h"
#include <stddef.h>
#include <stdint.h>

static void cp_handler(const char *src, const char *dst) {
    if (!src || !src[0] || !dst || !dst[0]) {
        shell_write_line("cp: usage: cp <src> <dst>");
        return;
    }

    uint32_t src_dir = 0;
    const char *src_leaf = 0;
    if (!split_path_parent(src, shell_current_dir_cluster(), &src_dir, &src_leaf)) {
        shell_write("cp: invalid source '"); shell_write(src); shell_write_line("'");
        return;
    }

    // Resolve destination
    uint32_t dst_dir = 0;
    const char *dst_leaf = 0;
    bool dst_is_dir = false;
    uint32_t base = shell_current_dir_cluster();
    uint32_t dst_cl = 0; bool dst_dirflag = false;
    if (fat_resolve_path((dst[0]=='/')?fat_get_root_cluster():base, dst, &dst_cl, &dst_dirflag)) {
        if (dst_dirflag) {
            // copy into dir, use source leaf
            dst_dir = dst_cl;
            dst_leaf = src_leaf;
        } else {
            // overwrite existing file
            dst_dir = shell_current_dir_cluster();
            // use provided path's leaf
            split_path_parent(dst, shell_current_dir_cluster(), &dst_dir, &dst_leaf);
        }
    } else {
        // destination doesn't exist: split path
        if (!split_path_parent(dst, shell_current_dir_cluster(), &dst_dir, &dst_leaf)) {
            shell_write("cp: invalid destination '"); shell_write(dst); shell_write_line("'");
            return;
        }
    }

    // Read source (buffer-limited)
    uint8_t buf[2048];
    size_t read_bytes = 0;
    if (!fat_read_file_at(src_dir, src_leaf, buf, sizeof(buf), &read_bytes)) {
        shell_write("cp: cannot read '"); shell_write(src); shell_write_line("'");
        return;
    }
    if (read_bytes >= sizeof(buf)) {
        shell_write_line("cp: source too large for current buffer (limit 2KB)");
        return;
    }

    if (fat_write_file_at(dst_dir, dst_leaf, buf, read_bytes)) {
        shell_write("cp: copied ");
        shell_write(src_leaf);
        shell_write(" -> ");
        shell_write(dst_leaf);
        shell_write_line("");
    } else {
        shell_write("cp: failed to write '"); shell_write(dst_leaf); shell_write_line("'");
    }
}

const command_t CMD_CP = {
    .name = "cp",
    .help = "cp: copy a file (limited to small files)",
    .handler = cp_handler
};
