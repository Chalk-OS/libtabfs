#pragma once

#include <stdint.h>
#include <stdlib.h>

extern "C" {
    extern uint8_t* example_disk;

    void* libtabfs_alloc(int size);
    void libtabfs_free(void* ptr, int size);
    void libtabfs_memcpy(void* dest, void* src, int size);
    int libtabfs_strlen(char* str);
    char* libtabfs_strchr(char* str, char c);
    int libtabfs_strcmp(char* a, char* b);
    void my_device_read(dev_t __linux_dev_t, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize);
    void my_device_write(dev_t __linux_dev_t, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize);
    void libtabfs_read_device(void* dev_data, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize);
    void libtabfs_write_device(void* dev_data, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize);
    void libtabfs_set_range_device(void* dev_data, long long lba_address, bool is_absolute_lba, int offset, unsigned char b, int size);
}

void init_example_disk();