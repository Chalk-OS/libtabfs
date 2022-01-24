#include "bridge.h"

#include "common.h"
#include "volume.h"
#include "bat.h"

#define LIBTABFS_BAT_DATAOFF   LIBTABFS_PTR_SIZE * 2 + sizeof(libtabfs_lba_28_t)

libtabfs_bat_t* libtabfs_load_bat(libtabfs_volume_t* volume, libtabfs_lba_28_t bat_addr) {
    int s = volume->blockSize;

    int bat_size = s + LIBTABFS_BAT_DATAOFF;
    libtabfs_bat_t* bat = (libtabfs_bat_t*) libtabfs_alloc(bat_size);
    bat->__volume = volume;
    bat->__lba = bat_addr;

    libtabfs_read_device(
        volume->__dev_data,
        bat_addr, volume->flags.absolute_lbas, 0,
        ((void*)bat) + LIBTABFS_BAT_DATAOFF,
        s
    );

    if (bat->block_count > 1) {
        bat = (libtabfs_bat_t*) libtabfs_realloc(bat, bat_size, bat_size + s * (bat->block_count - 1));
        libtabfs_read_device(
            volume->__dev_data,
            bat_addr + 1, volume->flags.absolute_lbas, 0,
            ((void*)bat) + LIBTABFS_BAT_DATAOFF + s,
            s * (bat->block_count - 1)
        );
    }

    return bat;
}

void libtabfs_bat_destroy(libtabfs_bat_t* bat) {
    int s = bat->__volume->blockSize;
    int bat_size = s + LIBTABFS_BAT_DATAOFF;
    bat_size += s * (bat->block_count - 1);
    libtabfs_free(bat, bat_size);
}

long libtabfs_bat_getDatabyteCount(libtabfs_bat_t* bat) {
    int blockSize_bytes = bat->__volume->blockSize;
    return (bat->block_count * blockSize_bytes) - 6;
}

long libtabfs_bat_getcount(libtabfs_bat_t* bat) {
    int blockSize_bytes = bat->__volume->blockSize;
    long dataBytes = (bat->block_count * blockSize_bytes) - 6;
    return (dataBytes * 8);
}

libtabfs_lba_28_t libtabfs_bat_getstart(libtabfs_bat_t* bat) {
    // calculating the lba offset by summing the count of all previous bat's
    libtabfs_lba_28_t start_lba = bat->__volume->bat_start_LBA;
    libtabfs_bat_t* current = bat->__volume->__bat_root;
    while (current != bat) {
        start_lba += libtabfs_bat_getcount(current);
        current = current->__next_bat;
    }
    return start_lba;
}

libtabfs_lba_28_t libtabfs_bat_getlba(libtabfs_bat_t* bat, int bytePos, int bitPos) {
    libtabfs_lba_28_t lba = libtabfs_bat_getstart(bat);
    lba += bytePos * 8;
    lba += bitPos;
    return lba;
}

void libtabfs_bat_flush_to_disk(libtabfs_bat_t* bat) {
    int blockSize_bytes = bat->__volume->blockSize;
    libtabfs_write_device(
        bat->__volume->__dev_data,
        bat->__lba, bat->__volume->flags.absolute_lbas, 0,
        bat + LIBTABFS_BAT_DATAOFF,
        blockSize_bytes * bat->block_count
    );
}

void libtabfs_bat_sync(libtabfs_bat_t* bat) {
    while (bat != NULL) {
        libtabfs_bat_flush_to_disk(bat);
        bat = bat->__next_bat;
    }
}

void libtabfs_bat_flush_part_to_disk(libtabfs_bat_t* bat, int block_off) {
    int blockSize_bytes = bat->__volume->blockSize;
    libtabfs_write_device(
        bat->__volume->__dev_data,
        bat->__lba + block_off, bat->__volume->flags.absolute_lbas, 0,
        bat + LIBTABFS_BAT_DATAOFF + (blockSize_bytes * block_off),
        blockSize_bytes
    );
}

libtabfs_bat_t* libtabfs_bat_getBatRegion(libtabfs_volume_t* volume, libtabfs_lba_28_t lba) {
    lba -= volume->bat_start_LBA;

    libtabfs_bat_t* prev = NULL;
    libtabfs_bat_t* bat = volume->__bat_root;
    while (1) {
        // find out how many LBA's the current section can hold
        int lba_count = libtabfs_bat_getcount(bat);
        if (lba >= lba_count) {
            prev = bat;
            bat = bat->__next_bat;
            lba -= lba_count;
            if (bat == 0) {
                // no next bat
                return NULL;
            }
            continue;
        }
        else {
            return bat;
        }
    }
}

libtabfs_error libtabfs_bat_are_blocks_free(libtabfs_bat_t* bat, int bytePos, int bitPos, unsigned int count) {

    // fist check the remaining range of the start byte
    for (;bitPos < 8; bitPos++) {
        if ((bat->data[bytePos] & (0x80 >> bitPos)) == 0) {
            count--;
            if (count == 0) { return LIBTABFS_ERR_NONE; }
            continue;
        }
        else {
            // found lba in range that is already allocated. fail since we want one continuos group of lba's
            return LIBTABFS_ERR_RANGE_NOSPACE;
        }
    }

    // count is not depleted yet? begin checking remaining bytes
    int bytecount = bat->__volume->blockSize - 6 + ((bat->block_count - 1) * bat->__volume->blockSize);
    for (; bytePos < bytecount; bytePos++) {
        for (int j = 0; j < 8; j++) {
            if ((bat->data[bytePos] & (0x80 >> j)) == 0) {
                count--;
                if (count == 0) { return LIBTABFS_ERR_NONE; }
                continue;
            }
            else {
                // found lba in range that is already allocated. fail
                return LIBTABFS_ERR_RANGE_NOSPACE;
            }
        }
    }

    // count is STILL not depleted? try to continue with the next bat
    if (bat->__next_bat != NULL) {
        return libtabfs_bat_are_blocks_free(bat->__next_bat, 0, 0, count);
    }

    // no next bat
    return LIBTABFS_ERR_DEVICE_NOSPACE;
}

void libtabfs_bat_mark_range(libtabfs_bat_t* bat, int bytePos, int bitPos, unsigned int count) {
    for (;bitPos < 8; bitPos++) {
        bat->data[bytePos] |= (0x80 >> bitPos);
        count--;
        if (count == 0) { return; }
    }

    int bytecount = bat->__volume->blockSize - 6 + ((bat->block_count - 1) * bat->__volume->blockSize);
    for (; bytePos < bytecount; bytePos++) {
        for (int j = 0; j < 8; j++) {
            bat->data[bytePos] |= (0x80 >> j);
            count--;
            if (count == 0) { return; }
        }
    }

    if (bat->__next_bat != NULL) {
        libtabfs_bat_mark_range(bat, 0, 0, count);
    }
}

void libtabfs_bat_clear_range(libtabfs_bat_t* bat, int bytePos, int bitPos, unsigned int count) {
    for (;bitPos < 8; bitPos++) {
        bat->data[bytePos] &= ~(0x80 >> bitPos);
        count--;
        if (count == 0) { return; }
    }

    int bytecount = bat->__volume->blockSize - 6 + ((bat->block_count - 1) * bat->__volume->blockSize);
    for (; bytePos < bytecount; bytePos++) {
        for (int j = 0; j < 8; j++) {
            bat->data[bytePos] &= ~(0x80 >> j);
            count--;
            if (count == 0) { return; }
        }
    }

    if (bat->__next_bat != NULL) {
        libtabfs_bat_mark_range(bat, 0, 0, count);
    }
}

// bool libtabfs_bat_allocateLoseBlocks(libtabfs_volume_t* volume, unsigned short count, long long* lba_out);
// void libtabfs_bat_freeLoseBlocks(libtabfs_volume_t* volume, unsigned short count, long long* lba_in);

libtabfs_lba_28_t libtabfs_bat_allocateChainedBlocks(libtabfs_volume_t* volume, unsigned short count) {
    libtabfs_bat_t* bat = volume->__bat_root;
    int blockSize_bytes = volume->blockSize;
    while (1) {
        int bytecount = blockSize_bytes - 6 + ((bat->block_count - 1) * blockSize_bytes);
        for (int i = 0; i < bytecount; i++) {
            if ( bat->data[i] < 0xff ) {
                for (int j = 0; j < 8; j++) {
                    if ((bat->data[i] & (0x80 >> j)) == 0) {
                        // found free block; start check if <count> blocks are free
                        int r = libtabfs_bat_are_blocks_free(bat, i, j, count);
                        if (r == LIBTABFS_ERR_NONE) {
                            libtabfs_bat_mark_range(bat, i, j, count);
                            return libtabfs_bat_getlba(bat, i, j);
                        }
                        else if (r == LIBTABFS_ERR_RANGE_NOSPACE) {
                            // count cannot be satisfied? try again elsewhere!
                            continue;
                        }
                        else if (r == LIBTABFS_ERR_DEVICE_NOSPACE) {
                            // no device space anymore... error!
                            return LIBTABFS_INVALID_LBA28;
                        }
                    }
                }
            }
        }

        if (bat->__next_bat != NULL) {
            bat = bat->__next_bat;
        }
        else {
            return LIBTABFS_INVALID_LBA28;
        }
    }
    return LIBTABFS_INVALID_LBA28;
}

void libtabfs_bat_freeChainedBlocks(libtabfs_volume_t* volume, unsigned short count, libtabfs_lba_28_t lba) {
    if (lba < volume->bat_start_LBA || lba > volume->max_LBA) {
        return;
    }

    libtabfs_bat_t* bat = libtabfs_bat_getBatRegion(volume, lba);
    if (bat == NULL) { return; }

    libtabfs_lba_28_t rlba = lba - libtabfs_bat_getstart(bat);
    int bytepos = rlba / 8;
    int bitpos = rlba % 8;

    libtabfs_bat_clear_range(bat, bytepos, bitpos, count);
}

bool libtabfs_bat_isFree(libtabfs_volume_t* volume, libtabfs_lba_28_t lba) {
    if (lba < volume->bat_start_LBA || lba > volume->max_LBA) {
        #ifdef LIBTABFS_DEBUG_PRINTF
            printf("[libtabfs_bat_isFree] lba: 0x%x is below bat_start_LBA or byond max_LBA\n", lba);
        #endif
        return false;
    }
    libtabfs_bat_t* bat = libtabfs_bat_getBatRegion(volume, lba);
    if (bat == NULL) {
        return false;   // error while getting bat region; means we are out of memory on the drive!
    }

    libtabfs_lba_28_t rlba = lba - libtabfs_bat_getstart(bat);
    int bytepos = rlba / 8;
    int bitpos = rlba % 8;
    #ifdef LIBTABFS_DEBUG_PRINTF
        printf(
            "[libtabfs_bat_isFree] lba: 0x%x | bat_getstart: 0x%x | r-lba: 0x%x | bat->__lba: 0x%x | bytepos: %d | bitpos: %d\n",
            lba, libtabfs_bat_getstart(bat), rlba, bat->__lba, bytepos, bitpos
        );
    #endif
    return (bat->data[bytepos] & (0x80 >> bitpos)) == 0;
}
