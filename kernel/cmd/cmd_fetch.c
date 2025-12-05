#include "../../include/commands.h"
#include "../../include/shell_api.h"
#include "../../include/hal.h"
#include "../../include/fat.h"
#include "../../include/ver.h"
#include "../../include/ascii.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static const char fetch_ver[] = "guyOS fetch v0.0.7";

static size_t str_len(const char *s) { size_t n = 0; while (s && s[n]) n++; return n; }
static void write_spaces(size_t n) {
    char buf[64];
    while (n) {
        size_t chunk = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
        for (size_t i = 0; i < chunk; i++) buf[i] = ' ';
        buf[chunk] = 0;
        shell_write(buf);
        n -= chunk;
    }
}

static void fill(char *dst, size_t max, const char *a, const char *b) {
    size_t i=0;
    while (a && a[i] && i+1<max) { dst[i]=a[i]; i++; }
    size_t j=0;
    while (b && b[j] && i+1<max) { dst[i++]=b[j++]; }
    dst[(i<max)?i:max-1] = 0;
}

static void hex4(uint16_t v, char out[5]) {
    for (int i = 0; i < 4; i++) {
        uint8_t nibble = (v >> ((3 - i) * 4)) & 0xF;
        out[i] = (char)(nibble < 10 ? '0' + nibble : 'A' + (nibble - 10));
    }
    out[4] = 0;
}

static const char *detect_gpu_name(void) {
    static char buf[64];
    static bool done = false;
    if (done) return buf[0] ? buf : "VGA (text)";

    const char *fallback = "VGA (text)";
    buf[0] = 0;

    for (uint8_t bus = 0; bus < 32; bus++) { // limit for speed; enough for most VMs
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t addr = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8);
                outl(0xCF8, addr);
                uint32_t id = inl(0xCFC);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);
                if (vendor == 0xFFFF) continue;

                outl(0xCF8, addr | 0x08); // class code at offset 0x08
                uint32_t classreg = inl(0xCFC);
                uint8_t class = (uint8_t)(classreg >> 24);
                if (class != 0x03) continue; // not a display controller

                const char *name = "GPU";
                if (vendor == 0x80EE) name = "VirtualBox/Bochs VGA";
                else if (vendor == 0x1234) name = "QEMU std VGA";
                else if (vendor == 0x8086) name = "Intel VGA";
                else if (vendor == 0x10DE) name = "NVIDIA GPU";
                else if (vendor == 0x1002) name = "AMD/ATI GPU";

                char vs[5], ds[5];
                hex4(vendor, vs); hex4(device, ds);
                size_t w = 0;
                const char *p = name;
                while (*p && w + 1 < sizeof(buf)) buf[w++] = *p++;
                if (w + 1 < sizeof(buf)) buf[w++] = ' ';
                if (w + 1 < sizeof(buf)) buf[w++] = '(';
                for (int i = 0; vs[i] && w + 1 < sizeof(buf); i++) buf[w++] = vs[i];
                if (w + 1 < sizeof(buf)) buf[w++] = ':';
                for (int i = 0; ds[i] && w + 1 < sizeof(buf); i++) buf[w++] = ds[i];
                if (w + 1 < sizeof(buf)) buf[w++] = ')';
                buf[w] = 0;
                done = true;
                return buf;
            }
        }
    }

    done = true;
    return fallback;
}

static void fetch_handler(const char *arg1, const char *arg2) {
    (void)arg1; (void)arg2;

    const char *user = shell_current_user();
    const char *cwd = shell_get_cwd();

    size_t cmd_count = 0;
    commands_get_list(&cmd_count);

    char numbuf[16]; int ni=0; size_t t=cmd_count; if (t==0) numbuf[ni++]='0'; else { char tmp[16]; int ti=0; while (t>0 && ti<16){ tmp[ti++]= '0'+(t%10); t/=10; } while (ti>0) numbuf[ni++]=tmp[--ti]; } numbuf[ni]=0;

static char line_user[64], line_shell[64], line_cwd[64], line_cmds[64], line_os[64], line_kernel[64];
static char line_cpu[64], line_mem[64], line_gpu[64];
static char line_gs[64];
    fill(line_user,   sizeof(line_user),   "User:      ", user ? user : "<none>");
    fill(line_shell,  sizeof(line_shell),  "Shell:     gsh ", shellver);
    fill(line_cwd,    sizeof(line_cwd),    "CWD:       ", cwd ? cwd : "/");
    fill(line_cmds,   sizeof(line_cmds),   "Cmds:      ", numbuf);
    fill(line_os,     sizeof(line_os),     "OS:        guyOS ", version);
    fill(line_kernel, sizeof(line_kernel), "Kernel:    gkern ", kernelver);
    fill(line_gs,     sizeof(line_gs),     "GScript:   ", gs_ver);

    const char *cpu = shell_cpu_brand();
    fill(line_cpu, sizeof(line_cpu), "CPU:       ", cpu ? cpu : "unknown");

    // Try PCI scan for a display controller; fallback to text VGA.
    fill(line_gpu, sizeof(line_gpu), "GPU:       ", detect_gpu_name());

    uint64_t mem_total = shell_mem_total_bytes();
    char mem_num[32];
    if (mem_total == 0) {
        fill(mem_num, sizeof(mem_num), "unknown", "");
    } else {
        uint64_t mb = mem_total / (1024 * 1024);
        char tmp[32]; int ti = 0; uint64_t v = (mb > 0) ? mb : 1; // avoid zero MB
        char rev[32]; int ri = 0; while (v > 0 && ri < 32) { rev[ri++] = '0' + (v % 10); v /= 10; }
        while (ri > 0) tmp[ti++] = rev[--ri];
        tmp[ti] = 0;
        fill(mem_num, sizeof(mem_num), tmp, " MB");
    }
    fill(line_mem, sizeof(line_mem), "Memory:    ", mem_num);

    const char *info[24];
    size_t info_len = 0;
    info[info_len++] = fetch_ver;
    info[info_len++] = "---------------------";
    info[info_len++] = line_user;
    info[info_len++] = line_shell;
    info[info_len++] = line_cwd;
    info[info_len++] = line_cmds;
    info[info_len++] = line_os;
    info[info_len++] = line_kernel;
    info[info_len++] = line_gs;
    info[info_len++] = "Uptime:    n/a";
    info[info_len++] = "Packages:  n/a (for now)";
    info[info_len++] = "Network:   n/a (for now)";

    const char *pad = "           "; // 11 spaces to align values under labels
    const size_t wrap_limit = 52; // keep within 80 cols beside the logo
    if (str_len(cpu ? cpu : "") <= wrap_limit) {
        info[info_len++] = line_cpu;
    } else {
        info[info_len++] = "CPU:";
        fill(line_cpu, sizeof(line_cpu), pad, cpu ? cpu : "unknown");
        info[info_len++] = line_cpu;
    }

    const char *gpu_name = line_gpu + 11; // skip "GPU:       " prefix used in fill
    if (str_len(gpu_name) <= wrap_limit) {
        info[info_len++] = line_gpu;
    } else {
        info[info_len++] = "GPU:";
        fill(line_gpu, sizeof(line_gpu), pad, detect_gpu_name());
        info[info_len++] = line_gpu;
    }

    if (info_len < sizeof(info)/sizeof(info[0])) info[info_len++] = line_mem;
    if (info_len < sizeof(info)/sizeof(info[0])) info[info_len++] = "Colors:";

    // Print side-by-side: logo on left, info on right
    size_t logo_pad = 0;
    for (size_t i = 0; i < sizeof(logo)/sizeof(logo[0]); i++) {
        size_t l = str_len(logo[i]);
        if (l > logo_pad) logo_pad = l;
    }

    size_t info_lines = info_len;
    size_t logo_lines = sizeof(logo) / sizeof(logo[0]);
    size_t max_lines = (logo_lines > info_lines) ? logo_lines : info_lines;
    size_t info_idx = 0;

    for (size_t line = 0; line < max_lines; line++) {
        if (line < logo_lines) {
            shell_write(logo[line]);
        } else {
            write_spaces(logo_pad);
        }

        shell_write("  ");

        if (info_idx < info_lines) {
            shell_write_line(info[info_idx]);
            info_idx++;
        } else {
            shell_write_line("");
        }
    }

    // palette: two rows, 0-7 and 8-15
    uint8_t colors1[] = {0,1,2,3,4,5,6,7};
    uint8_t colors2[] = {8,9,10,11,12,13,14,15};
    write_spaces(logo_pad);
    shell_write("  ");
    for (size_t i = 0; i < sizeof(colors1); i++) { shell_set_color(colors1[i]); shell_write("## "); }
    shell_reset_color(); shell_write_line("");
    write_spaces(logo_pad);
    shell_write("  ");
    for (size_t i = 0; i < sizeof(colors2); i++) { shell_set_color(colors2[i]); shell_write("## "); }
    shell_reset_color(); shell_write_line("");
}

const command_t CMD_FETCH = {
    .name = "fetch",
    .help = "fetch: show system info",
    .handler = fetch_handler
};
