#include "./bridge_impl.hpp"
#include <string.h>
#include <stdio.h>
#include "./utils.hpp"

extern "C" {

    void* libtabfs_alloc(int size) {
        void* ptr = calloc(size, sizeof(uint8_t));
        return ptr;
    }

    void libtabfs_free(void* ptr, int size) {
        free(ptr);
    }

    void libtabfs_memcpy(void* dest, void* src, int size) {
        memcpy(dest, src, size);
    }

    int libtabfs_strlen(char* str) {
        return strlen(str);
    }

    char* libtabfs_strchr(char* str, char c) {
        return strchr(str, c);
    }

    int libtabfs_strcmp(char* a, char* b) {
        return strcmp(a, b);
    }

    uint8_t* example_disk;

    void my_device_read(dev_t __linux_dev_t, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize) {
        printf(
            "my_device_read(dev=0x%x, lba=0x%lx, is_abs_lba=%s, off=0x%04x, buf=%p, bufsz=% 4d)\n",
            __linux_dev_t, lba_address, (is_absolute_lba ? "yes" : "no "), offset, buffer, bufferSize
        );
        memcpy(buffer, example_disk + (lba_address * 512) + offset, bufferSize);
        //dump_mem(example_disk + (lba_address * 512) + offset, bufferSize);
    }
    void my_device_write(dev_t __linux_dev_t, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize) {
        printf(
            "my_device_write(dev=0x%x, lba=0x%lx, is_abs_lba=%s, off=0x%04x, buf=%p, bufsz=% 4d)\n",
            __linux_dev_t, lba_address, (is_absolute_lba ? "yes" : "no "), offset, buffer, bufferSize
        );
        memcpy(example_disk + (lba_address * 512) + offset, buffer, bufferSize);
        //dump_mem((uint8_t*)buffer, bufferSize);
        //dump_mem(example_disk + (lba_address * 512) + offset, bufferSize);
    }

    void libtabfs_read_device(void* dev_data, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize) {
        dev_t* dev = (dev_t*) dev_data;
        my_device_read(*dev, lba_address, is_absolute_lba, offset, buffer, bufferSize);
    }
    void libtabfs_write_device(void* dev_data, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize) {
        dev_t* dev = (dev_t*) dev_data;
        my_device_write(*dev, lba_address, is_absolute_lba, offset, buffer, bufferSize);
    }

}

void init_example_disk() {
    // init the example disk
    example_disk = (uint8_t*) calloc( 512 * 10, sizeof(uint8_t) );

    /**
     * layout:
     * 
     * 0x0 - preload / bootsector with tabfs header
     * 0x1 - tabfs volume info
     * 0x2 - section 1 of bat with one block (0x2)
     * 0x3 - root table
     * 0x4 - section 2 of bat with two blocks (0x4, 0x5)
     * 0x5 - ///
     * 0x6 - free!
     */

    // write header
    {
        memcpy(example_disk + 0x1C0, "TABFS-28", 9);            // magic
        write_i16(example_disk + 0x1F0, 0b0000000000000001);    // flags
        write_i64(example_disk + 0x1F6, 0x1);                   // info LBA
        write_i16(example_disk + 0x1FE, 0xAA55);
    }

    // write volume info
    {
        memcpy(example_disk + 512, "TABFS-28", 9);  // magic
        write_i32(0x1, 0x10, 0x2);                  // bat LBA
        write_i32(0x1, 0x14, 0x0);                  // min LBA
        write_i32(0x1, 0x18, 0x2);                  // bat-start LBA
        write_i32(0x1, 0x1C, 0xFFF);                // max LBA
        write_i32(0x1, 0x20, 512);                  // blockSize
        write_i8(0x1, 0x24, 1);                     // BS
        write_i16(0x1, 0x26, 0b0000000000000001);   // flags
        write_i32(0x1, 0x28, 0x3);                  // root LBA
        write_i32(0x1, 0x2C, 512);                  // root size
        write_clear(0x01, 0x30, 0x0, 32);
        write_mem(0x1, 0x50, (void*)"This is an awesome volume!", 27);
    }

    // write bat
    {
        // first section
        write_i32(0x2, 0, 0x4);
        write_i16(0x2, 4, 1);
        write_clear(0x2, 6, 0, 512 - 6);
        // set first 3 blocks as taken
        write_i8(0x2, 6, 0b11110000);

        // second section
        write_i32(0x4, 0, 0);
        write_i16(0x4, 4, 2);
        write_clear(0x4, 6, 0, 1024 - 6);
    }

    // write roottable tableinfo
    {
        write_i8(0x3, 0, 0xE0);
    }
}