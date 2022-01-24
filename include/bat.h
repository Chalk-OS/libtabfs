#ifndef __LIBTABFS_BAT_H__
#define __LIBTABFS_BAT_H__

#include "./common.h"
#include "./volume.h"

struct libtabfs_bat {
    libtabfs_volume_t* __volume;
    struct libtabfs_bat* __next_bat;
    libtabfs_lba_28_t __lba;

    // data fields read from disk
    unsigned int next_bat;
    unsigned short block_count;
    unsigned char data[1];          // contains actually more than one byte
};
typedef struct libtabfs_bat libtabfs_bat_t;

/**
 * @brief loads an BAT section from disk
 * 
 * @param volume the volume to read from
 * @param bat_addr the LBA 28 where to start reading
 * @return the read BAT section
 */
libtabfs_bat_t* libtabfs_load_bat(libtabfs_volume_t* volume, libtabfs_lba_28_t bat_addr);

/**
 * @brief destroys / free's an BAT section
 * 
 * @param bat the BAT section to free
 */
void libtabfs_bat_destroy(libtabfs_bat_t* bat);

/**
 * @brief returns the count of databytes used by the current BAT section
 * 
 * @param bat the BAT section to use
 * @return count of databytes in the section
 */
long libtabfs_bat_getDatabyteCount(libtabfs_bat_t* bat);

/**
 * @brief returns the count of LBA's an BAT section can hold informations about
 * 
 * @param bat the BAT section to use
 * @return count of LBA's in the section
 */
long libtabfs_bat_getcount(libtabfs_bat_t* bat);

/**
 * @brief return the start LBA 28 for an BAT section
 * 
 * @param bat the BAT section to use
 * @return an LBA 28 that corresponds to the first bit in the BAT
 */
libtabfs_lba_28_t libtabfs_bat_getstart(libtabfs_bat_t* bat);

/**
 * @brief calculates the LBA 28 for any given bit inside an BAT-sections's data bitmap
 * 
 * @param bat the BAT section to calculate the lba for
 * @param bytePos the byteposition inside the BAT section
 * @param bitPos the bitposition into the byteposition
 * @return the lba
 */
libtabfs_lba_28_t libtabfs_bat_getlba(libtabfs_bat_t* bat, int bytePos, int bitPos);

/**
 * @brief flush a complete BAT section to disk
 * 
 * @param bat the BAT section to flush
 */
void libtabfs_bat_flush_to_disk(libtabfs_bat_t* bat);

/**
 * @brief syncs all BAT sections to disk
 * 
 * @param bat the BAT section to start syncing
 */
void libtabfs_bat_sync(libtabfs_bat_t* bat);

/**
 * @brief only flushes one block of the BAT to disk
 * 
 * @param bat the BAT section which block to flush
 * @param block_off offset of the block into the section
 */
void libtabfs_bat_flush_part_to_disk(libtabfs_bat_t* bat, int block_off);

/**
 * @brief returns the BAT region of an LBA; LBA should *not* be relative to bat_start_LBA
 * 
 * @param volume the volume to use
 * @param lba the lba to find the coresponding BAT region
 * @return the found BAT region or NULL if none could be found
 */
libtabfs_bat_t* libtabfs_bat_getBatRegion(libtabfs_volume_t* volume, libtabfs_lba_28_t lba);

/**
 * @brief checks if an given range (count) of blocks are free from the given start position
 * 
 * @param bat the bat region to start
 * @param bytePos the starting byte offset
 * @param bitPos the starting bit offset in the starting byte offset
 * @param count how many blocks to check to be free
 * @return LIBTABFS_ERR_NONE if nothing went wrong;
 *          LIBTABFS_ERR_RANGE_NOSPACE if range cannot be satisfied at the given position;
 *          ERROR_DEVICE_NOSPACE if the device as no space anymore for the given range
 */
libtabfs_error libtabfs_bat_are_blocks_free(libtabfs_bat_t* bat, int bytePos, int bitPos, unsigned int count);

/**
 * @brief marks an range; should be first tested by libtabfs_bat_are_blocks_free
 * 
 * @param bat the bat region to start
 * @param bytePos the starting byte offset
 * @param bitPos the starting bit offset in the starting byte offset
 * @param count how many blocks to check to be free
 */
void libtabfs_bat_mark_range(libtabfs_bat_t* bat, int bytePos, int bitPos, unsigned int count);

/**
 * @brief clears an range; this dosnt test if the range was set before!
 * 
 * @param bat the bat region to start
 * @param bytePos the starting byte offset
 * @param bitPos the starting bit offset in the starting byte offset
 * @param count how many blocks to check to be free
 */
void libtabfs_bat_clear_range(libtabfs_bat_t* bat, int bytePos, int bitPos, unsigned int count);

/**
 * @brief try and allocate a specific amount of lose blocks (not neccessarily near each other)
 * 
 * @param volume the tabfs instance to operate on
 * @param count the amount of lose blocks to allocate; also acts as a minimum size to lba_out
 * @param lba_out buffer that will hold the allocated blocks after this call
 * @return true if the allocation was successfull, false when not (out of blocks)
 */
bool libtabfs_bat_allocateLoseBlocks(libtabfs_volume_t* volume, unsigned short count, long long* lba_out);

/**
 * @brief deallocate/free a specific amount of lose blocks
 * 
 * @param volume the tabfs instance to operate on
 * @param count the amount of lose blocks to free; also acts as a minimum size to lba_in
 * @param lba_in buffer that will hold the blocks to free; possibly filled by libtabfs_bat_allocateLoseBlocks
 */
void libtabfs_bat_freeLoseBlocks(libtabfs_volume_t* volume, unsigned short count, long long* lba_in);

/**
 * @brief try and allocate a specific amount of chained blocks (all after each other)
 * 
 * @param volume the tabfs instance to operate on
 * @param count the amount of blocks to allocate
 * @return the first LBA of the chain of blocks or negative one if out of blocks
 */
libtabfs_lba_28_t libtabfs_bat_allocateChainedBlocks(libtabfs_volume_t* volume, unsigned short count);

/**
 * @brief try and free a specific amount of chained blocks
 * 
 * @param volume the tabfs instance to operate on
 * @param count the amount of blocks to free
 * @param lba the first LBA of the chain of blocks; returned by libtabfs_bat_allocateChainedBlocks
 */
void libtabfs_bat_freeChainedBlocks(libtabfs_volume_t* volume, unsigned short count, libtabfs_lba_28_t lba);

/**
 * @brief check if an given LBA is free.
 * if the requested LBA is out of the BAT's range, false is returned
 * 
 * @param volume the tabfs instance to operate on
 * @param lba the LBA address to be checked
 * @return true when the LBA address is free, false otherwise
 */
bool libtabfs_bat_isFree(libtabfs_volume_t* volume, libtabfs_lba_28_t lba);

#endif // __LIBTABFS_BAT_H__