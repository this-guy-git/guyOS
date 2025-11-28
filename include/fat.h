
#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

void name_from_83(const dirent_t *ent, char *out);

// Initialize FAT filesystem
bool fat_init(uint32_t part_lba_start);

// Directory operations
bool fat_mkdir(const char *path);
bool fat_mkdir_at(uint32_t parent_cluster, const char *name, uint32_t *out_cluster);
bool fat_list_dir(uint32_t dir_cluster, void (*callback)(const char *name, bool is_dir, uint32_t size));
// Verbose directory listing: pass raw dirent pointer for more diagnostics
bool fat_list_dir_verbose(uint32_t dir_cluster, void (*callback)(const dirent_t *ent));
// Ensure a directory with `name` exists inside `parent_cluster`.
// Returns the cluster number for the directory on success, 0 on failure.
uint32_t fat_ensure_dir_at(uint32_t parent_cluster, const char *name);
uint32_t fat_get_root_cluster(void);
bool fat_resolve_path(uint32_t start_cluster, const char *path, uint32_t *out_cluster, bool *is_dir);

// File operations
bool fat_read_file(const char *name, uint8_t *buf, size_t max, size_t *out_len);
bool fat_write_file(const char *name, const uint8_t *buf, size_t len);
bool fat_read_file_at(uint32_t dir_cluster, const char *name, uint8_t *buf, size_t max, size_t *out_len);
bool fat_write_file_at(uint32_t dir_cluster, const char *name, const uint8_t *buf, size_t len);
// Delete a file (not directories) in the given directory.
bool fat_delete_file_at(uint32_t dir_cluster, const char *name);

#endif
