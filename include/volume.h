#ifndef __LIBTABFS_VOLUME_H__
#define __LIBTABFS_VOLUME_H__

#include "./common.h"
#include "./linkedlist.h"

typedef struct {
    bool absolute_lbas : 1;
} libtabfs_flags;

/**
 * @brief Header of an tabfs volume; typically at the end of an bootsector
 */
struct libtabfs_header {
    unsigned char magic[16];
    unsigned char private_data[32];
    union {
        libtabfs_flags flags;
        unsigned short __flags;
    };
    unsigned char __unused[4];
    libtabfs_lba_48_t info_LBA;
    unsigned char bootSignature[2];
} LIBTABFS_PACKED;
typedef struct libtabfs_header libtabfs_header_t;

#define LIBTABFS_IS_BOOTABLE(header)   (header.bootSignature[0] == 0x55 && header.bootSignature[1] == 0xAA)

/**
 * @brief Volume descriptor
 */
struct libtabfs_volume {
    unsigned char magic[16];
    libtabfs_lba_28_t bat_LBA;
    libtabfs_lba_28_t min_LBA;
    libtabfs_lba_28_t bat_start_LBA;
    libtabfs_lba_28_t max_LBA;
    unsigned int blockSize;
    unsigned char BS;
    unsigned char __unused;
    union {
        libtabfs_flags flags;
        unsigned short __flags;
    };
    libtabfs_lba_28_t root_LBA;
    unsigned int root_size;
    unsigned char __reserved[32];
    char volume_label[176];
    //--------------------------------
    void* __dev_data;
    libtabfs_lba_28_t __lba;
    struct libtabfs_bat* __bat_root;
    struct libtabfs_entrytable* __root_table;
    libtabfs_linkedlist_t* __table_cache;
    libtabfs_linkedlist_t* __fat_cache;
} LIBTABFS_PACKED;
typedef struct libtabfs_volume libtabfs_volume_t;

/**
 * @brief creates an new volume from an specific device and an specific lba address;
 * The address given must contain an tabfs header
 * 
 * @param lba_address the address to load from
 * @param absolute_lba if this is true, lba_address is an absolute address; used by the read-command
 * @param volume_out an new volume instance
 * @return errorcode of the operation;
 *      LIBTABFS_ERR_NO_BOOTSIG: lba block dosnt contain valid boot signature
 *      LIBTABFS_ERR_WRONG_MAGIC: lba block dosnt contain tabfs magic useable with libtabfs
 *      LIBTABFS_ERR_NONE: if all went right
 */
libtabfs_error libtabfs_new_volume(void* dev_data, long long lba_address, bool absolute_lba, libtabfs_volume_t** volume_out);

/**
 * @brief syncs an complete volume to the disk
 * 
 * @param volume the volume to sync
 */
void libtabfs_volume_sync(libtabfs_volume_t* volume);

/**
 * @brief sets the volume label for an volume
 * 
 * @param volume the volume to set the label for
 * @param label the new label
 * @param sync should an sync to disk be forced?
 * @return errorcode of the operation;
 *      LIBTABFS_ERR_LABEL_TOLONG: if the label is to long
 *      LIBTABFS_ERR_NONE: if all went right
 */
libtabfs_error libtabfs_volume_set_label(libtabfs_volume_t* volume, char* label, bool sync);

/**
 * @brief returns an read-only pointer to the volume label; this should *NOT* be written to!
 * Instead use libtabfs_volume_set_label
 * 
 * @param volume the volume to get the label for
 * @return an zero-terminated string
 */
const char* libtabfs_volume_get_label(libtabfs_volume_t* volume);

/**
 * @brief destroys an volume; syncs it to disk before full destory
 * 
 * @param volume 
 */
void libtabfs_destroy_volume(libtabfs_volume_t* volume);

#endif // __LIBTABFS_VOLUME_H__