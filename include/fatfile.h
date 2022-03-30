#ifndef __LIBTABFS_FATFILE_H__
#define __LIBTABFS_FATFILE_H__

#include "./common.h"
#include "./entrytable.h"

struct libtabfs_fat_entry {
    unsigned int index;
    libtabfs_lba_28_t lba;
    libtabfs_time_t modify_date;
};
typedef struct libtabfs_fat_entry libtabfs_fat_entry_t;

struct libtabfs_fat {
    libtabfs_volume_t* __volume;
    unsigned int __byteSize;
    libtabfs_lba_28_t __lba;

    // --------------------------------
    libtabfs_lba_28_t next_section;
    unsigned int next_size;
    unsigned char unused[8];

    libtabfs_fat_entry_t entries[1]; // actually more than one;
};
typedef struct libtabfs_fat libtabfs_fat_t;

//--------------------------------------------------------------------------------
// FAT creation, sync & destroying
//--------------------------------------------------------------------------------

/**
 * @brief reads an fat section from disk; this dosnt uses the fatcache! use libtabfs_get_fat_section instead
 * this method registers the loaded section into the fatcache
 * 
 * @param volume the volume to operate on
 * @param lba the lba of the fat section
 * @param size the bytesize of the fat section
 * @return the loaded fat section
 */
libtabfs_fat_t* libtabfs_read_fat(libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size);

/**
 * @brief queries the fatcache for an cached fat section. it does not load it from disk when it cannot be found!
 * use libtabfs_get_fat_section instead.
 * 
 * @param volume the volume to operate on
 * @param fat_lba the lba of the fat section to search for
 * @return the requested fat section or NULL if it couldn't be found
 */
libtabfs_fat_t* libtabfs_find_cached_fat(libtabfs_volume_t* volume, libtabfs_lba_28_t fat_lba);

/**
 * @brief retrieves an fat section by first quering the fatcache; if not found, its loaded from disk using libtabfs_read_fat
 * 
 * @param volume the volume to operate on
 * @param lba the lba of the fat section
 * @param size the bytesize of the fat section
 * @return the requested fat section
 */
libtabfs_fat_t* libtabfs_get_fat_section(libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size);

/**
 * @brief writes an fat section to disk
 * 
 * @param fat the fat section to sync to disk
 */
void libtabfs_fat_sync(libtabfs_fat_t* fat);

/**
 * @brief creates an new fat section; adds the result to the fatcache
 * 
 * @param volume the volume to operate on
 * @param lba the lba of the fat section
 * @param size the bytesize of the fat section
 * @return libtabfs_fat_t* 
 */
libtabfs_fat_t* libtabfs_create_fat_section(
    libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size
);

/**
 * @brief internal function; called when an entry inside the cache needs to be free'd
 * 
 * @param fat the fat section to free
 */
void libtabfs_fat_cachefree_callback(libtabfs_fat_t* fat);

//--------------------------------------------------------------------------------
// FAT traversal
//--------------------------------------------------------------------------------

/**
 * @brief searches after an free entry inside an fat section
 * 
 * @param fat the fat section to start searching from
 * @param entry_out pointer which will be set to the found entry on success
 * @param fat_out optional pointer which will be set to the fat section containing the free entry (only on success)
 * @param offset_out optional pointer which will be set to the offset of the entry into its fat section (only on success)
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_fat_findfree(
    libtabfs_fat_t* fat,
    libtabfs_fat_entry_t** entry_out,
    libtabfs_fat_t** fat_out,
    int* offset_out
);

/**
 * @brief searches after the latest entry for an particular index inside an fat section
 * 
 * @param index the index to search
 * @param fat the fat section to start searching from
 * @param entry_out pointer which will be set to the found entry on success
 * @param fat_out optional pointer which will be set to the fat section containing the free entry (only on success)
 * @param offset_out optional pointer which will be set to the offset of the entry into its fat section (only on success)
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_fat_findlatest(
    int index,
    libtabfs_fat_t* fat,
    libtabfs_fat_entry_t** entry_out,
    libtabfs_fat_t** fat_out,
    int* offset_out
);

//--------------------------------------------------------------------------------
// FAT file handling
//--------------------------------------------------------------------------------

/**
 * @brief creates an FAT file
 * 
 * @param entrytable the entrytable to create in
 * @param name name of the directory to create
 * @param fileflags the fileflags for the directory
 * @param create_ts creation timestamp
 * @param userid owning user
 * @param groupid owning group
 * @param entry_out pointer which will be set to the new entry on success
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_fatfile(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    libtabfs_entrytable_entry_t** entry_out
);

/**
 * @brief internal function; please use libtabfs_read_file instead!
 */
libtabfs_error libtabfs_fatfile_read(
    libtabfs_volume_t* volume,
    libtabfs_entrytable_entry_t* entry,
    unsigned long int offset, unsigned long int len, unsigned char* buffer,
    unsigned long int* bytesRead
);

/**
 * @brief internal function; please use libtabfs_write_file instead!
 */
libtabfs_error libtabfs_fatfile_write(
    libtabfs_volume_t* volume,
    libtabfs_entrytable_entry_t* entry,
    unsigned long int offset, unsigned long int len, unsigned char* buffer,
    unsigned long int* bytesWritten
);

#endif // __LIBTABFS_FATFILE_H__