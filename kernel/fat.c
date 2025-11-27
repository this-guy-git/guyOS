#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../include/fat.h"
#include "../include/shell_api.h" // for debug prints
#include "../include/hal.h"
#include "../include/disk.h"

// Small helper to emit debug strings to COM1 (serial) so we can capture debug
static void serial_debug(const char *s) {
    if (!s) return;
    for (const char *p = s; *p; p++) {
        outb(0x3F8, (uint8_t)*p);
    }
}

typedef struct {
    uint8_t jmp[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors16;
    uint8_t media;
    uint16_t fat_size16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
    uint32_t fat_size32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot;
} __attribute__((packed)) bpb_t;

// dirent_t is now defined in fat.h

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_LONG_NAME 0x0F

static uint32_t part_lba = 0;
static bpb_t bpb;

static inline uint32_t cluster_to_lba(uint32_t cluster) {
    uint32_t first_data = part_lba + bpb.reserved_sectors + (bpb.fat_count * bpb.fat_size32);
    return first_data + (cluster - 2) * bpb.sectors_per_cluster;
}

static bool read_sector(uint32_t lba, void *buf) {
    return disk_read_sector(lba, buf);
}

static bool write_sector(uint32_t lba, const void *buf) {
    return disk_write_sector(lba, buf);
}

// Simple retry wrappers to ride out transient I/O hiccups.
static bool read_sector_retry(uint32_t lba, void *buf) {
    for (int i = 0; i < 3; i++) {
        if (read_sector(lba, buf)) return true;
    }
    return false;
}

static bool write_sector_retry(uint32_t lba, const void *buf) {
    for (int i = 0; i < 3; i++) {
        if (write_sector(lba, buf)) return true;
    }
    return false;
}

bool fat_init(uint32_t part_lba_start) {
    part_lba = part_lba_start;
    uint8_t sec[512];
    
    // Debug: try to read the boot sector
    if (!read_sector(part_lba, sec)) {
        // Failed to read boot sector
        return false;
    }
    
    // Check for valid FAT32 signature
    if (sec[510] != 0x55 || sec[511] != 0xAA) {
        // Invalid boot signature
        return false;
    }
    
    bpb = *(bpb_t *)sec;
    
    if (bpb.bytes_per_sector != 512) return false;
    if (bpb.fat_size32 == 0) return false;
    
    return true;
}

static bool fat_read_fat(uint32_t cluster, uint32_t *value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = bpb.reserved_sectors + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    uint8_t sec[512];
    if (!read_sector_retry(part_lba + fat_sector, sec)) {
        shell_write("[debug] fat_write_fat: read_sector failed at LBA ");
        serial_debug("[debug] fat_write_fat: read_sector failed at LBA ");
        char tmp[12]; int ti=0; uint32_t t = part_lba + fat_sector; if (t==0) tmp[ti++]='0'; else { char r[12]; int ri=0; while (t>0) { r[ri++]= '0' + (t % 10); t/=10; } while (ri>0) tmp[ti++]=r[--ri]; } tmp[ti]=0; shell_write(tmp); shell_write_line("");
        return false;
    }
    *value = *(uint32_t *)(sec + ent_offset) & 0x0FFFFFFF;
    return true;
}

static bool fat_write_fat(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = bpb.reserved_sectors + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    uint8_t sec[512];
    if (!read_sector_retry(part_lba + fat_sector, sec)) return false;
    *(uint32_t *)(sec + ent_offset) = value;
    if (!write_sector_retry(part_lba + fat_sector, sec)) {
        shell_write("[debug] fat_write_fat: write_sector failed at LBA ");
        serial_debug("[debug] fat_write_fat: write_sector failed at LBA ");
        char tmp[12]; int ti=0; uint32_t t = part_lba + fat_sector; if (t==0) tmp[ti++]='0'; else { char r[12]; int ri=0; while (t>0) { r[ri++]= '0' + (t % 10); t/=10; } while (ri>0) tmp[ti++]=r[--ri]; } tmp[ti]=0; shell_write(tmp); shell_write_line("");
        return false;
    }
    if (bpb.fat_count > 1) {
        if (!write_sector_retry(part_lba + fat_sector + bpb.fat_size32, sec)) {
            // Second FAT copy write failed — log warning but don't fail the operation
            shell_write("[warning] fat_write_fat: write_sector(second copy) failed at LBA ");
            serial_debug("[warning] fat_write_fat: write_sector(second copy) failed at LBA \n");
            char tmp[12]; int ti=0; uint32_t t = part_lba + fat_sector + bpb.fat_size32; if (t==0) tmp[ti++]='0'; else { char r[12]; int ri=0; while (t>0) { r[ri++]= '0' + (t % 10); t/=10; } while (ri>0) tmp[ti++]=r[--ri]; } tmp[ti]=0; shell_write(tmp); shell_write_line("");
            // continue — primary FAT is updated which allows allocations to proceed
        }
    }
    return true;
}

static uint32_t fat_alloc_cluster(void) {
    uint32_t fat_sectors = bpb.fat_size32;
    uint8_t sec[512];
    for (uint32_t s = 0; s < fat_sectors; s++) {
        if (!read_sector_retry(part_lba + bpb.reserved_sectors + s, sec)) return 0;
        for (uint32_t i = 0; i < 128; i++) {
            uint32_t val = ((uint32_t *)sec)[i] & 0x0FFFFFFF;
            uint32_t cluster = s * 128 + i;
            if (cluster < 2) continue;
            if (val == 0) {
                if (!fat_write_fat(cluster, 0x0FFFFFFF)) return 0;
                return cluster;
            }
        }
    }
    return 0;
}

static void fat_free_chain(uint32_t start) {
    uint32_t cl = start;
    while (cl >= 2 && cl < 0x0FFFFFF8) {
        uint32_t next = 0;
        if (!fat_read_fat(cl, &next)) break;
        fat_write_fat(cl, 0);
        if (next == 0 || next >= 0x0FFFFFF8) break;
        cl = next;
    }
}

// Helper to convert normal filename to 8.3 format
static uint8_t name_to_83(const char *name, char *out83) {
    // Convert a normal name into an 8.3 short name. Handles "." and ".."
    // explicitly so path resolution for parent directories works.
    uint8_t ntres = 0;
    for (int i = 0; i < 11; i++) out83[i] = ' ';
    if (!name || !name[0]) return 0;

    // Special entries
    if (name[0] == '.' && name[1] == 0) {
        out83[0] = '.';
        return 0;
    }
    if (name[0] == '.' && name[1] == '.' && name[2] == 0) {
        out83[0] = '.';
        out83[1] = '.';
        return 0;
    }

    const char *dot = 0;
    for (const char *p = name; *p; p++) if (*p == '.') dot = p; // last dot

    int i = 0;
    bool any_lower_base = false;
    for (const char *p = name; *p && i < 8; p++) {
        if (p == dot) break;
        char c = *p;
        if (c >= 'a' && c <= 'z') { any_lower_base = true; c = (char)(c - 32); }
        if (c != '.') out83[i++] = c;
    }
    if (any_lower_base) ntres |= 0x08;

    if (dot) {
        int j = 8;
        bool any_lower_ext = false;
        for (const char *p = dot + 1; *p && j < 11; p++) {
            char c = *p;
            if (c >= 'a' && c <= 'z') { any_lower_ext = true; c = (char)(c - 32); }
            out83[j++] = c;
        }
        if (any_lower_ext) ntres |= 0x10;
    }
    return ntres;
}

void name_from_83(const dirent_t *ent, char *out) {
    // Reconstruct a filename honoring NTRES case bits.
    bool lower_base = (ent->ntres & 0x08) != 0;
    bool lower_ext  = (ent->ntres & 0x10) != 0;
    // If no case info, prefer lower-case display to avoid ALLCAPS noise.
    if (!lower_base && !lower_ext) { lower_base = true; lower_ext = true; }
    int idx = 0;
    for (int i = 0; i < 8 && ent->name[i] != ' '; i++) {
        char c = (char)ent->name[i];
        if (lower_base && c >= 'A' && c <= 'Z') c = (char)(c + 32);
        out[idx++] = c;
    }
    if (ent->name[8] != ' ') {
        out[idx++] = '.';
        for (int i = 8; i < 11 && ent->name[i] != ' '; i++) {
            char c = (char)ent->name[i];
            if (lower_ext && c >= 'A' && c <= 'Z') c = (char)(c + 32);
            out[idx++] = c;
        }
    }
    out[idx] = 0;
}

// Read directory entries from a cluster chain
static bool read_dir_cluster(uint32_t cluster, uint8_t *buf) {
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++) {
        if (!read_sector_retry(lba + s, buf + s * 512)) return false;
    }
    return true;
}

static bool write_dir_cluster(uint32_t cluster, const uint8_t *buf) {
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++) {
        uint32_t target = lba + s;
        bool ok = write_sector_retry(target, buf + s * 512);
        if (!ok) {
            shell_write("[debug] write_dir_cluster: write_sector failed at LBA ");
            serial_debug("[debug] write_dir_cluster: write_sector failed at LBA ");
            char tmp[12]; int ti = 0; uint32_t t = target;
            if (t == 0) tmp[ti++] = '0'; else { char r[12]; int ri=0; while (t > 0) { r[ri++] = '0' + (t % 10); t /= 10; } while (ri > 0) tmp[ti++] = r[--ri]; }
            tmp[ti] = 0; shell_write(tmp); shell_write_line("");
            return false;
        }
    }
    shell_write("[debug] write_dir_cluster: wrote cluster ");
    serial_debug("[debug] write_dir_cluster: wrote cluster ");
    char tmp[12]; int ti = 0; uint32_t t = cluster;
    if (t == 0) tmp[ti++] = '0'; else { char r[12]; int ri=0; while (t > 0) { r[ri++] = '0' + (t % 10); t /= 10; } while (ri > 0) tmp[ti++] = r[--ri]; }
    tmp[ti] = 0; shell_write(tmp); shell_write_line(" ok");
    return true;
}

// LFN structure per FAT spec
typedef struct {
    uint8_t seq;
    uint16_t name1[5];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    uint16_t name2[6];
    uint16_t zero;
    uint16_t name3[2];
} __attribute__((packed)) lfn_ent_t;

static void lfn_reset(char *buf, int buf_sz) {
    if (buf_sz > 0) buf[0] = 0;
}

static void lfn_store(const lfn_ent_t *lfn, char *buf, int buf_sz) {
    // buf_sz includes terminating byte; store ASCII subset only.
    int seq = (lfn->seq & 0x1F) - 1; // 0-based
    if (seq < 0) return;
    int base = seq * 13;
    int max = buf_sz - 1;
    const uint16_t *chunks[3] = { lfn->name1, lfn->name2, lfn->name3 };
    const int counts[3] = {5, 6, 2};
    int pos = base;
    for (int part = 0; part < 3; part++) {
        for (int i = 0; i < counts[part]; i++) {
            if (pos >= max) return;
            uint16_t c = chunks[part][i];
            if (c == 0x0000 || c == 0xFFFF) {
                buf[pos] = 0;
                return;
            }
            buf[pos++] = (c <= 0x7F) ? (char)c : '?';
        }
    }
    if (pos < buf_sz) buf[pos] = 0;
}

// Find entry in a directory chain, matching either 8.3 or reconstructed LFN.
static bool dir_find_in_cluster(uint32_t dir_cluster, const char *name83, const char *long_name, dirent_t *ent_out, uint32_t *cluster_out, uint32_t *idx_out) {
    uint8_t buf[4096]; // assuming max 8 sectors per cluster
    uint32_t cl = dir_cluster;
    char lfn[260]; lfn_reset(lfn, sizeof(lfn));

    while (cl >= 2 && cl < 0x0FFFFFF8) {
        if (!read_dir_cluster(cl, buf)) return false;
        dirent_t *ents = (dirent_t *)buf;
        int max_ents = (bpb.sectors_per_cluster * 512) / 32;
        for (int i = 0; i < max_ents; i++) {
            if (ents[i].name[0] == 0x00) return false; // end of directory
            if (ents[i].name[0] == 0xE5) { lfn_reset(lfn, sizeof(lfn)); continue; } // deleted
            if (ents[i].attr == ATTR_LONG_NAME) { lfn_store((const lfn_ent_t *)&ents[i], lfn, sizeof(lfn)); continue; }

            bool match = false;
            if (name83) {
                match = true;
                for (int j = 0; j < 11; j++) {
                    if (ents[i].name[j] != (uint8_t)name83[j]) { match = false; break; }
                }
            }
            if (!match && long_name && lfn[0]) {
                int eq = 1;
                for (int j = 0; long_name[j] || lfn[j]; j++) {
                    if (long_name[j] != lfn[j]) { eq = 0; break; }
                }
                match = eq != 0;
            }

            if (match) {
                if (ent_out) *ent_out = ents[i];
                if (cluster_out) *cluster_out = cl;
                if (idx_out) *idx_out = (uint32_t)i;
                return true;
            }
            lfn_reset(lfn, sizeof(lfn));
        }
        uint32_t next = 0;
        if (!fat_read_fat(cl, &next)) break;
        if (next == 0 || next >= 0x0FFFFFF8) break;
        cl = next;
    }
    return false;
}

// Create or update a directory entry in parent directory
static bool dir_create_entry(uint32_t parent_cluster, const char *name83, uint8_t ntres, uint32_t entry_cluster, uint8_t attr, uint32_t size, bool allow_replace) {
    uint8_t buf[4096];
    uint32_t cl = parent_cluster;
    int slot = -1;
    dirent_t *ents = NULL;
    int max_ents = 0;

    for (;;) {
        if (!read_dir_cluster(cl, buf)) return false;
        ents = (dirent_t *)buf;
        max_ents = (bpb.sectors_per_cluster * 512) / 32;
        slot = -1;

        if (allow_replace) {
            for (int i = 0; i < max_ents; i++) {
                if (ents[i].name[0] == 0x00) break;
                if (ents[i].name[0] == 0xE5) continue;
                if (ents[i].attr == ATTR_LONG_NAME) continue;
                bool match = true;
                for (int j = 0; j < 11; j++) {
                    if (ents[i].name[j] != (uint8_t)name83[j]) { match = false; break; }
                }
                if (match) { slot = i; break; }
            }
        }

        if (slot == -1) {
            for (int i = 0; i < max_ents; i++) {
                if (ents[i].name[0] == 0x00) { slot = i; break; }
                if (ents[i].name[0] == 0xE5 && slot == -1) slot = i;
            }
        }

        if (slot != -1) break;

        // No space: extend directory by allocating a new cluster and linking it.
        uint32_t next = 0;
        if (!fat_read_fat(cl, &next)) return false;
        if (next >= 2 && next < 0x0FFFFFF8) {
            cl = next;
            continue;
        }
        uint32_t new_cl = fat_alloc_cluster();
        if (new_cl == 0) return false;
        // Link new cluster to chain
        if (!fat_write_fat(cl, new_cl)) return false;
        if (!fat_write_fat(new_cl, 0x0FFFFFFF)) return false;
        // Clear the new cluster buffer
        for (int i = 0; i < (int)sizeof(buf); i++) buf[i] = 0;
        ents = (dirent_t *)buf;
        max_ents = (bpb.sectors_per_cluster * 512) / 32;
        slot = 0;
        cl = new_cl;
        break;
    }

    // Debug: show what we're about to write
    {
        char nm[12]; for (int k=0;k<11;k++) nm[k]=name83[k]; nm[11]=0;
        char hex[3]; const char *h = "0123456789ABCDEF"; hex[0]=h[(ntres>>4)&0xF]; hex[1]=h[ntres&0xF]; hex[2]=0;
        shell_write("[debug] dir_create_entry parent=");
        serial_debug("[debug] dir_create_entry parent=");
        { char tmp[12]; int ti=0; uint32_t t=parent_cluster; if (t==0) tmp[ti++]='0'; else { char r[12]; int ri=0; while (t>0) { r[ri++]= '0'+(t%10); t/=10; } while (ri>0) tmp[ti++]=r[--ri]; } tmp[ti]=0; shell_write(tmp); }
        shell_write(" name=\""); shell_write(nm); shell_write("\" ntres=0x"); shell_write(hex); shell_write_line("");
    }
    // Create the entry
    for (int j = 0; j < 11; j++) ents[slot].name[j] = (uint8_t)name83[j];
    ents[slot].attr = attr;
    ents[slot].ntres = ntres;
    ents[slot].ctime_tenths = 0;
    ents[slot].ctime = 0;
    ents[slot].cdate = 0;
    ents[slot].adate = 0;
    ents[slot].first_cluster_hi = (uint16_t)((entry_cluster >> 16) & 0xFFFF);
    ents[slot].mtime = 0;
    ents[slot].mdate = 0;
    ents[slot].first_cluster_lo = (uint16_t)(entry_cluster & 0xFFFF);
    ents[slot].size = size;
    
    // Mark next entry as end if we used the last 0x00 slot
    if (slot + 1 < max_ents && ents[slot].name[0] == 0x00) {
        ents[slot + 1].name[0] = 0x00;
    }
    
    bool ok = write_dir_cluster(cl, buf);
    shell_write("[debug] dir_create_entry write_dir_cluster -> "); shell_write(ok ? "ok\n" : "fail\n");
    serial_debug("[debug] dir_create_entry write_dir_cluster -> "); serial_debug(ok ? "ok\n" : "fail\n");
    return ok;
}

// Create a directory with . and .. entries
static bool create_directory_entries(uint32_t dir_cluster, uint32_t parent_cluster) {
    uint8_t buf[4096];
    for (int i = 0; i < (int)(bpb.sectors_per_cluster * 512); i++) buf[i] = 0;
    
    dirent_t *ents = (dirent_t *)buf;
    
    // . entry
    for (int i = 0; i < 11; i++) ents[0].name[i] = ' ';
    ents[0].name[0] = '.';
    ents[0].attr = ATTR_DIRECTORY;
    ents[0].first_cluster_hi = (uint16_t)((dir_cluster >> 16) & 0xFFFF);
    ents[0].first_cluster_lo = (uint16_t)(dir_cluster & 0xFFFF);
    
    // .. entry
    for (int i = 0; i < 11; i++) ents[1].name[i] = ' ';
    ents[1].name[0] = '.';
    ents[1].name[1] = '.';
    ents[1].attr = ATTR_DIRECTORY;
    ents[1].first_cluster_hi = (uint16_t)((parent_cluster >> 16) & 0xFFFF);
    ents[1].first_cluster_lo = (uint16_t)(parent_cluster & 0xFFFF);
    
    return write_dir_cluster(dir_cluster, buf);
}

// Public API: Create a directory
bool fat_mkdir(const char *path) {
    // Parse path to get parent and directory name
    // For now, assume path is just "dirname" in root
    char name83[11];
    uint8_t ntres = name_to_83(path, name83);
    
    // Check if already exists
    dirent_t ent;
    if (dir_find_in_cluster(bpb.root_cluster, name83, path, &ent, NULL, NULL)) {
        return false; // already exists
    }
    
    // Allocate cluster for new directory
    uint32_t new_cluster = fat_alloc_cluster();
    if (new_cluster == 0) return false;
    
    // Initialize directory with . and .. entries
    if (!create_directory_entries(new_cluster, bpb.root_cluster)) {
        fat_free_chain(new_cluster);
        return false;
    }
    
    // Create entry in parent (root)
    if (!dir_create_entry(bpb.root_cluster, name83, ntres, new_cluster, ATTR_DIRECTORY, 0, true)) {
        fat_free_chain(new_cluster);
        return false;
    }
    
    return true;
}

bool fat_mkdir_at(uint32_t parent_cluster, const char *name, uint32_t *out_cluster) {
    char name83[11];
    uint8_t ntres = name_to_83(name, name83);
    dirent_t ent;
    uint32_t existing_cluster = 0;
    uint32_t existing_idx = 0;
    if (dir_find_in_cluster(parent_cluster, name83, name, &ent, &existing_cluster, &existing_idx)) {
        // If entry already exists, ensure ntres matches desired case bits. If not, update the directory entry.
        if (ent.ntres != ntres) {
            // read parent cluster and update entry
            uint8_t buf[4096];
            if (read_dir_cluster(parent_cluster, buf)) {
                dirent_t *ents = (dirent_t *)buf;
                ents[existing_idx].ntres = ntres;
                if (!write_dir_cluster(parent_cluster, buf)) {
                    shell_write_line("[debug] fat_mkdir_at: failed to update ntres for existing entry");
                } else {
                    shell_write_line("[debug] fat_mkdir_at: updated ntres for existing entry");
                }
            }
        }
        uint32_t cl = ((uint32_t)ent.first_cluster_hi << 16) | ent.first_cluster_lo;
        if (out_cluster) *out_cluster = cl;
        return true; // already exists
    }

    uint32_t new_cluster = fat_alloc_cluster();
    if (new_cluster == 0) return false;

    if (!create_directory_entries(new_cluster, parent_cluster)) {
        fat_free_chain(new_cluster);
        return false;
    }

    if (!dir_create_entry(parent_cluster, name83, ntres, new_cluster, ATTR_DIRECTORY, 0, true)) {
        fat_free_chain(new_cluster);
        return false;
    }
    if (out_cluster) *out_cluster = new_cluster;
    return true;
}

// Navigate to a directory by path
bool fat_chdir(const char *path, uint32_t *out_cluster) {
    bool is_dir = false;
    if (!fat_resolve_path(bpb.root_cluster, path, out_cluster, &is_dir)) return false;
    return is_dir;
}

bool fat_resolve_path(uint32_t start_cluster, const char *path, uint32_t *out_cluster, bool *is_dir_out) {
    if (!path || !out_cluster) return false;

    // Track the traversal stack so ".." can be handled even if directory entries are missing.
    uint32_t stack[32];
    int depth = 0;
    uint32_t current = (path[0] == '/') ? bpb.root_cluster : start_cluster;
    stack[depth] = current;

    const char *p = path;
    if (*p == '/') p++;

    char component[32]; // allow slightly longer components (still truncated to 8.3)
    bool last_is_dir = true;

    while (1) {
        while (*p == '/') p++;
        if (*p == 0) break;

        const char *start = p;
        size_t len = 0;
        while (p[len] && p[len] != '/' && len + 1 < sizeof(component)) {
            component[len] = p[len];
            len++;
        }
        component[len] = 0;
        while (p[len] && p[len] != '/') len++;
        p = start + len;

        if (component[0] == 0) continue;

        if (component[0] == '.' && component[1] == 0) {
            last_is_dir = true;
            continue;
        }
        if (component[0] == '.' && component[1] == '.' && component[2] == 0) {
            if (depth > 0) {
                depth--;
                current = stack[depth];
            } else {
                // Use on-disk .. if present; otherwise fall back to root.
                dirent_t parent;
                char dotdot[11];
                name_to_83("..", dotdot);
                if (dir_find_in_cluster(current, dotdot, "..", &parent, NULL, NULL)) {
                    uint32_t cl = ((uint32_t)parent.first_cluster_hi << 16) | parent.first_cluster_lo;
                    if (cl != 0) current = cl;
                    else current = bpb.root_cluster;
                } else {
                    current = bpb.root_cluster;
                }
            }
            last_is_dir = true;
            continue;
        }

        char name83[11];
        name_to_83(component, name83);
        dirent_t ent;
        if (!dir_find_in_cluster(current, name83, component, &ent, NULL, NULL)) return false;
        uint32_t cl = ((uint32_t)ent.first_cluster_hi << 16) | ent.first_cluster_lo;
        bool is_dir = (ent.attr & ATTR_DIRECTORY) != 0;

        last_is_dir = is_dir;
        current = cl;
        if (is_dir && depth + 1 < (int)(sizeof(stack) / sizeof(stack[0]))) {
            stack[++depth] = current;
        }
    }

    if (out_cluster) *out_cluster = current;
    if (is_dir_out) *is_dir_out = last_is_dir;
    return true;
}

// Internal read helper
static bool fat_read_file_internal(uint32_t dir_cluster, const char *name, uint8_t *buf, size_t max, size_t *out_len) {
    char name83[11];
    name_to_83(name, name83);

    dirent_t ent;
    if (!dir_find_in_cluster(dir_cluster, name83, name, &ent, NULL, NULL)) return false;
    
    uint32_t cluster = ((uint32_t)ent.first_cluster_hi << 16) | ent.first_cluster_lo;
    uint32_t bytes_left = ent.size;
    size_t written = 0;
    uint32_t cl = cluster;
    uint8_t sec[512];
    
    while (bytes_left && cl >= 2 && cl < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cl);
        for (uint32_t s = 0; s < bpb.sectors_per_cluster && bytes_left; s++) {
            if (!read_sector_retry(lba + s, sec)) return false;
            size_t to_copy = bytes_left > 512 ? 512 : bytes_left;
            if (written + to_copy > max) to_copy = max - written;
            if (to_copy == 0) break;
            for (size_t i = 0; i < to_copy; i++) buf[written + i] = sec[i];
            written += to_copy;
            bytes_left -= to_copy;
        }
        if (bytes_left == 0) break;
        if (!fat_read_fat(cl, &cl)) break;
    }
    if (out_len) *out_len = written;
    return true;
}


bool fat_read_file(const char *name, uint8_t *buf, size_t max, size_t *out_len) {
    return fat_read_file_internal(bpb.root_cluster, name, buf, max, out_len);
}

bool fat_read_file_at(uint32_t dir_cluster, const char *name, uint8_t *buf, size_t max, size_t *out_len) {
    return fat_read_file_internal(dir_cluster, name, buf, max, out_len);
}

// Write file to any directory
static bool fat_write_file_internal(uint32_t dir_cluster, const char *name, const uint8_t *buf, size_t len) {
    dirent_t ent;
    uint32_t ent_cluster = 0, ent_idx = 0;
    uint32_t cluster = 0;
    char name83[11];
    uint8_t ntres = name_to_83(name, name83);
    bool exists = dir_find_in_cluster(dir_cluster, name83, name, &ent, &ent_cluster, &ent_idx);
    
    if (exists) {
        cluster = ((uint32_t)ent.first_cluster_hi << 16) | ent.first_cluster_lo;
        fat_free_chain(cluster);
    }
    
    uint32_t needed_clusters = (len + (bpb.sectors_per_cluster * 512 - 1)) / (bpb.sectors_per_cluster * 512);
    if (needed_clusters == 0) needed_clusters = 1;
    
    uint32_t first = 0, prev = 0;
    for (uint32_t c = 0; c < needed_clusters; c++) {
        uint32_t cl = fat_alloc_cluster();
        if (cl == 0) return false;
        if (first == 0) first = cl;
        if (prev) fat_write_fat(prev, cl);
        prev = cl;
    }
    fat_write_fat(prev, 0x0FFFFFFF);
    cluster = first;
    
    uint32_t remaining = (uint32_t)len;
    size_t offset = 0;
    uint32_t cl = cluster;
    uint8_t sec[512];
    
    while (cl >= 2 && cl < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cl);
        for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++) {
            size_t to_copy = remaining > 512 ? 512 : remaining;
            for (size_t i = 0; i < to_copy; i++) sec[i] = buf[offset + i];
            if (to_copy < 512) for (size_t i = to_copy; i < 512; i++) sec[i] = 0;
            if (!write_sector_retry(lba + s, sec)) return false;
            offset += to_copy;
            if (remaining > to_copy) remaining -= to_copy; else remaining = 0;
            if (remaining == 0) break;
        }
        uint32_t next = 0;
        if (!fat_read_fat(cl, &next)) break;
        cl = next;
        if (remaining == 0) break;
    }
    
    if (!exists) {
        if (!dir_create_entry(dir_cluster, name83, ntres, cluster, ATTR_ARCHIVE, (uint32_t)len, true)) {
            return false;
        }
    } else {
        // Update existing entry
        uint8_t buf_dir[4096];
        if (!read_dir_cluster(ent_cluster, buf_dir)) return false;
        dirent_t *ents = (dirent_t *)buf_dir;
        ents[ent_idx].first_cluster_hi = (uint16_t)((cluster >> 16) & 0xFFFF);
        ents[ent_idx].first_cluster_lo = (uint16_t)(cluster & 0xFFFF);
        ents[ent_idx].size = (uint32_t)len;
        // Preserve case bits for filename if different
        if (ents[ent_idx].ntres != ntres) {
            ents[ent_idx].ntres = ntres;
            // Debug
            shell_write("[debug] fat_write_file_internal: updated ntres for existing file\n");
            serial_debug("[debug] fat_write_file_internal: updated ntres for existing file\n");
        }
        if (!write_dir_cluster(ent_cluster, buf_dir)) return false;
    }
    
    return true;
}

bool fat_write_file(const char *name, const uint8_t *buf, size_t len) {
    return fat_write_file_internal(bpb.root_cluster, name, buf, len);
}

bool fat_write_file_at(uint32_t dir_cluster, const char *name, const uint8_t *buf, size_t len) {
    return fat_write_file_internal(dir_cluster, name, buf, len);
}

// List directory contents
bool fat_list_dir(uint32_t dir_cluster, void (*callback)(const char *name, bool is_dir, uint32_t size)) {
    uint8_t buf[4096];
    uint32_t cl = dir_cluster;
    char lfn[260]; lfn_reset(lfn, sizeof(lfn));

    while (cl >= 2 && cl < 0x0FFFFFF8) {
        if (!read_dir_cluster(cl, buf)) return false;
        dirent_t *ents = (dirent_t *)buf;
        int max_ents = (bpb.sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_ents; i++) {
            if (ents[i].name[0] == 0x00) return true;
            if (ents[i].name[0] == 0xE5) { lfn_reset(lfn, sizeof(lfn)); continue; }
            if (ents[i].attr == ATTR_LONG_NAME) { lfn_store((const lfn_ent_t *)&ents[i], lfn, sizeof(lfn)); continue; }
            if (ents[i].name[0] == '.') { lfn_reset(lfn, sizeof(lfn)); continue; } // skip . and ..

            char name[260];
            if (lfn[0]) {
                int j = 0; while (lfn[j] && j + 1 < (int)sizeof(name)) { name[j] = lfn[j]; j++; } name[j] = 0;
            } else {
                name_from_83(&ents[i], name);
            }
            bool is_dir = (ents[i].attr & ATTR_DIRECTORY) != 0;
            callback(name, is_dir, ents[i].size);
            lfn_reset(lfn, sizeof(lfn));
        }
        uint32_t next = 0;
        if (!fat_read_fat(cl, &next)) break;
        if (next == 0 || next >= 0x0FFFFFF8) break;
        cl = next;
    }
    return true;
}

// Verbose directory listing that passes raw dirent_t to callback
bool fat_list_dir_verbose(uint32_t dir_cluster, void (*callback)(const dirent_t *ent)) {
    uint8_t buf[4096];
    uint32_t cl = dir_cluster;
    while (cl >= 2 && cl < 0x0FFFFFF8) {
        if (!read_dir_cluster(cl, buf)) return false;
        dirent_t *ents = (dirent_t *)buf;
        int max_ents = (bpb.sectors_per_cluster * 512) / 32;

        for (int i = 0; i < max_ents; i++) {
            if (ents[i].name[0] == 0x00) return true;
            if (ents[i].name[0] == 0xE5) continue;
            if (ents[i].attr == ATTR_LONG_NAME) continue;
            if (ents[i].name[0] == '.') continue; // skip . and ..
            callback(&ents[i]);
        }
        uint32_t next = 0;
        if (!fat_read_fat(cl, &next)) break;
        if (next == 0 || next >= 0x0FFFFFF8) break;
        cl = next;
    }
    return true;
}

uint32_t fat_ensure_dir_at(uint32_t parent_cluster, const char *name) {
    if (!name || !name[0]) return 0;
    char name83[11];
    uint8_t ntres = name_to_83(name, name83);
    dirent_t ent;
    uint32_t cluster = 0;
    uint32_t idx = 0;
    // If exists, return the cluster and update ntres if needed
    if (dir_find_in_cluster(parent_cluster, name83, name, &ent, &cluster, &idx)) {
        uint32_t cl = ((uint32_t)ent.first_cluster_hi << 16) | ent.first_cluster_lo;
        if (ent.ntres != ntres) {
            uint8_t buf[4096];
            if (read_dir_cluster(parent_cluster, buf)) {
                dirent_t *ents = (dirent_t *)buf;
                ents[idx].ntres = ntres;
                if (!write_dir_cluster(parent_cluster, buf)) {
                    // failed to persist ntres change
                    shell_write_line("[debug] fat_ensure_dir_at: failed to update ntres for existing entry");
                    serial_debug("[debug] fat_ensure_dir_at: failed to update ntres for existing entry\n");
                } else {
                    shell_write_line("[debug] fat_ensure_dir_at: updated ntres for existing entry");
                    serial_debug("[debug] fat_ensure_dir_at: updated ntres for existing entry\n");
                }
            }
        }
        return cl;
    }

    // Not found: create it
    if (!fat_mkdir_at(parent_cluster, name, &cluster)) {
        // Creation failed; maybe it actually exists despite a warning (e.g., backup FAT write).
        if (!dir_find_in_cluster(parent_cluster, name83, name, &ent, &cluster, &idx)) return 0;
        return ((uint32_t)ent.first_cluster_hi << 16) | ent.first_cluster_lo;
    }
    if (cluster != 0) return cluster;
    // Resolve cluster of newly-created entry if not provided
    if (dir_find_in_cluster(parent_cluster, name83, name, &ent, &cluster, &idx)) {
        return ((uint32_t)ent.first_cluster_hi << 16) | ent.first_cluster_lo;
    }
    return 0;
}

uint32_t fat_get_root_cluster(void) {
    return bpb.root_cluster;
}
