#pragma once
#include <stdbool.h>
#include <stdint.h>

bool disk_read_sector(uint32_t lba, void *buf);
bool disk_write_sector(uint32_t lba, const void *buf);
