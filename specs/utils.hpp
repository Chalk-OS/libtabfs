#pragma once

#include <stdint.h>

void write_i8(void* dest, uint8_t i);
void write_i16(void* dest, uint16_t i);
void write_i32(void* dest, uint32_t i);
void write_i48(void* dest, uint64_t i);
void write_i64(void* dest, uint64_t i);

#define WRAP_WRITE(name, type) void write_##name(long long lba, int offset, type i);
WRAP_WRITE(i8, uint8_t)
WRAP_WRITE(i16, uint16_t)
WRAP_WRITE(i32, uint32_t)
WRAP_WRITE(i48, uint64_t)
WRAP_WRITE(i64, uint64_t)
#undef WRAP_WRITE

void write_mem(long long lba, int offset, void* mem, int n);
void write_clear(long long lba, int offset, uint8_t c, int n);

void dump_mem(uint8_t* mem, int size);

extern "C" {
    #include "libtabfs.h"
}
void dump_bat_region(libtabfs_bat_t* bat);
void dump_entrytable_cache(libtabfs_volume_t* volume);
void dump_entrytable_region(libtabfs_entrytable_t* entrytable);