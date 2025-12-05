#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef struct {
    char magic[4];
    u16 version;
    u16 reserved;
    u32 code_size;
    u32 string_size;
} __attribute__((packed)) gxe_header_t;

static size_t slen(const char *s) { size_t n = 0; while (s && s[n]) n++; return n; }
static void swrite(const char *a, const char *b) {
    if (a) shell_write(a);
    if (b) shell_write(b);
}

static bool resolve_parent(const char *arg, uint32_t *out_dir, const char **out_name) {
    static char path[128];
    size_t i = 0;
    if (!arg || !arg[0]) return false;
    while (arg[i] && i + 1 < sizeof(path)) { path[i] = arg[i]; i++; }
    path[i] = 0;
    int last = -1;
    for (int j = 0; path[j]; j++) if (path[j] == '/') last = j;
    uint32_t start = (path[0] == '/') ? fat_get_root_cluster() : shell_current_dir_cluster();
    if (last < 0) {
        *out_dir = start;
        *out_name = path;
        return slen(path) > 0;
    }
    path[last] = 0;
    const char *name = path + last + 1;
    bool is_dir = false;
    uint32_t dir = 0;
    if (!fat_resolve_path(start, path, &dir, &is_dir) || !is_dir) return false;
    *out_dir = dir;
    *out_name = name;
    return slen(name) > 0;
}

static bool build_str_table(const u8 *base, u32 size, const char **out_list, u16 *out_count) {
    u16 cnt = 0;
    u32 off = 0;
    while (off < size) {
        if (cnt >= 256) return false;
        const char *s = (const char *)(base + off);
        u32 len = 0;
        while (off + len < size && base[off + len] != 0) len++;
        if (off + len >= size) return false; // missing null
        out_list[cnt++] = s;
        off += len + 1;
    }
    *out_count = cnt;
    return true;
}

static size_t str_copy(char *dst, size_t dst_sz, const char *src) {
    size_t i = 0;
    if (!dst || dst_sz == 0) return 0;
    if (!src) { dst[0] = 0; return 0; }
    while (src[i] && i + 1 < dst_sz) { dst[i] = src[i]; i++; }
    dst[i] = 0;
    return i;
}

static void gxe_exec(const u8 *code, u32 code_size, const char **strs, u16 str_count, uint32_t cwd_cluster) {
    u32 ip = 0;
    static char last_input[128];
    static char slots[16][128];
    last_input[0] = 0;
    for (int i = 0; i < 16; i++) slots[i][0] = 0;
    while (ip < code_size) {
        u8 op = code[ip++];
        switch (op) {
            case 0x01: // PRINT
            case 0x02: { // PRINTLN
                if (ip + 2 > code_size) return;
                u16 idx = (u16)(code[ip] | (code[ip + 1] << 8));
                ip += 2;
                if (idx >= str_count) return;
                if (op == 0x01) shell_write(strs[idx]);
                else shell_write_line(strs[idx]);
                break;
            }
            case 0x03: // CLEAR
                shell_clear_body();
                shell_redraw();
                break;
            case 0x04: { // TITLE
                if (ip + 2 > code_size) return;
                u16 idx = (u16)(code[ip] | (code[ip + 1] << 8));
                ip += 2;
                if (idx >= str_count) return;
                shell_set_title(strs[idx]);
                shell_redraw();
                break;
            }
            case 0x05: { // INPUT prompt -> stores into last_input
                if (ip + 2 > code_size) return;
                u16 idx = (u16)(code[ip] | (code[ip + 1] << 8));
                ip += 2;
                const char *prompt = (idx < str_count) ? strs[idx] : "";
                shell_read_line(last_input, sizeof(last_input), true, prompt, 0);
                break;
            }
            case 0x06: // PRINT_LAST (no newline)
                shell_write(last_input);
                break;
            case 0x07: // PRINTLN_LAST
                shell_write_line(last_input);
                break;
            case 0x08: { // STORE slot
                if (ip >= code_size) return;
                u8 slot = code[ip++];
                if (slot < 16) {
                    size_t i = 0;
                    while (last_input[i] && i + 1 < sizeof(slots[0])) { slots[slot][i] = last_input[i]; i++; }
                    slots[slot][i] = 0;
                }
                break;
            }
            case 0x09: { // LOAD_PRINT slot
                if (ip >= code_size) return;
                u8 slot = code[ip++];
                if (slot < 16) shell_write(slots[slot]);
                break;
            }
            case 0x0A: { // LOAD_PRINTLN slot
                if (ip >= code_size) return;
                u8 slot = code[ip++];
                if (slot < 16) shell_write_line(slots[slot]);
                break;
            }
            case 0x0B: { // SLOT_SET slot, str_idx
                if (ip + 3 > code_size) return;
                u8 slot = code[ip++];
                u16 idx = (u16)(code[ip] | (code[ip + 1] << 8));
                ip += 2;
                if (slot < 16 && idx < str_count) str_copy(slots[slot], sizeof(slots[0]), strs[idx]);
                break;
            }
            case 0x10: { // LOAD_FILE path_idx
                if (ip + 2 > code_size) return;
                u16 idx = (u16)(code[ip] | (code[ip + 1] << 8));
                ip += 2;
                if (idx >= str_count) return;
                const char *path = strs[idx];
                uint8_t fbuf[4096];
                size_t got = 0;
                uint32_t file_cl = 0; bool is_dir = false;
                if (!fat_resolve_path(cwd_cluster, path, &file_cl, &is_dir) || is_dir) {
                    // clear slots on failure
                    for (int s = 0; s < 16; s++) slots[s][0] = 0;
                    break;
                }
                if (!fat_read_file_at(cwd_cluster, path, fbuf, sizeof(fbuf) - 1, &got)) {
                    for (int s = 0; s < 16; s++) slots[s][0] = 0;
                    break;
                }
                fbuf[got] = 0;
                // split into lines
                size_t si = 0;
                size_t li = 0;
                for (size_t i = 0; i <= got && li < 16; i++) {
                    if (fbuf[i] == '\n' || fbuf[i] == 0) {
                        fbuf[i] = 0;
                        str_copy(slots[li], sizeof(slots[0]), (char *)fbuf + si);
                        li++;
                        si = i + 1;
                    }
                }
                break;
            }
            case 0x11: { // SAVE_FILE path_idx, count
                if (ip + 3 > code_size) return;
                u16 idx = (u16)(code[ip] | (code[ip + 1] << 8));
                ip += 2;
                u8 cnt = code[ip++];
                if (idx >= str_count) return;
                const char *path = strs[idx];
                // join slots into buffer
                uint8_t out[4096];
                size_t w = 0;
                for (u8 s = 0; s < cnt && s < 16; s++) {
                    const char *line = slots[s];
                    size_t j = 0;
                    while (line && line[j] && w + 1 < sizeof(out)) out[w++] = (uint8_t)line[j++];
                    if (w + 1 < sizeof(out)) out[w++] = '\n';
                }
                size_t len = w;
                uint32_t dir = 0; bool is_dir = false;
                if (!fat_resolve_path(cwd_cluster, ".", &dir, &is_dir)) dir = cwd_cluster;
                fat_write_file_at(dir, path, out, len);
                break;
            }
            case 0xFF: // EXIT
                return;
            default:
                return;
        }
    }
}

static void cmd_gxe_run(const char *arg1, const char *arg2) {
    (void)arg2;
    if (!arg1 || !arg1[0]) {
        shell_write_line("usage: gxe <file.gxe>");
        return;
    }

    uint32_t dir = 0; const char *name = 0;
    if (!resolve_parent(arg1, &dir, &name)) {
        shell_write_line("gxe: invalid path");
        return;
    }

    uint8_t buf[8192];
    size_t got = 0;
    if (!fat_read_file_at(dir, name, buf, sizeof(buf), &got)) {
        shell_write_line("gxe: failed to read file");
        return;
    }
    if (got < sizeof(gxe_header_t)) {
        shell_write_line("gxe: file too small");
        return;
    }

    gxe_header_t h;
    const u8 *p = buf;
    for (size_t i = 0; i < sizeof(h); i++) ((u8 *)&h)[i] = p[i];
    if (h.magic[0] != 'G' || h.magic[1] != 'X' || h.magic[2] != 'E' || h.magic[3] != 0) {
        shell_write_line("gxe: bad magic");
        return;
    }
    if (h.version != 1) {
        shell_write_line("gxe: unsupported version");
        return;
    }
    u32 need = (u32)sizeof(gxe_header_t) + h.code_size + h.string_size;
    if (need > got) {
        shell_write_line("gxe: truncated file");
        return;
    }

    const u8 *code = buf + sizeof(gxe_header_t);
    const u8 *str_base = code + h.code_size;
    const char *strs[256];
    u16 str_count = 0;
    if (!build_str_table(str_base, h.string_size, strs, &str_count)) {
        shell_write_line("gxe: bad string table");
        return;
    }

    gxe_exec(code, h.code_size, strs, str_count, shell_current_dir_cluster());
}

const command_t CMD_GXE = {
    .name = "gxe",
    .help = "gxe <file>: run a .gxe app",
    .handler = cmd_gxe_run
};
