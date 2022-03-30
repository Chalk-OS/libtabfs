#pragma once

#include <stdint.h>

void dump_mem(uint8_t* mem, int size);

extern "C" {
    #include "libtabfs.h"
}
void dump_bat_region(libtabfs_bat_t* bat);

void dump_entrytable_cache(libtabfs_volume_t* volume);
void dump_entrytable_region(libtabfs_entrytable_t* entrytable);
void dump_entrytable_entry(libtabfs_entrytable_entry_t* entry, libtabfs_volume_t* volume);

void dump_fat_region(libtabfs_fat_t* fat);