#include "./utils.hpp"
#include <string.h>

extern uint8_t* example_disk;

void write_i8(void* dest, uint8_t i) {
    memcpy(dest, &i, sizeof(uint8_t));
}
void write_i16(void* dest, uint16_t i) {
    memcpy(dest, &i, sizeof(uint16_t));
}
void write_i32(void* dest, uint32_t i) {
    memcpy(dest, &i, sizeof(uint32_t));
}
void write_i48(void* dest, uint64_t i) {
    memcpy(dest, &i, 6);
}
void write_i64(void* dest, uint64_t i) {
    memcpy(dest, &i, sizeof(uint64_t));
}

#define WRAP_WRITE(name, type) \
    void write_##name(long long lba, int offset, type i) { write_##name(example_disk + (lba * 512) + offset, i); }
WRAP_WRITE(i8, uint8_t)
WRAP_WRITE(i16, uint16_t)
WRAP_WRITE(i32, uint32_t)
WRAP_WRITE(i48, uint64_t)
WRAP_WRITE(i64, uint64_t)

void write_mem(long long lba, int offset, void* mem, int n) {
    memcpy(example_disk + (lba * 512) + offset, mem, n);
}
void write_clear(long long lba, int offset, uint8_t c, int n) {
    memset(example_disk + (lba * 512) + offset, c, n);
}