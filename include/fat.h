#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool fat_init(uint32_t part_lba_start);
bool fat_read_file(const char *name11, uint8_t *buf, size_t max, size_t *out_len);
bool fat_write_file(const char *name11, const uint8_t *buf, size_t len);
