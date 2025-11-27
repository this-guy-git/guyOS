#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/fat.h"
#include <stddef.h>

static void print_entry(const char *name, bool is_dir, uint32_t size) {
    // Show name as passed from fat_list_dir (already case-correct)
    if (is_dir) {
        shell_write(name);
        shell_write_line("/");
    } else {
        shell_write(name);
        shell_write("    "); // a few spaces between name and size
        char buf[16];
        int idx = 0;
        uint32_t s = size;
        if (s == 0) buf[idx++] = '0';
        else {
            char tmp[16];
            int t = 0;
            while (s > 0 && t < 16) {
                tmp[t++] = '0' + (s % 10);
                s /= 10;
            }
            while (t > 0) buf[idx++] = tmp[--t];
        }
        buf[idx] = 0;
        shell_write(buf);
        shell_write_line("");
    }
}

static void print_entry_verbose(const dirent_t *ent) {
    char raw[12];
    for (int i = 0; i < 11; i++) raw[i] = ent->name[i];
    raw[11] = 0;
    char human[32]; name_from_83(ent, human);
    // print ntres in hex (two nybbles)
    char ntbuf[8];
    const char *hex = "0123456789ABCDEF";
    ntbuf[0] = hex[(ent->ntres >> 4) & 0xF];
    ntbuf[1] = hex[ent->ntres & 0xF];
    ntbuf[2] = 0;
    // attributes and first cluster
    char attrbuf[8]; attrbuf[0] = "0123456789ABCDEF"[(ent->attr >> 4) & 0xF]; attrbuf[1] = "0123456789ABCDEF"[ent->attr & 0xF]; attrbuf[2] = 0;
    uint32_t cl = ((uint32_t)ent->first_cluster_hi << 16) | ent->first_cluster_lo;
    // cluster to decimal
    char clbuf[12]; int ci = 0; if (cl == 0) { clbuf[ci++] = '0'; } else { uint32_t t=cl; char r[12]; int ri=0; while (t>0) { r[ri++]= '0'+(t%10); t/=10; } while (ri>0) clbuf[ci++]=r[--ri]; } clbuf[ci]=0;
    // size
    char szbuf[12]; int si = 0; uint32_t s = ent->size; if (s==0) { szbuf[si++]='0'; } else { uint32_t t = s; char r[12]; int ri=0; while (t>0) { r[ri++]='0'+(t%10); t/=10;} while (ri>0) szbuf[si++]=r[--ri]; } szbuf[si]=0;

    shell_write("raw=\""); shell_write(raw); shell_write("\" ntres="); shell_write(ntbuf);
    shell_write(" attr=0x"); shell_write(attrbuf);
    shell_write(" cl="); shell_write(clbuf);
    shell_write(" size="); shell_write_line(szbuf);
    shell_write("    human=\""); shell_write(human); shell_write_line("\"");
}

static void ls_handler(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;
    
    bool verbose = false;
    const char *target = 0;
    if (arg1 && arg1[0] == '-' && arg1[1] == 'v') {
        verbose = true;
        target = (arg2 && arg2[0]) ? arg2 : 0;
    } else {
        target = (arg1 && arg1[0]) ? arg1 : 0;
    }
    uint32_t start = shell_current_dir_cluster();
    uint32_t dir_cluster = start;
    bool is_dir = true;

    if (target) {
        uint32_t base = (target[0] == '/') ? fat_get_root_cluster() : start;
        if (!fat_resolve_path(base, target, &dir_cluster, &is_dir)) {
            shell_write_line("ls: no such file or directory");
            return;
        }
        if (!is_dir) {
            shell_write_line("ls: not a directory");
            return;
        }
    }

    // support verbose mode ls -v
    if (verbose) {
        if (!fat_list_dir_verbose(dir_cluster, print_entry_verbose)) shell_write_line("ls: cannot access directory");
        return;
    }
    if (!fat_list_dir(dir_cluster, print_entry)) shell_write_line("ls: cannot access directory");
}

const command_t CMD_LS = {
    .name = "ls",
    .help = "ls: list directory contents",
    .handler = ls_handler
};
