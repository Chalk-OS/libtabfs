#include <cxxspec/cxxspec.hpp>

extern "C" {
    #define LIBTABFS_DEVICE_ARGS            dev_t __linux_dev_t
    #define LIBTABFS_HEADER_DEVICE_FIELDS   dev_t __linux_dev_t;
    #define LIBTABFS_DEVICE_ARGS_TO_HEADER(header)  header->__linux_dev_t = __linux_dev_t;

    #define LIBTABFS_DEVICE_PARAMS(header)  header->__linux_dev_t

    #define LIBTABFS_ALLOC(size)        malloc(size);
    #define LIBTABFS_FREE(ptr, size)    free(ptr);

    #define LIBTABFS_READ_DEVICE    my_device_read
    #define LIBTABFS_WRITE_DEVICE   my_device_write

    struct tabfs_header;
    typedef struct tabfs_header* tabfs_header_t;
    //#include <libtabfs.h>

    uint8_t* example_disk;

    void my_device_read(dev_t __linux_dev_t, long long lba_address, int offset, void* buffer, int bufferSize) {
        printf("my_device_read(%ld, 0x%lx, 0x%x, %p, %d)\n", __linux_dev_t, lba_address, offset, buffer, bufferSize);
        memcpy(buffer, example_disk + (lba_address * 512) + offset, bufferSize);
    }
    void my_device_write(dev_t __linux_dev_t, long long lba_address, int offset, void* buffer, int bufferSize) {}

    #define LIBTABFS_IMPLEMENTATION
    #include <libtabfs.h>
}

tabfs_header_t gHeader;

describe(libtabfs, $ {
    explain("tabfs_new_header", $ {
        it("should have a magic", _ {
            expect((char*)gHeader->magic).to_eq((char*)"TABFS-28");
        });
        it("should have a bat on lba 0x1", _ {
            expect(TABFS_LBA_48_TOI(gHeader->bat_LBA)).to_eq(0x1);
        });
        it("should have the root table on lba 0x2", _ {
            expect(TABFS_LBA_48_TOI(gHeader->root_LBA)).to_eq(0x2);
        });

        explain("bat - first section", $ {
            it("should be on lba 0x1", _ {
                tabfs_bat_t bat = gHeader->__bat_root;
                expect(bat->__lba).to_eq(0x1);
            });
            it("should have a block_count of 1", _ {
                tabfs_bat_t bat = gHeader->__bat_root;
                expect(bat->block_count).to_eq(1);
            });
            it("should reference on the next section on lba 0x3", _ {
                tabfs_bat_t bat = gHeader->__bat_root;
                expect(bat->next_bat).to_eq(0x3);
            });
        });

        explain("bat - second section", $ {
            it("should be accessible through the first section", _ {
                tabfs_bat_t bat = gHeader->__bat_root;
                expect(bat->__next_bat).to_not_eq(nullptr);
            });
            it("should be on lba 0x3", _ {
                tabfs_bat_t bat = gHeader->__bat_root;
                expect(bat->__next_bat).to_not_eq(nullptr);
                bat = bat->__next_bat;
                expect(bat->__lba).to_eq(0x3);
            });
            it("should have a block_count of 2", _ {
                tabfs_bat_t bat = gHeader->__bat_root;
                expect(bat->__next_bat).to_not_eq(nullptr);
                bat = bat->__next_bat;
                expect(bat->block_count).to_eq(2);
            });
        });

    });

    explain("tabfs_bat_isFree", $ {
        it("should have non-free blocks on 0x0 - 0x4", _ {
            expect(tabfs_bat_isFree(gHeader, 0x0)).to_eq(false);
            expect(tabfs_bat_isFree(gHeader, 0x1)).to_eq(false);
            expect(tabfs_bat_isFree(gHeader, 0x2)).to_eq(false);
            expect(tabfs_bat_isFree(gHeader, 0x3)).to_eq(false);
            expect(tabfs_bat_isFree(gHeader, 0x4)).to_eq(false);
        });
        it("should work accross bat regions", _ {
            expect(tabfs_bat_isFree(gHeader, 0xfd0)).to_eq(true);
        });
        it("should create new bat regions if the current regions cannot hold the requested lba", _ {
            expect(tabfs_bat_isFree(gHeader, 0xfd0 + 0x1fd0)).to_eq(true);
        });
    });
});

void dump_mem(uint8_t* mem, int size) {
    printf("\e[32m-- 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\e[0m");
    for (int i = 0; i < size; i++) {
        if ((i % 16) == 0) {
            printf("\n\e[32m%02x\e[0m ", i / 16);
        }
        printf("%02x ", mem[i]);
    }
    printf("\n");
}

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

void init_example_disk() {
    // init the example disk
    example_disk = (uint8_t*) calloc( 512 * 5, sizeof(uint8_t) );

    // write header
    {
        memcpy(example_disk + 0x1BD, "TABFS-28", 9);    // magic
        write_i48(example_disk + 0x1ED, 0x1);   // bat LBA
        write_i8(example_disk + 0x1F3, 1);      // blockSize
        write_i48(example_disk + 0x1F4, 0x2);   // root_LBA
        write_i32(example_disk + 0x1FA, 0);     // root_size
        write_i16(example_disk + 0x1FE, 0xAA55);
    }

    // write bat
    {
        write_i32(0x1, 0, 0x3);
        write_i16(0x1, 4, 1);
        write_clear(0x1, 6, 0, 512 - 6);
        // set first 3 blocks as taken
        write_i8(0x1, 6, 0b11111000);

        write_i32(0x3, 0, 0);
        write_i16(0x3, 4, 2);
        write_clear(0x3, 6, 0, 1024 - 6);
    }
}

int main(int argc, char** argv) {
    init_example_disk();

    //dump_mem(example_disk, 512);
    //dump_mem(example_disk + 512, 512);

    gHeader = tabfs_new_header((dev_t) 0, 0);

    //dump_mem((uint8_t*)gHeader, sizeof(tabfs_header));
    printf("gHeader->__bat_root: %p\n", gHeader->__bat_root);
    dump_mem((uint8_t*)gHeader->__bat_root, sizeof(tabfs_bat));

    // run specs
    cxxspec::runSpecs(--argc, ++argv);
    return 0;
}