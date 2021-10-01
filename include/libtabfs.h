#ifndef __LIBTABFS_H__
#define __LIBTABFS_H__

#include <stdbool.h>

#define TABFS_PACKED    __attribute__((packed))
#define TABFS_PTR_SIZE  sizeof(void*)
#define TABFS_INT_SIZE  sizeof(int)
#define TABFS_LONG_LONG_SIZE    sizeof(long long)

struct tabfs_lba_48 {
    unsigned int byte0_3;
    unsigned char byte4;
    unsigned char byte5;
} TABFS_PACKED;

typedef struct tabfs_lba_48 tabfs_lba_48_t;

#define TABFS_LBA_48_TOI(lba)  (long long)( lba.byte0_3 | (lba.byte4 << 32) | (lba.byte5 << 40) )

struct tabfs_header {
    struct tabfs_bat* __bat_root;
    //--------------------------------
    unsigned char magic[16];            //  0 - 15
    unsigned char private_data[32];     // 16 - 47
    tabfs_lba_48_t bat_LBA;             // 48 - 53
    unsigned char blockSize;            // 54
    tabfs_lba_48_t root_LBA;            // 55 - 60
    unsigned int  root_size;            // 61 - 64
    unsigned char bootSignature[2];     // 65 - 66
    //--------------------------------
    LIBTABFS_HEADER_DEVICE_FIELDS
} TABFS_PACKED;
typedef struct tabfs_header* tabfs_header_t;

#define TABFS_IS_BOOTABLE(header)   (header.bootSignature[0] == 0x55 && header.bootSignature[1] == 0xAA)

#define TABFS_HEADER_DATAOFF        TABFS_PTR_SIZE * 1
#define TABFS_HEADER_DATASIZE       67

#ifndef LIBTABFS_DEVICE_ARGS
    #error "need macro LIBTABFS_DEVICE_TYPES"
#endif

tabfs_header_t tabfs_new_header( LIBTABFS_DEVICE_ARGS, long long lba_address );

/*===========================================================
                            BAT
===========================================================*/

struct tabfs_bat {
    tabfs_header_t __header;
    struct tabfs_bat* __next_bat;
    long long __lba;

    // data fields read from disk
    unsigned int next_bat;
    unsigned short block_count;
    unsigned char data[1];      // contains actually more than one byte
};
typedef struct tabfs_bat* tabfs_bat_t;

#define TABFS_BAT_DATAOFF   TABFS_PTR_SIZE * 2 + TABFS_LONG_LONG_SIZE

/**
 * check if an given lba is free
 * 
 * @param header the tabfs instance to operate on
 * @param lba the lba address to be checked
 * @return true when the lba address is free, false otherwise
 */
bool tabfs_bat_isFree(tabfs_header_t header, long long lba);

/**
 * try and allocate a specific amount of lose blocks (not neccessarily near each other)
 * 
 * @param header the tabfs instance to operate on
 * @param count the amount of lose blocks to allocate; also acts as a minimum size to lba_out
 * @param lba_out buffer that will hold the allocated blocks after this call
 * @return true if the allocation was successfull, false when not (out of blocks)
 */
bool tabfs_bat_allocateLoseBlocks(tabfs_header_t header, unsigned short count, long long* lba_out);

/**
 * deallocate/free a specific amount of lose blocks
 * 
 * @param header the tabfs instance to operate on
 * @param count the amount of lose blocks to free; also acts as a minimum size to lba_in
 * @param lba_in buffer that will hold the blocks to free; possibly filled by tabfs_bat_allocateLoseBlocks
 */
void tabfs_bat_freeLoseBlocks(tabfs_header_t header, unsigned short count, long long* lba_in);

/**
 * try and allocate a specific amount of chained blocks (all after each other)
 * 
 * @param header the tabfs instance to operate on
 * @param count the amount of blocks to allocate
 * @return the first lba of the chain of blocks or zero if out of blocks
 */
long long tabfs_bat_allocateChainedBlocks(tabfs_header_t header, unsigned short count);

/**
 * try and free a specific amount of chained blocks
 * 
 * @param header the tabfs instance to operate on
 * @param count the amount of blocks to free
 * @param lba the first lba of the chain of blocks; returned by tabfs_bat_allocateChainedBlocks
 */
void tabfs_bat_freeChainedBlocks(tabfs_header_t header, unsigned short count, long long lba);

//############################################################################################
//      IMPLEMENTATION OF LIBTABFS
//############################################################################################

#ifdef LIBTABFS_IMPLEMENTATION

    #ifndef LIBTABFS_ALLOC
        #error "need an allocation function: void* LIBTABFS_ALLOC(int bytes_to_alloc)"
    #endif

    #ifndef LIBTABFS_FREE
        #error "need an de-allocation function: void LIBTABFS_FREE(void* memory, int bytes_to_free)"
    #endif

    #ifndef LIBTABFS_REALLOC
        
        #include <string.h>
        void* libtabfs_default_realloc(void* old, int old_size, int new_size) {
            void* new_mem = LIBTABFS_ALLOC(new_size);
            memcpy( new_mem, old, old_size );
            LIBTABFS_FREE(old, old_size);
            return new_mem;
        }
        #define LIBTABFS_REALLOC(old, old_size, new_size)   libtabfs_default_realloc(old, old_size, new_size)

    #endif

    #ifndef LIBTABFS_READ_DEVICE
        #error "need a read function: void LIBTABFS_READ_DEVICE(LIBTABFS_DEVICE_TYPES, long long lba_address, int offset, void* buffer, int bufferSize)"
    #endif

    #ifndef LIBTABFS_WRITE_DEVICE
        #error "need a write function: void LIBTABFS_WRITE_DEVICE(LIBTABFS_DEVICE_TYPES, long long lba_address, int offset, void* buffer, int bufferSize)"
    #endif

    #define BLOCK_SIZE(header)  (header->blockSize * 512)
    #define LBA(header, lba)    (lba * header->blockSize)

    #define PTR_SIZE    sizeof(void*)
    #define ASSURE_NON_NULLPTR(what)    if (what == 0) { return 0; }

    //---------------------------------------------------------------------------------------------

    tabfs_bat_t tabfs_load_bat(tabfs_header_t header, long long bat_addr) {
        int s = BLOCK_SIZE(header);

        int bat_size = s + TABFS_BAT_DATAOFF;
        tabfs_bat_t bat = (tabfs_bat_t) LIBTABFS_ALLOC(bat_size);
        bat->__header = header;
        bat->__lba = bat_addr;

        printf("[tabfs_load_bat] bat: %p\n", bat);

        LIBTABFS_READ_DEVICE(
            LIBTABFS_DEVICE_PARAMS(header),
            bat_addr, 0,
            ((void*)bat) + TABFS_BAT_DATAOFF,
            s
        );

        printf("[tabfs_load_bat] bat->__lba: %d\n", bat->__lba);

        if (bat->block_count > 1) {
            bat = (tabfs_bat_t) LIBTABFS_REALLOC(bat, bat_size, bat_size + s * (bat->block_count - 1));
            LIBTABFS_READ_DEVICE(
                LIBTABFS_DEVICE_PARAMS(header),
                bat_addr + 1, 0,
                ((void*)bat) + TABFS_BAT_DATAOFF + s,
                s * (bat->block_count - 1)
            );
        }

        return bat;
    }

    tabfs_header_t tabfs_new_header(LIBTABFS_DEVICE_ARGS, long long lba_address) {

        tabfs_header_t header = (tabfs_header_t) LIBTABFS_ALLOC( sizeof(struct tabfs_header) );
        ASSURE_NON_NULLPTR(header)
        printf("[tabfs_new_header] header: %p\n", header);

        LIBTABFS_DEVICE_ARGS_TO_HEADER(header)

        LIBTABFS_READ_DEVICE(
            LIBTABFS_DEVICE_PARAMS(header),
            lba_address, 0x1BD,
            ((void*)header) + TABFS_HEADER_DATAOFF,
            TABFS_HEADER_DATASIZE
        );

        // read the complete BAT into memory
        header->__bat_root = tabfs_load_bat(header, TABFS_LBA_48_TOI(header->bat_LBA));
        printf("[tabfs_new_header] __bat_root: %p\n", header->__bat_root);
        tabfs_bat_t bat = header->__bat_root;
        while (bat->next_bat != 0) {
            bat->__next_bat = tabfs_load_bat(header, bat->next_bat);
            bat = bat->__next_bat;
        }

        return header;
    }

    //---------------------------------------------------------------------------------------------

    // loads an bat region from memory or get it's from the cache
    /*tabfs_bat_t tabfs_getBat(tabfs_header_t header, int bat_addr) {
        long long bat_LBA = TABFS_LBA_48_TOI(header->bat_LBA);
        if ( bat_addr == bat_LBA) {
            if ( header->__bat_root == 0 ) {
                header->__bat_root = tabfs_load_bat(header, bat_LBA);
            }
            return header->__bat_root;
        }
    }*/

    #ifndef LIBTABFS_BAT_DEFAULTSIZE
        #define LIBTABFS_BAT_DEFAULTSIZE 2
    #endif

    // flushes whole bat to disk
    void tabfs_bat_flush_to_disk(tabfs_bat_t bat) {
        int s = BLOCK_SIZE(bat->__header);
        LIBTABFS_WRITE_DEVICE(LIBTABFS_DEVICE_PARAMS(bat->__header), bat->__lba, 0, bat + TABFS_BAT_DATAOFF, s * bat->block_count);
    }

    // only flushes one block of the bat to disk
    void tabfs_bat_flush_part_to_disk(tabfs_bat_t bat, int block_off) {
        int s = BLOCK_SIZE(bat->__header);
        LIBTABFS_WRITE_DEVICE(
            LIBTABFS_DEVICE_PARAMS(bat->__header),
            bat->__lba + block_off, 0,
            bat + TABFS_BAT_DATAOFF + (s * block_off), s
        );
    }

    tabfs_bat_t tabfs_bat_getBatRegion(tabfs_header_t header, long long* lba) {
        tabfs_bat_t bat = header->__bat_root;
        while (1) {
            // find out how many LBA's the current section can hold
            int lba_count = BLOCK_SIZE(bat->__header) - 6 + ((bat->block_count - 1) * BLOCK_SIZE(bat->__header));
            lba_count *= 8;
            printf("[tabfs_bat_getBatRegion] lba_count: 0x%x\n", lba_count);

            if (*lba >= lba_count) {
                bat = bat->__next_bat;
                *lba -= lba_count;
                if (bat == 0) {
                    // no next bat; create a new one!
                    //long long addr = tabfs_bat_allocateChainedBlocks(header, LIBTABFS_BAT_DEFAULTSIZE);
                    //if (addr == 0) { return 0; }

                    //bat->next_bat = addr;
                    //bat = addr;
                    //tabfs_bat_flush_part_to_disk(bat, 0);   // flush header
                    return NULL;
                }
                continue;
            }
            else {
                return bat;
            }
        }
    }

    bool tabfs_bat_are_blocks_free(tabfs_bat_t bat, int bytePos, int bitPos, unsigned short count) {
        // fist check the remaining range of the start byte
        for (;bitPos < 8; bitPos++) {
            if ((bat->data[bytePos] & (0x80 >> bitPos)) == 0) {
                count--;
                if (count == 0) { return true; }
                continue;
            }
            else {
                return false;
            }
        }

        // count is not depleted yet? begin checking remaining bytes
        int bytecount = BLOCK_SIZE(bat->__header) - 6 + ((bat->block_count - 1) * BLOCK_SIZE(bat->__header));
        for (; bytePos < bytecount; bytePos++) {
            for (int j = 0; j < 8; j++) {
                if ((bat->data[bytePos] & (0x80 >> j)) == 0) {
                    count--;
                    if (count == 0) { return true; }
                    continue;
                }
                else {
                    return false;
                }
            }
        }

        // count is STILL not depleted? try to continue with the next bat
        if (bat->__next_bat == 0) {
            
        }
    }

    long long tabfs_bat_allocateChainedBlocks(tabfs_header_t header, unsigned short count) {
        tabfs_bat_t bat = header->__bat_root;
        while (1) {
            int bytecount = BLOCK_SIZE(bat->__header) - 6 + ((bat->block_count - 1) * BLOCK_SIZE(bat->__header));
            for (int i = 0; i < bytecount; i++) {
                if ( bat->data[i] < 0xff ) {
                    for (int j = 0; j < 8; j++) {
                        if ((bat->data[i] & (0x80 >> j)) == 0) {
                            // found free block; start check if <count> blocks are free
                            if (tabfs_bat_are_blocks_free(bat, i, j, count)) {

                                // TODO: mark range

                            }


                        }
                    }
                }
            }
        }
    }

    bool tabfs_bat_isFree(tabfs_header_t header, long long lba) {
        tabfs_bat_t bat = tabfs_bat_getBatRegion(header, &lba);
        if (bat == NULL) {
            return false;   // error while getting bat region; sometimes means we are out of memory on the drive!
        }
        int bytepos = (lba * header->blockSize) / 8;
        int bitpos = (lba * header->blockSize) % 8;
        printf("[tabfs_bat_isFree] lba: 0x%x | bat->__lba: 0x%x | bytepos: %d | bitpos: %d\n", lba, bat->__lba, bytepos, bitpos);
        return (bat->data[bytepos] & (0x80 >> bitpos)) == 0;
    }

#endif

#endif