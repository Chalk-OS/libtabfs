#include "bridge.h"

#include "common.h"
#include "volume.h"
#include "bat.h"
#include "entrytable.h"

const char* libtabfs_magic = "TABFS-28\0\0\0\0\0\0\0";

#define ASSURE_NON_NULLPTR(what)    if (what == 0) { return LIBTABFS_ERR_GENERIC; }

libtabfs_error libtabfs_new_volume(void* dev_data, long long lba_address, bool absolute_lba, libtabfs_volume_t** volume_out) {
    if (volume_out == NULL) { return LIBTABFS_ERR_ARGS; }

    libtabfs_header_t header;

    // read the header form the device
    libtabfs_read_device(
        dev_data,                   // deviceparams
        lba_address, absolute_lba,  // lba, is_absolute_lba
        0x1C0,                      // offset into the block
        ((void*)&header),           // buffer to write to
        sizeof(libtabfs_header_t)   // count to read / size if buffer (in bytes)
    );

    // check bootsignature
    if (!LIBTABFS_IS_BOOTABLE(header)) {
        return LIBTABFS_ERR_NO_BOOTSIG;
    }

    // check magic
    for (int i = 0; i < 16; i++) {
        if (header.magic[i] != libtabfs_magic[i]) {
            return LIBTABFS_ERR_WRONG_MAGIC;
        }
    }

    libtabfs_volume_t* volume = (libtabfs_volume_t*) libtabfs_alloc( sizeof(struct libtabfs_volume) );
    ASSURE_NON_NULLPTR(volume);

    // read volume from the device
    libtabfs_read_device(
        dev_data, LIBTABFS_LBA48_TO_LBA28(header.info_LBA), header.flags.absolute_lbas,
        0x0, ((void*)volume), 256
    );

    // store the device arguments into the volume
    volume->__dev_data = dev_data;
    volume->__lba = LIBTABFS_LBA48_TO_LBA28(header.info_LBA);
    volume->__table_cache = libtabfs_linkedlist_create( libtabfs_entrytable_destroy );

    *volume_out = volume;

    // read the complete BAT into memory
    volume->__bat_root = libtabfs_load_bat(volume, volume->bat_LBA);
    libtabfs_bat_t* bat = volume->__bat_root;
    while (bat->next_bat != 0) {
        bat->__next_bat = libtabfs_load_bat(volume, bat->next_bat);
        bat = bat->__next_bat;
    }

    // read the root entrytable
    volume->__root_table = libtabfs_read_entrytable(volume, volume->root_LBA, volume->root_size);

    return LIBTABFS_ERR_NONE;
};

void libtabfs_volume_sync(libtabfs_volume_t* volume) {
    // sync volume informations to drive
    libtabfs_write_device(
        volume->__dev_data,
        volume->__lba, volume->flags.absolute_lbas, 0,
        (void*)volume, 256
    );

    // sync all bats
    libtabfs_bat_sync(volume->__bat_root);
}

libtabfs_error libtabfs_volume_set_label(libtabfs_volume_t* volume, char* label, bool sync) {
    int labellen = libtabfs_strlen(label);

    if (labellen > 175) {
        return LIBTABFS_ERR_LABEL_TOLONG;
    }

    libtabfs_memcpy(volume->volume_label, label, labellen);
    volume->volume_label[labellen] = '\0';

    if (sync) { libtabfs_volume_sync(volume); }
    return LIBTABFS_ERR_NONE;
}

const char* libtabfs_volume_get_label(libtabfs_volume_t* volume) {
    return volume->volume_label;
}

void libtabfs_destroy_volume(libtabfs_volume_t* volume) {
    libtabfs_volume_sync(volume);

    // free all bat regions
    libtabfs_bat_t* cur = volume->__bat_root;
    while (cur != NULL) {
        libtabfs_bat_t* tmp = cur;
        cur = cur->__next_bat;
        libtabfs_bat_destroy(tmp);
    }

    // free all tables; the root table is also inside our cache!
    libtabfs_linkedlist_destroy(volume->__table_cache);

    libtabfs_free(volume, sizeof(struct libtabfs_volume));
}