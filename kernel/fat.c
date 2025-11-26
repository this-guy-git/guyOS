#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../include/fat.h"
#include "../include/disk.h"

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

bool fat_init(uint32_t part_lba_start) {
    part_lba = part_lba_start;
    uint8_t sec[512];
    if (!read_sector(part_lba, sec)) return false;
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
    if (!read_sector(part_lba + fat_sector, sec)) return false;
    *value = *(uint32_t *)(sec + ent_offset) & 0x0FFFFFFF;
    return true;
}

static bool fat_write_fat(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = bpb.reserved_sectors + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    uint8_t sec[512];
    if (!read_sector(part_lba + fat_sector, sec)) return false;
    *(uint32_t *)(sec + ent_offset) = value;
    if (!write_sector(part_lba + fat_sector, sec)) return false;
    if (bpb.fat_count > 1) {
        if (!write_sector(part_lba + fat_sector + bpb.fat_size32, sec)) return false;
    }
    return true;
}

static uint32_t fat_alloc_cluster(void) {
    uint32_t fat_sectors = bpb.fat_size32;
    uint8_t sec[512];
    for (uint32_t s = 0; s < fat_sectors; s++) {
        if (!read_sector(part_lba + bpb.reserved_sectors + s, sec)) return 0;
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

typedef struct {
    uint8_t name[11];
    uint8_t attr;
    uint8_t ntres;
    uint8_t ctime_tenths;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t first_cluster_hi;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t first_cluster_lo;
    uint32_t size;
} __attribute__((packed)) dirent_t;

static bool dir_find(const char *name11, dirent_t *ent_out, uint32_t *lba_out, uint32_t *idx_out) {
    uint32_t cluster = bpb.root_cluster;
    uint8_t sec[512];
    uint32_t sectors_per_cluster = bpb.sectors_per_cluster;
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t s = 0; s < sectors_per_cluster; s++) {
        if (!read_sector(lba + s, sec)) return false;
        dirent_t *ents = (dirent_t *)sec;
        for (int i = 0; i < 16; i++) {
            if (ents[i].name[0] == 0x00) return false;
            if (ents[i].attr == 0x0F) continue;
            bool match = true;
            for (int j = 0; j < 11; j++) {
                if ((uint8_t)name11[j] != ents[i].name[j]) { match = false; break; }
            }
            if (match) {
                if (ent_out) *ent_out = ents[i];
                if (lba_out) *lba_out = lba + s;
                if (idx_out) *idx_out = (uint32_t)i;
                return true;
            }
        }
    }
    return false;
}

static bool dir_update(const char *name11, uint32_t cluster, uint32_t size, bool overwrite) {
    uint32_t root = bpb.root_cluster;
    uint8_t sec[512];
    uint32_t lba = cluster_to_lba(root);
    uint32_t sectors_per_cluster = bpb.sectors_per_cluster;
    for (uint32_t s = 0; s < sectors_per_cluster; s++) {
        if (!read_sector(lba + s, sec)) return false;
        dirent_t *ents = (dirent_t *)sec;
        for (int i = 0; i < 16; i++) {
            bool empty = (ents[i].name[0] == 0x00 || ents[i].name[0] == 0xE5);
            bool match = true;
            for (int j = 0; j < 11; j++) {
                if ((uint8_t)name11[j] != ents[i].name[j]) { match = false; break; }
            }
            if (empty || (overwrite && match)) {
                for (int j = 0; j < 11; j++) ents[i].name[j] = (uint8_t)name11[j];
                ents[i].attr = 0x20;
                ents[i].first_cluster_lo = (uint16_t)(cluster & 0xFFFF);
                ents[i].first_cluster_hi = (uint16_t)((cluster >> 16) & 0xFFFF);
                ents[i].size = size;
                return write_sector(lba + s, sec);
            }
        }
    }
    return false;
}

bool fat_read_file(const char *name11, uint8_t *buf, size_t max, size_t *out_len) {
    dirent_t ent;
    if (!dir_find(name11, &ent, NULL, NULL)) return false;
    uint32_t cluster = ((uint32_t)ent.first_cluster_hi << 16) | ent.first_cluster_lo;
    uint32_t bytes_left = ent.size;
    size_t written = 0;
    uint32_t cl = cluster;
    uint8_t sec[512];
    while (bytes_left && cl >= 2 && cl < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cl);
        for (uint32_t s = 0; s < bpb.sectors_per_cluster && bytes_left; s++) {
            if (!read_sector(lba + s, sec)) return false;
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

bool fat_write_file(const char *name11, const uint8_t *buf, size_t len) {
    dirent_t ent;
    uint32_t ent_lba = 0, ent_idx = 0;
    uint32_t cluster = 0;
    bool exists = dir_find(name11, &ent, &ent_lba, &ent_idx);
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
            if (!write_sector(lba + s, sec)) return false;
            offset += to_copy;
            if (remaining > to_copy) remaining -= to_copy; else remaining = 0;
            if (remaining == 0) break;
        }
        uint32_t next = 0;
        if (!fat_read_fat(cl, &next)) break;
        cl = next;
        if (remaining == 0) break;
    }
    if (!dir_update(name11, cluster, (uint32_t)len, exists)) return false;
    return true;
}
