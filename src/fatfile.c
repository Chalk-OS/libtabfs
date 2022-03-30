#include "bridge.h"

#include "common.h"
#include "volume.h"
#include "bat.h"
#include "fatfile.h"

#define LIBTABFS_FAT_DATAOFFSET  (LIBTABFS_PTR_SIZE) + sizeof(unsigned int) + sizeof(libtabfs_lba_28_t)

//--------------------------------------------------------------------------------
// FAT creation, sync & destroying
//--------------------------------------------------------------------------------

libtabfs_fat_t* libtabfs_read_fat(libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size) {

    libtabfs_fat_t* fat = (libtabfs_fat_t*) libtabfs_alloc(LIBTABFS_FAT_DATAOFFSET + size);

    libtabfs_read_device(
        volume->__dev_data,
        lba, volume->flags.absolute_lbas, 0,
        (void*) fat + LIBTABFS_FAT_DATAOFFSET, size
    );

    fat->__volume = volume;
    fat->__lba = lba;
    fat->__byteSize = size;

    // add the fat to our cache!
    libtabfs_linkedlist_add(volume->__fat_cache, fat);

    return fat;
}

libtabfs_fat_t* libtabfs_find_cached_fat(libtabfs_volume_t* volume, libtabfs_lba_28_t fat_lba) {
    libtabfs_linkedlist_entry_t* entry = volume->__fat_cache->head;
    while (entry != NULL) {
        libtabfs_fat_t* fat = (libtabfs_fat_t*) entry->data;
        if (fat->__lba == fat_lba) {
            return fat;
        }
        else {
            entry = entry->next;
        }
    }
    return NULL;
}

libtabfs_fat_t* libtabfs_get_fat_section(libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size) {
    libtabfs_fat_t* next_section = libtabfs_find_cached_fat(volume, lba);
    if (next_section == NULL) {
        return libtabfs_read_fat(volume, lba, size);
    }
    else {
        return next_section;
    }
}

void libtabfs_fat_sync(libtabfs_fat_t* fat) {
    libtabfs_write_device(
        fat->__volume->__dev_data,
        fat->__lba, fat->__volume->flags.absolute_lbas, 0,
        (void*) fat + LIBTABFS_FAT_DATAOFFSET, fat->__byteSize
    );
}

libtabfs_fat_t* libtabfs_create_fat_section(
    libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size
) {

    libtabfs_fat_t* fat = (libtabfs_fat_t*) libtabfs_alloc(LIBTABFS_FAT_DATAOFFSET + size);
    fat->__volume = volume;
    fat->__lba = lba;
    fat->__byteSize = size;

    // add the table to our cache!
    libtabfs_linkedlist_add(volume->__fat_cache, fat);

    // initialize the table (by zeroing it)
    int blocks = size / fat->__volume->blockSize;
    for (int i = 0; i < blocks; i++) {
        libtabfs_set_range_device(
            fat->__volume->__dev_data,
            fat->__lba + i, fat->__volume->flags.absolute_lbas,
            0, 0, fat->__volume->blockSize
        );
    }

    // sync the fat to disk to ensure we have an empty fat
    libtabfs_fat_sync(fat);

    return fat;
}

void libtabfs_fat_cachefree_callback(libtabfs_fat_t* fat) {
    libtabfs_fat_sync(fat);
    libtabfs_free(fat, LIBTABFS_FAT_DATAOFFSET + fat->__byteSize);
}

//--------------------------------------------------------------------------------
// FAT traversal
//--------------------------------------------------------------------------------

libtabfs_error libtabfs_fat_findfree(
    libtabfs_fat_t* fat,
    libtabfs_fat_entry_t** entry_out,
    libtabfs_fat_t** fat_out,
    int* offset_out
) {
    if (entry_out == NULL) { return LIBTABFS_ERR_ARGS; }
    *entry_out = NULL;

    int entryCount = fat->__byteSize / 16;
    for (int i = 1; i < entryCount; i++) {
        libtabfs_fat_entry_t* entry = &(fat->entries[i]);
        if (entry->index == 0 && entry->lba == 0) {
            *entry_out = entry;
            if (fat_out != NULL) { *fat_out = fat; }
            if (offset_out != NULL) { *offset_out = i; }
            return LIBTABFS_ERR_NONE;
        }
    }

    if (fat->next_size != 0 && !LIBTABFS_IS_INVALID_LBA28(fat->next_section)) {
        // try to find in next section
        libtabfs_fat_t* next_section = libtabfs_get_fat_section(fat->__volume, fat->next_section, fat->next_size);
        return libtabfs_fat_findfree(next_section, entry_out, fat_out, offset_out);
    }
    else {
        // no next section configured; create a new section!

        libtabfs_lba_28_t next_section_lba = libtabfs_bat_allocateChainedBlocks(fat->__volume, 2);
        if (LIBTABFS_IS_INVALID_LBA28(next_section_lba)) {
            return LIBTABFS_ERR_DEVICE_NOSPACE;
        }

        int next_section_size = 2 * fat->__volume->blockSize;

        libtabfs_fat_t* next_section = libtabfs_create_fat_section(
            fat->__volume, next_section_lba, next_section_size
        );

        fat->next_section = next_section_lba;
        fat->next_size = next_section_size;

        *entry_out = &(next_section->entries[1]);
        if (fat_out != NULL) { *fat_out = next_section; }
        if (offset_out != NULL) { *offset_out = 1; }

        return LIBTABFS_ERR_NONE;
    }
    return LIBTABFS_ERR_FAT_FULL;
}

libtabfs_error libtabfs_fat_findlatest(
    int index,
    libtabfs_fat_t* fat,
    libtabfs_fat_entry_t** entry_out,
    libtabfs_fat_t** fat_out,
    int* offset_out
) {
    if (entry_out == NULL) { return LIBTABFS_ERR_ARGS; }
    *entry_out = NULL;

    libtabfs_time_t currentTime = { .i64_data = 0 };
    int entryCount = (fat->__byteSize / 16) - 1;
    for (int i = 0; i < entryCount; i++) {
        libtabfs_fat_entry_t* entry = &(fat->entries[i]);
        if (entry->index == index && entry->modify_date.i64_data > currentTime.i64_data) {
            *entry_out = entry;
            if (fat_out != NULL) { *fat_out = fat; }
            if (offset_out != NULL) { *offset_out = i; }
            currentTime = entry->modify_date;
        }
    }

    if (currentTime.i64_data == 0) {
        return LIBTABFS_ERR_NOT_FOUND;
    }
    return LIBTABFS_ERR_NONE;
}

// libtabfs_error libtabfs_fat_find(
//     int index, libtabfs_time_t timestamp,
//     libtabfs_fat_t* fat,
//     libtabfs_fat_entry_t** entry_out,
//     libtabfs_fat_t** fat_out,
//     int* offset_out
// ) {
// }

//--------------------------------------------------------------------------------
// FAT file handling
//--------------------------------------------------------------------------------

#define NAME_CHECK \
    int namelen = libtabfs_strlen(name); if (namelen > 62) { return LIBTABFS_ERR_NAME_TOLONG; }

libtabfs_error libtabfs_create_fatfile(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    libtabfs_entrytable_entry_t** entry_out
) {
    NAME_CHECK
    if (entry_out == NULL) { return LIBTABFS_ERR_ARGS; }

    libtabfs_lba_28_t fatTable_lba = libtabfs_bat_allocateChainedBlocks(entrytable->__volume, 2);
    if (LIBTABFS_IS_INVALID_LBA28(fatTable_lba)) {
        return LIBTABFS_ERR_DEVICE_NOSPACE;
    }

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_create_entry(entrytable, name, &entry);
    if (err != LIBTABFS_ERR_NONE) {
        libtabfs_bat_freeChainedBlocks(entrytable->__volume, 2, fatTable_lba);
        return err;
    }

    entry->rawflags = 0;
    entry->flags.type = LIBTABFS_ENTRYTYPE_FILE_FAT;
    libtabfs_fileflags_to_entry(fileflags, entry);

    libtabfs_entry_chown(entry, userid, groupid);
    entry->create_ts = create_ts;
    libtabfs_entry_touch(entry, create_ts, create_ts);

    *entry_out = entry;

    entry->data.lba_and_size.lba = fatTable_lba;
    entry->data.lba_and_size.size = 2 * entrytable->__volume->blockSize;

    // initialize the table (by zeroing it)
    for (int i = 0; i < 2; i++) {
        libtabfs_set_range_device(
            entrytable->__volume->__dev_data,
            fatTable_lba + i, entrytable->__volume->flags.absolute_lbas,
            0, 0, entrytable->__volume->blockSize
        );
    }

    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_fatfile_read(
    libtabfs_volume_t* volume,
    libtabfs_entrytable_entry_t* entry,
    unsigned long int offset, unsigned long int len, unsigned char* buffer,
    unsigned long int* bytesRead
) {
    // first find the entry in the fat table...

    libtabfs_fat_t* fat = libtabfs_get_fat_section(volume, entry->data.lba_and_size.lba, entry->data.lba_and_size.size);

    // we combine the offset into the first block with the length to retrieve to get the span of blocks
    int lenInclBlockOffset = (offset % volume->blockSize) + len;

    // then we calculate the amount of blocks this span "touches"
    int blocksToTouch = lenInclBlockOffset / volume->blockSize;
    if ((lenInclBlockOffset % volume->blockSize) > 0) {
        // adding one since the span of blocks only covers a fraction of the last block
        blocksToTouch++;
    }

    int startBlockIndex = (offset / volume->blockSize);

    #ifdef LIBTABFS_DEBUG_PRINTF
        printf("libtabfs_fatfile_read(offset: %d, len: %d); lenInclBlockOffset=%d | blocksToTouch=%d | startBlockIndex=%d\n",
            offset, len, lenInclBlockOffset, blocksToTouch, startBlockIndex);
    #endif

    // iterate over the blocks
    for (int i = 0; i < blocksToTouch; i++) {
        libtabfs_fat_entry_t* fatentry = NULL;
        int blockIndex = startBlockIndex + i;
        libtabfs_error err = libtabfs_fat_findlatest(blockIndex, fat, &fatentry, NULL, NULL);
        if (err == LIBTABFS_ERR_NOT_FOUND) {
            // TODO: maybe optimize this a bit and dont create blocks when only reading... only make file bigger when writing!

            err = libtabfs_fat_findfree(fat, &fatentry, NULL, NULL);
            if (err != LIBTABFS_ERR_NONE) {
                return err;
            }

            libtabfs_lba_28_t blockLba = libtabfs_bat_allocateChainedBlocks(fat->__volume, 1);
            if (LIBTABFS_IS_INVALID_LBA28(blockLba)) {
                return LIBTABFS_ERR_DEVICE_NOSPACE;
            }

            fatentry->index = blockIndex;
            fatentry->lba = blockLba;
            libtabfs_get_current_time(&(fatentry->modify_date));

            #ifdef LIBTABFS_DEBUG_PRINTF
                printf("-> creating new block for index %d with lba 0x%x\n", blockIndex, blockLba);
            #endif
        }
        else if (err != LIBTABFS_ERR_NONE) {
            return err;
        }

        // copy bytes into buffer
        int block_off = 0;
        if (i == 0) {
            // if the first block in our range, then add the initial offset
            block_off = offset % volume->blockSize;
        }

        int block_len = volume->blockSize;  // copy whole blocks
        if (block_off != 0) {
            block_len -= block_off;
        }

        if (i >= (blocksToTouch - 1)) {
            // if last block, only copy the fraction we actually want
            block_len = len - (*bytesRead);
        }

        #ifdef LIBTABFS_DEBUG_PRINTF
            printf("-> i=%d | blockIndex=%d | block_off=%d | block_len=%d\n", i, blockIndex, block_off, block_len);
        #endif

        libtabfs_read_device(
            volume->__dev_data,
            fatentry->lba, volume->flags.absolute_lbas,
            block_off, buffer + (i * volume->blockSize), block_len
        );

        *bytesRead += block_len;
    }

    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_fatfile_write(
    libtabfs_volume_t* volume,
    libtabfs_entrytable_entry_t* entry,
    unsigned long int offset, unsigned long int len, unsigned char* buffer,
    unsigned long int* bytesWritten
) {
    // first find the entry in the fat table...

    libtabfs_fat_t* fat = libtabfs_get_fat_section(volume, entry->data.lba_and_size.lba, entry->data.lba_and_size.size);

    // we combine the offset into the first block with the length to retrieve to get the span of blocks
    int lenInclBlockOffset = (offset % volume->blockSize) + len;

    // then we calculate the amount of blocks this span "touches"
    int blocksToTouch = lenInclBlockOffset / volume->blockSize;
    if ((lenInclBlockOffset % volume->blockSize) > 0) {
        // adding one since the span of blocks only covers a fraction of the last block
        blocksToTouch++;
    }

    int startBlockIndex = (offset / volume->blockSize);

    #ifdef LIBTABFS_DEBUG_PRINTF
        printf("libtabfs_fatfile_write(offset: %d, len: %d); lenInclBlockOffset=%d | blocksToTouch=%d | startBlockIndex=%d\n",
            offset, len, lenInclBlockOffset, blocksToTouch, startBlockIndex);
    #endif

    // iterate over the blocks
    for (int i = 0; i < blocksToTouch; i++) {
        libtabfs_fat_entry_t* fatentry = NULL;
        int blockIndex = startBlockIndex + i;
        libtabfs_error err = libtabfs_fat_findlatest(blockIndex, fat, &fatentry, NULL, NULL);
        if (err == LIBTABFS_ERR_NOT_FOUND) {
            err = libtabfs_fat_findfree(fat, &fatentry, NULL, NULL);
            if (err != LIBTABFS_ERR_NONE) {
                return err;
            }

            libtabfs_lba_28_t blockLba = libtabfs_bat_allocateChainedBlocks(fat->__volume, 1);
            if (LIBTABFS_IS_INVALID_LBA28(blockLba)) {
                return LIBTABFS_ERR_DEVICE_NOSPACE;
            }

            fatentry->index = blockIndex;
            fatentry->lba = blockLba;
            libtabfs_get_current_time(&(fatentry->modify_date));

            #ifdef LIBTABFS_DEBUG_PRINTF
                printf("-> creating new block for index %d with lba 0x%x and modify_date %d\n", blockIndex, blockLba, fatentry->modify_date.i64_data);
            #endif
        }
        else if (err != LIBTABFS_ERR_NONE) {
            return err;
        }

        // copy bytes from buffer
        int block_off = 0;
        if (i == 0) {
            // if the first block in our range, then add the initial offset
            block_off = offset % volume->blockSize;
        }

        int block_len = volume->blockSize;  // copy whole blocks
        if (block_off != 0) {
            block_len -= block_off;
        }

        if (i >= (blocksToTouch - 1)) {
            // if last block, only copy the fraction we actually want
            block_len = len - (*bytesWritten);
        }

        libtabfs_write_device(
            volume->__dev_data,
            fatentry->lba, volume->flags.absolute_lbas,
            block_off, buffer + (i * volume->blockSize), block_len
        );

        *bytesWritten += block_len;
    }

    return LIBTABFS_ERR_NONE;
}