#include "bridge.h"

#include "common.h"
#include "volume.h"
#include "bat.h"
#include "entrytable.h"
#ifdef LIBTABFS_DEBUG_PRINTF
    #include <stdio.h>
#endif

//--------------------------------------------------------------------------------
// Entrytable creation, sync & destroying
//--------------------------------------------------------------------------------

#define LIBTABFS_ENTRYTABLE_DATAOFFSET  (LIBTABFS_PTR_SIZE) + sizeof(unsigned int) + sizeof(libtabfs_lba_28_t)

libtabfs_entrytable_t* libtabfs_read_entrytable(libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size) {

    libtabfs_entrytable_t* entrytable = (libtabfs_entrytable_t*) libtabfs_alloc(LIBTABFS_ENTRYTABLE_DATAOFFSET + size);

    libtabfs_read_device(
        volume->__dev_data,
        lba, volume->flags.absolute_lbas, 0,
        (void*) entrytable + LIBTABFS_ENTRYTABLE_DATAOFFSET, size
    );

    entrytable->__volume = volume;
    entrytable->__lba = lba;
    entrytable->__byteSize = size;

    // add the table to our cache!
    libtabfs_linkedlist_add(volume->__table_cache, entrytable);

    return entrytable;
}

libtabfs_entrytable_t* libtabfs_get_entrytable(libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size) {
    libtabfs_entrytable_t* next_section = libtabfs_find_cached_entrytable(volume, lba);
    if (next_section == NULL) {
        return libtabfs_read_entrytable(volume, lba, size);
    }
    else {
        return next_section;
    }
}

libtabfs_entrytable_t* libtabfs_create_entrytable(
    libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size, libtabfs_entrytable_t* parent_table
) {

    libtabfs_entrytable_t* entrytable = (libtabfs_entrytable_t*) libtabfs_alloc(LIBTABFS_ENTRYTABLE_DATAOFFSET + size);
    entrytable->__volume = volume;
    entrytable->__lba = lba;
    entrytable->__byteSize = size;

    // configure tabinfo entry
    libtabfs_entrytable_tableinfo_t* tabinfo = &( entrytable->entries[0] );
    tabinfo->flags.type = LIBTABFS_ENTRYTYPE_TABLEINFO;

    // set the parent information
    tabinfo->parent_lba = parent_table->__lba;
    tabinfo->parent_size = parent_table->__byteSize;

    // add the table to our cache!
    libtabfs_linkedlist_add(volume->__table_cache, entrytable);

    // TODO: clear and flush to disk to ensure we have an empty entrytable created!
    return entrytable;
}

void libtabfs_entrytable_cachefree_callback(libtabfs_entrytable_t* entrytable) {
    libtabfs_entrytable_sync(entrytable);
    libtabfs_free(entrytable, LIBTABFS_ENTRYTABLE_DATAOFFSET + entrytable->__byteSize);
}

void libtabfs_entrytable_destroy(libtabfs_entrytable_t* entrytable) {
    // sync the entrytable section one last time to disk
    libtabfs_entrytable_sync(entrytable);

    // removes the entry from the tablecache
    libtabfs_linkedlist_remove_data(entrytable->__volume->__table_cache, entrytable);

    libtabfs_free(entrytable, LIBTABFS_ENTRYTABLE_DATAOFFSET + entrytable->__byteSize);
}

void libtabfs_entrytable_remove(libtabfs_entrytable_t* entrytable) {
    libtabfs_bat_freeChainedBlocks(
        entrytable->__volume,
        entrytable->__byteSize / entrytable->__volume->blockSize,
        entrytable->__lba
    );
    libtabfs_linkedlist_remove_data(entrytable->__volume->__table_cache, entrytable);
    libtabfs_free(entrytable, LIBTABFS_ENTRYTABLE_DATAOFFSET + entrytable->__byteSize);
}

void libtabfs_entrytable_sync(libtabfs_entrytable_t* entrytable) {
    libtabfs_write_device(
        entrytable->__volume->__dev_data,
        entrytable->__lba, entrytable->__volume->flags.absolute_lbas, 0,
        (void*) entrytable + LIBTABFS_ENTRYTABLE_DATAOFFSET, entrytable->__byteSize
    );
}

//--------------------------------------------------------------------------------
// Helper
//--------------------------------------------------------------------------------

void libtabfs_fileflags_to_entry(libtabfs_fileflags_t fileflags, libtabfs_entrytable_entry_t* entry) {
    entry->flags.set_uid = fileflags.set_uid;
    entry->flags.set_gid = fileflags.set_gid;
    entry->flags.sticky = fileflags.sticky;

    // first clean rawflags from old acl
    entry->rawflags &= ~(0b1111111100000001);

    // set acl
    entry->rawflags |= (fileflags.raw_user & 0b00000100) >> 2;
    entry->rawflags |= (fileflags.raw_user & 0b00000011) << (6 + 8);

    entry->rawflags |= (fileflags.raw_group & 0b00000111) << (3 + 8);
    entry->rawflags |= (fileflags.raw_other & 0b00000111) << 8;
}

bool libtabfs_check_perm(libtabfs_entrytable_entry_t* entry, unsigned int userid, unsigned int groupid, unsigned char perm) {
    if (userid == entry->user_id && (LIBTABFS_TAB_ENTRY_ACLUSR(entry) & perm)) {
        return true;
    }
    else if (userid == entry->group_id && (LIBTABFS_TAB_ENTRY_ACLGRP(entry) & perm)) {
        return true;
    }
    else if ((LIBTABFS_TAB_ENTRY_ACLOTH(entry) & perm)) {
        return true;
    }
    return false;
}

void libtabfs_entry_chown(libtabfs_entrytable_entry_t* entry, unsigned int userid, unsigned int groupid) {
    entry->user_id = userid;
    entry->group_id = groupid;
}

void libtabfs_entry_touch(libtabfs_entrytable_entry_t* entry, libtabfs_time_t m_time, libtabfs_time_t a_time) {
    entry->modify_ts = m_time;
    entry->access_ts = a_time;
}

libtabfs_entrytable_t* libtabfs_find_cached_entrytable(libtabfs_volume_t* volume, libtabfs_lba_28_t entrytable_lba) {
    libtabfs_linkedlist_entry_t* entry = volume->__table_cache->head;
    while (entry != NULL) {
        libtabfs_entrytable_t* tab = (libtabfs_entrytable_t*) entry->data;
        if (tab->__lba == entrytable_lba) {
            return tab;
        }
        else {
            entry = entry->next;
        }
    }
    return NULL;
}

libtabfs_entrytable_t* libtabfs_entrytable_get_first_section(libtabfs_entrytable_t* section) {
    while (section != NULL) {
        libtabfs_entrytable_tableinfo_t* tabinfo = &( section->entries[0] );
        if (tabinfo->prev_size != 0 && !LIBTABFS_IS_INVALID_LBA28(tabinfo->prev_lba)) {
            section = libtabfs_get_entrytable(section->__volume, tabinfo->prev_lba, tabinfo->prev_size);
        }
        else {
            return section;
        }
    }
    return section;
}

libtabfs_entrytable_t* libtabfs_entrytable_get_parent(libtabfs_entrytable_t* section) {
    libtabfs_entrytable_tableinfo_t* tabinfo = &( section->entries[0] );
    if (tabinfo->parent_size != 0 && !LIBTABFS_IS_INVALID_LBA28(tabinfo->parent_lba)) {
        return libtabfs_get_entrytable(section->__volume, tabinfo->parent_lba, tabinfo->parent_size);
    }
    else {
        return NULL;
    }
}

libtabfs_entrytable_t* libtabfs_entrytable_nextsection(libtabfs_entrytable_t* section) {
    libtabfs_entrytable_tableinfo_t* tabinfo = &( section->entries[0] );
    if (tabinfo->next_size != 0 && !LIBTABFS_IS_INVALID_LBA28(tabinfo->next_lba)) {
        return libtabfs_get_entrytable(section->__volume, tabinfo->next_lba, tabinfo->next_size);
    }
    else {
        return NULL;
    }
}

libtabfs_error libtabfs_entrytab_findfree(
    libtabfs_entrytable_t* entrytable,
    libtabfs_entrytable_entry_t** entry_out,
    libtabfs_entrytable_t** entrytable_out,
    int* offset_out
) {
    if (entry_out == NULL) { return LIBTABFS_ERR_ARGS; }
    *entry_out = NULL;

    int entryCount = entrytable->__byteSize / 64;
    for (int i = 1; i < entryCount; i++) {
        libtabfs_entrytable_entry_t* entry = &(entrytable->entries[i]);
        if (entry->flags.type == LIBTABFS_ENTRYTYPE_UNKNOWN) {
            *entry_out = entry;
            if (entrytable_out != NULL) { *entrytable_out = entrytable; }
            if (offset_out != NULL) { *offset_out = i; }
            return LIBTABFS_ERR_NONE;
        }
    }

    libtabfs_entrytable_tableinfo_t* tabinfo = &( entrytable->entries[0] );

    if (tabinfo->next_size != 0 && !LIBTABFS_IS_INVALID_LBA28(tabinfo->next_lba)) {
        // try to find in next section
        libtabfs_entrytable_t* next_section = libtabfs_get_entrytable(entrytable->__volume, tabinfo->next_lba, tabinfo->next_size);
        return libtabfs_entrytab_findfree(next_section, entry_out, entrytable_out, offset_out);
    }
    else {
        // no next section configured; create a new section!

        libtabfs_lba_28_t next_section_lba = libtabfs_bat_allocateChainedBlocks(entrytable->__volume, 2);
        if (LIBTABFS_IS_INVALID_LBA28(next_section_lba)) {
            return LIBTABFS_ERR_DEVICE_NOSPACE;
        }

        int next_section_size = 2 * entrytable->__volume->blockSize;

        libtabfs_entrytable_t* next_section = libtabfs_create_entrytable(
            entrytable->__volume, next_section_lba, next_section_size,
            libtabfs_entrytable_get_parent(entrytable)
        );

        tabinfo->next_lba = next_section_lba;
        tabinfo->next_size = next_section_size;

        libtabfs_entrytable_tableinfo_t* nextsection_tabinfo = &( next_section->entries[0] );
        nextsection_tabinfo->prev_lba = entrytable->__lba;
        nextsection_tabinfo->prev_size = entrytable->__byteSize;

        *entry_out = &(next_section->entries[1]);
        if (entrytable_out != NULL) { *entrytable_out = next_section; }
        if (offset_out != NULL) { *offset_out = 1; }

        return LIBTABFS_ERR_NONE;
    }
    return LIBTABFS_ERR_DIR_FULL;
}

libtabfs_error libtabfs_entrytable_count_entries(libtabfs_entrytable_t* entrytable, bool skip_longnames) {
    int count = 0;

    int entryCount = entrytable->__byteSize / 64;
    for (int i = 1; i < entryCount; i++) {
        libtabfs_entrytable_entry_t* entry = &(entrytable->entries[i]);
        if (skip_longnames && entry->flags.type == LIBTABFS_ENTRYTYPE_LONGNAME) {
            continue;
        }
        if (entry->flags.type != LIBTABFS_ENTRYTYPE_UNKNOWN) {
            count += 1;
        }
    }

    libtabfs_entrytable_tableinfo_t* tabinfo = &( entrytable->entries[0] );
    if (tabinfo->next_size != 0 && !LIBTABFS_IS_INVALID_LBA28(tabinfo->next_lba)) {
        libtabfs_entrytable_t* next_section = libtabfs_get_entrytable(entrytable->__volume, tabinfo->next_lba, tabinfo->next_size);
        count += libtabfs_entrytable_count_entries(next_section, skip_longnames);
    }
    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_entry_get_name(libtabfs_volume_t* volume, libtabfs_entrytable_entry_t* entry, char** name_out) {
    if (name_out == NULL) { return LIBTABFS_ERR_ARGS; }

    if (entry->longname_data.longname_identifier == 0x00) {
        *name_out = entry->name;
        return LIBTABFS_ERR_NONE;
    }

    libtabfs_entrytable_t* tab = libtabfs_get_entrytable(volume, entry->longname_data.longname_lba, entry->longname_data.longname_lba_size);
    libtabfs_entrytable_longname_t* lne = &( tab->entries[entry->longname_data.longname_offset] );
    if (lne->flags.type != LIBTABFS_ENTRYTYPE_LONGNAME) {
        return LIBTABFS_ERR_GENERIC;
    }

    *name_out = lne->name;
    return LIBTABFS_ERR_NONE;
}

//--------------------------------------------------------------------------------
// Find entrys inside an entrytable
//--------------------------------------------------------------------------------

libtabfs_error libtabfs_entrytab_findentry(
    libtabfs_entrytable_t* entrytable, char* name,
    libtabfs_entrytable_entry_t** entry_out,
    libtabfs_entrytable_t** entrytable_out,
    int* offset_out
) {
    int namelen = libtabfs_strlen(name);
    if (namelen > 62) {
        return LIBTABFS_ERR_NAME_TOLONG;
    }
    if (entry_out == NULL) {
        return LIBTABFS_ERR_ARGS;
    }

    int entryCount = entrytable->__byteSize / 64;
    if (namelen >= 22) {
        // search after an entry with lne!
        for (int i = 1; i < entryCount; i++) {
            libtabfs_entrytable_entry_t* entry = &(entrytable->entries[i]);
            if (
                entry->flags.type != LIBTABFS_ENTRYTYPE_UNKNOWN &&
                entry->longname_data.longname_identifier != 0x00
            ) {
                libtabfs_entrytable_t* lne_entrytable = libtabfs_get_entrytable(
                    entrytable->__volume, entry->longname_data.longname_lba, entry->longname_data.longname_lba_size
                );
                libtabfs_entrytable_longname_t* lne = &( lne_entrytable->entries[ entry->longname_data.longname_offset ] );
                if (lne->flags.type != LIBTABFS_ENTRYTYPE_LONGNAME) {
                    return LIBTABFS_ERR_GENERIC;
                }
                if (libtabfs_strcmp(lne->name, name) == 0) {
                    *entry_out = entry;
                    if (entrytable_out != NULL) { *entrytable_out = entrytable; }
                    if (offset_out != NULL) { *offset_out = i; }
                    return LIBTABFS_ERR_NONE;
                }
            }
        }
    }
    else {
        // search in-entry name
        for (int i = 1; i < entryCount; i++) {
            libtabfs_entrytable_entry_t* entry = &(entrytable->entries[i]);
            if (
                entry->flags.type != LIBTABFS_ENTRYTYPE_UNKNOWN &&
                entry->longname_data.longname_identifier == 0x00 &&
                libtabfs_strcmp(entry->name, name) == 0
            ) {
                *entry_out = entry;
                if (entrytable_out != NULL) { *entrytable_out = entrytable; }
                if (offset_out != NULL) { *offset_out = i; }
                return LIBTABFS_ERR_NONE;
            }
        }
    }

    *entry_out = NULL;
    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_entrytab_getsymlinktarget(
    libtabfs_entrytable_t* entrytable, libtabfs_entrytable_entry_t* symlink_entry,
    unsigned int userid, unsigned int groupid,
    libtabfs_entrytable_entry_t** entry_out,
    libtabfs_entrytable_t** entrytable_out,
    int* offset_out
) {
    libtabfs_entrytable_t* lne_entrytable = entrytable;
    int offset = symlink_entry->data.link.offset;
    while (true) {
        int entryCount = lne_entrytable->__byteSize / 64;
        if (offset >= entryCount) {
            offset -= entryCount;
            lne_entrytable = libtabfs_entrytable_nextsection(lne_entrytable);
            continue;
        }
        else {
            break;
        }
    }
    libtabfs_entrytable_longname_t* lne_path = &( lne_entrytable->entries[ offset ] );

    libtabfs_entrytable_t* tab = NULL;
    // if the path starts with an '/', its an absolute path!
    char* path = lne_path->name;
    if (*path == '/') {
        path += 1;
        tab = entrytable->__volume->__root_table;
    }
    else {
        tab = libtabfs_entrytable_get_first_section(lne_entrytable);
    }

    // get the target of the link
    return libtabfs_entrytab_traversetree(tab, path, true, userid, groupid, entry_out, entrytable_out, offset_out);
}

libtabfs_error libtabfs_entrytab_traversetree(
    libtabfs_entrytable_t* entrytable, char* relative_path, bool follow_symlink,
    unsigned int userid, unsigned int groupid,
    libtabfs_entrytable_entry_t** entry_out,
    libtabfs_entrytable_t** entrytable_out,
    int* offset_out
) {
    while (*relative_path != '\0') {
        if (relative_path[0] == '.') {
            if (relative_path[1] == '.' && relative_path[2] == '/') {
                // was '../' which means the parent directory;
                relative_path += 3;

                libtabfs_entrytable_tableinfo_t* tabinf = &( entrytable->entries[0] );
                if (tabinf->parent_size == 0 || LIBTABFS_IS_INVALID_LBA28(tabinf->parent_lba)) {
                    // we are already at the root... ignore any further try to go beyond
                    continue;
                }
                else {
                    entrytable = libtabfs_get_entrytable(
                        entrytable->__volume, tabinf->parent_lba, tabinf->parent_size
                    );
                }
            }
            else if (relative_path[1] == '/') {
                // was './', which means the current directory; skip the two chars and continue on
                relative_path += 2;
                continue;
            }
        }

        char* delim = libtabfs_strchr(relative_path, '/');      // TODO: make this delimiter changeable
        if (delim == NULL) {
            // no more traversel; just find the entry!
            libtabfs_error err = libtabfs_entrytab_findentry(entrytable, relative_path, entry_out, entrytable_out, offset_out);
            if (err != LIBTABFS_ERR_NONE) {
                return err;
            }

            if (*entry_out == NULL) {
                return LIBTABFS_ERR_NOT_FOUND;
            }

            if (follow_symlink && (*entry_out)->flags.type == LIBTABFS_ENTRYTYPE_SYMLINK) {
                err = libtabfs_entrytab_getsymlinktarget(entrytable, *entry_out, userid, groupid, entry_out, entrytable_out, offset_out);
            }

            return err;
        }
        else {
            *delim = '\0';  // temporarily set an end so libtabfs_strcmp can work!
            // TODO: rework this since temprarily modifing the path is not good; const char* cannot be used as argument then!
            libtabfs_error err = libtabfs_entrytab_findentry(entrytable, relative_path, entry_out, entrytable_out, offset_out);
            *delim = '/';   // restore the delimiter

            if (err != LIBTABFS_ERR_NONE) {
                return err;
            }

            if (*entry_out == NULL) {
                return LIBTABFS_ERR_NOT_FOUND;
            }

            if (follow_symlink && (*entry_out)->flags.type == LIBTABFS_ENTRYTYPE_SYMLINK) {
                err = libtabfs_entrytab_getsymlinktarget(entrytable, *entry_out, userid, groupid, entry_out, entrytable_out, offset_out);
            }

            if ((*entry_out)->flags.type != LIBTABFS_ENTRYTYPE_DIR) {
                *entry_out = NULL;
                return LIBTABFS_ERR_IS_NO_DIR;
            }

            // check execute bit to determine if we are allowed to enter!
            if (!libtabfs_check_perm(*entry_out, userid, groupid, 0b001)) {
                *entry_out = NULL;
                return LIBTABFS_ERR_NO_PERM;
            }

            // move to new entrytable
            entrytable = libtabfs_get_entrytable(
                entrytable->__volume, (*entry_out)->data.dir.lba, (*entry_out)->data.dir.size
            );

            // move the relative_path to the new one
            relative_path = delim + 1;

            if (*relative_path == '\0') {
                return LIBTABFS_ERR_NONE;
            }

            continue;
        }
    }
    return LIBTABFS_ERR_GENERIC;
}

//--------------------------------------------------------------------------------
// Entry creation
//--------------------------------------------------------------------------------

#define NAME_CHECK \
    int namelen = libtabfs_strlen(name); if (namelen > 62) { return LIBTABFS_ERR_NAME_TOLONG; }

libtabfs_error libtabfs_create_entry(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_entrytable_entry_t** entry_out
) {
    NAME_CHECK
    if (entry_out == NULL) { return LIBTABFS_ERR_ARGS; }

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_entrytab_findfree(entrytable, &entry, NULL, NULL);
    if (err != LIBTABFS_ERR_NONE) {
        return err;
    }

    if (namelen >= 22) {
        // temporaily set to something other than free so when we search for a free entry for the name, it dosnt get in the way
        entry->flags.type = LIBTABFS_ENTRYTYPE_LONGNAME;

        libtabfs_entrytable_entry_t* longname_entry = NULL;
        libtabfs_entrytable_t* entrytable_of_lne = NULL;
        int offset_of_lne = -1;

        libtabfs_error err = libtabfs_entrytab_findfree(entrytable, &longname_entry, &entrytable_of_lne, &offset_of_lne);
        if (err != LIBTABFS_ERR_NONE) {
            return err;
        }

        longname_entry->flags.type = LIBTABFS_ENTRYTYPE_LONGNAME;
        libtabfs_memcpy(
            ((libtabfs_entrytable_longname_t*) longname_entry)->name,
            name, namelen
        );
        ((libtabfs_entrytable_longname_t*) longname_entry)->name[namelen] = '\0';

        // reset type to free;
        entry->flags.type = LIBTABFS_ENTRYTYPE_UNKNOWN;

        entry->longname_data.longname_identifier = 0xFF;
        entry->longname_data.longname_lba = entrytable_of_lne->__lba;
        entry->longname_data.longname_offset = offset_of_lne;
    }
    else {
        libtabfs_memcpy(entry->name, name, namelen);
        entry->name[namelen] = '\0';
    }

    *entry_out = entry;
    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_create_dir(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    libtabfs_entrytable_t** entrytable_newdir_out
) {
    NAME_CHECK
    if (entrytable_newdir_out == NULL) {
        return LIBTABFS_ERR_ARGS;
    }

    // allocate an new entrytable
    libtabfs_lba_28_t entrytable_lba = libtabfs_bat_allocateChainedBlocks(entrytable->__volume, 2);
    if (LIBTABFS_IS_INVALID_LBA28(entrytable_lba)) {
        return LIBTABFS_ERR_DEVICE_NOSPACE;
    }
    #ifdef LIBTABFS_DEBUG_PRINTF
        printf("[libtabfs_create_dir] entrytable_lba: 0x%X\n", entrytable_lba);
    #endif

    int size = 2 * entrytable->__volume->blockSize;

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_create_entry( entrytable, name, &entry);
    if (err != LIBTABFS_ERR_NONE) {
        // error occured, free the allocated block for the entrytable!
        libtabfs_bat_freeChainedBlocks(entrytable->__volume, 2, entrytable_lba);
        return err;
    }

    libtabfs_entrytable_entry_data_t data = {
        .dir = { .lba = entrytable_lba, .size = size }
    };
    entry->data = data;

    entry->rawflags = 0;
    entry->flags.type = LIBTABFS_ENTRYTYPE_DIR;
    libtabfs_fileflags_to_entry(fileflags, entry);

    libtabfs_entry_chown(entry, userid, groupid);
    entry->create_ts = create_ts;
    libtabfs_entry_touch(entry, create_ts, create_ts);

    libtabfs_entrytable_t* new_entrytable = libtabfs_create_entrytable(
        entrytable->__volume, entrytable_lba, size,
        libtabfs_entrytable_get_first_section(entrytable)
    );
    *entrytable_newdir_out = new_entrytable;

    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_create_chardevice(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    unsigned int dev_id, unsigned int dev_flags
) {
    NAME_CHECK

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_create_entry( entrytable, name, &entry);
    if (err != LIBTABFS_ERR_NONE) {
        return err;
    }

    libtabfs_entrytable_entry_data_t data = {
        .dev = { .id = dev_id, .flags = dev_flags }
    };
    entry->data = data;

    entry->rawflags = 0;
    entry->flags.type = LIBTABFS_ENTRYTYPE_DEV_CHR;
    libtabfs_fileflags_to_entry(fileflags, entry);

    libtabfs_entry_chown(entry, userid, groupid);
    entry->create_ts = create_ts;
    libtabfs_entry_touch(entry, create_ts, create_ts);

    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_create_blockdevice(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    unsigned int dev_id, unsigned int dev_flags
) {
    NAME_CHECK

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_create_entry( entrytable, name, &entry);
    if (err != LIBTABFS_ERR_NONE) {
        return err;
    }

    libtabfs_entrytable_entry_data_t data = {
        .dev = { .id = dev_id, .flags = dev_flags }
    };
    entry->data = data;

    entry->rawflags = 0;
    entry->flags.type = LIBTABFS_ENTRYTYPE_DEV_BLK;
    libtabfs_fileflags_to_entry(fileflags, entry);

    libtabfs_entry_chown(entry, userid, groupid);
    entry->create_ts = create_ts;
    libtabfs_entry_touch(entry, create_ts, create_ts);

    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_create_fifo(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    unsigned int bufferSize
) {
    NAME_CHECK

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_create_entry( entrytable, name, &entry);
    if (err != LIBTABFS_ERR_NONE) {
        return err;
    }

    libtabfs_entrytable_entry_data_t data = {
        .fifo = { .bufferSize = bufferSize }
    };
    entry->data = data;

    entry->rawflags = 0;
    entry->flags.type = LIBTABFS_ENTRYTYPE_FIFO;
    libtabfs_fileflags_to_entry(fileflags, entry);

    libtabfs_entry_chown(entry, userid, groupid);
    entry->create_ts = create_ts;
    libtabfs_entry_touch(entry, create_ts, create_ts);

    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_create_symlink(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    char* path
) {
    NAME_CHECK
    
    int pathlen = libtabfs_strlen(path);
    if (pathlen > 62) { return LIBTABFS_ERR_ARGS; }

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_create_entry( entrytable, name, &entry );
    if (err != LIBTABFS_ERR_NONE) {
        return err;
    }

    entry->rawflags = 0;
    entry->flags.type = LIBTABFS_ENTRYTYPE_SYMLINK;

    libtabfs_entrytable_entry_t* lne_path;
    libtabfs_entrytable_t* lne_path_sec;
    int lne_path_off = -1;

    err = libtabfs_entrytab_findfree(entrytable, &lne_path, &lne_path_sec, &lne_path_off);
    if (err != LIBTABFS_ERR_NONE) {
        entry->rawflags = 0;
        return err;
    }

    libtabfs_entrytable_t* cur = entrytable;
    while (cur != lne_path_sec) {
        int entryCount = cur->__byteSize / 64;
        lne_path_off += entryCount;
        cur = libtabfs_entrytable_nextsection(cur);
        if (cur == NULL) { break; }
    }

    libtabfs_fileflags_to_entry(fileflags, entry);
    libtabfs_entry_chown(entry, userid, groupid);
    entry->create_ts = create_ts;
    libtabfs_entry_touch(entry, create_ts, create_ts);

    libtabfs_entrytable_entry_data_t data = {
        .link = { .offset = lne_path_off }
    };
    entry->data = data;

    lne_path->flags.type = LIBTABFS_ENTRYTYPE_LONGNAME;
    libtabfs_entrytable_longname_t* lne = (libtabfs_entrytable_longname_t*) lne_path;
    libtabfs_memcpy(lne->name, path, pathlen);
    lne->name[pathlen] = '\0';

    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_create_socket(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    unsigned int ipv4_or_serialnum
) {
    NAME_CHECK

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_create_entry( entrytable, name, &entry);
    if (err != LIBTABFS_ERR_NONE) {
        return err;
    }

    libtabfs_entrytable_entry_data_t data = {
        .sock = { .ipv4_or_serialnum = ipv4_or_serialnum }
    };
    entry->data = data;

    entry->rawflags = 0;
    entry->flags.type = LIBTABFS_ENTRYTYPE_SOCKET;
    libtabfs_fileflags_to_entry(fileflags, entry);

    libtabfs_entry_chown(entry, userid, groupid);
    entry->create_ts = create_ts;
    libtabfs_entry_touch(entry, create_ts, create_ts);

    return LIBTABFS_ERR_NONE;
}

libtabfs_error libtabfs_create_continuousfile(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    bool iskernel, size_t size, libtabfs_entrytable_entry_t** entry_out
) {
    NAME_CHECK
    if (entry_out == NULL) { return LIBTABFS_ERR_ARGS; }

    // first allocate the blocks...
    int blocks = size / entrytable->__volume->blockSize;
    if ((size % entrytable->__volume->blockSize) != 0) {
        blocks += 1;
    }

    libtabfs_lba_28_t fileContent_lba = libtabfs_bat_allocateChainedBlocks(entrytable->__volume, blocks);
    if (LIBTABFS_IS_INVALID_LBA28(fileContent_lba)) {
        return LIBTABFS_ERR_DEVICE_NOSPACE;
    }
    #ifdef LIBTABFS_DEBUG_PRINTF
        printf("[libtabfs_create_continuousfile] fileContent_lba: 0x%X | blocks: %d | size: %lu\n", fileContent_lba, blocks, size);
    #endif

    libtabfs_entrytable_entry_t* entry = NULL;
    libtabfs_error err = libtabfs_create_entry(entrytable, name, &entry);
    if (err != LIBTABFS_ERR_NONE) {
        libtabfs_bat_freeChainedBlocks(entrytable->__volume, blocks, fileContent_lba);
        return err;
    }

    entry->rawflags = 0;
    entry->flags.type = iskernel ? LIBTABFS_ENTRYTYPE_KERNEL : LIBTABFS_ENTRYTYPE_FILE_CONTINUOUS;
    libtabfs_fileflags_to_entry(fileflags, entry);

    libtabfs_entry_chown(entry, userid, groupid);
    entry->create_ts = create_ts;
    libtabfs_entry_touch(entry, create_ts, create_ts);

    *entry_out = entry;

    entry->data.lba_and_size.lba = fileContent_lba;
    entry->data.lba_and_size.size = size;

    // zero the file content
    for (int i = 0; i < blocks; i++) {
        libtabfs_set_range_device(
            entrytable->__volume->__dev_data,
            fileContent_lba + i, entrytable->__volume->flags.absolute_lbas,
            0, 0, entrytable->__volume->blockSize
        );
    }

    return LIBTABFS_ERR_NONE;
}

//--------------------------------------------------------------------------------
// Entry read / write (file only)
//--------------------------------------------------------------------------------

libtabfs_error libtabfs_read_file(
    libtabfs_volume_t* volume,
    libtabfs_entrytable_entry_t* entry, unsigned long int offset, unsigned long int len, unsigned char* buffer,
    unsigned long int* bytesRead
) {
    if (entry == NULL || buffer == NULL || bytesRead == NULL) {
        return LIBTABFS_ERR_ARGS;
    }

    switch (entry->flags.type) {
        case LIBTABFS_ENTRYTYPE_FILE_CONTINUOUS:
        case LIBTABFS_ENTRYTYPE_KERNEL: {
            // read from continuous file

            libtabfs_lba_28_t fileContent_lba = entry->data.lba_and_size.lba;
            unsigned int fileContent_size = entry->data.lba_and_size.size;
            if (offset >= fileContent_size) {
                *bytesRead = 0;
                return LIBTABFS_ERR_OFFSET_AFTER_FILE_END;
            }

            int real_len = len;
            if ((offset + real_len) > fileContent_size) {
                real_len = fileContent_size - offset;
            }

            // NOTE: this use is a bit hacky, since it relays on the fact that the blocks are chained and the read method dont do
            //       any sort of checks or similar
            libtabfs_read_device(
                volume->__dev_data,
                fileContent_lba, volume->flags.absolute_lbas,
                offset, buffer, real_len
            );

            *bytesRead = real_len;
            return LIBTABFS_ERR_NONE;
        }

        case LIBTABFS_ENTRYTYPE_FILE_FAT:
            return LIBTABFS_ERR_GENERIC;

        case LIBTABFS_ENTRYTYPE_FILE_SEG:
            return LIBTABFS_ERR_GENERIC;
    
        default:
            return LIBTABFS_ERR_ARGS;
    }

}

libtabfs_error libtabfs_write_file(
    libtabfs_volume_t* volume,
    libtabfs_entrytable_entry_t* entry, unsigned long int offset, unsigned long int len, unsigned char* buffer,
    unsigned long int* bytesWritten
) {
    if (entry == NULL || buffer == NULL || bytesWritten == NULL) {
        return LIBTABFS_ERR_ARGS;
    }

    switch (entry->flags.type) {
        case LIBTABFS_ENTRYTYPE_FILE_CONTINUOUS:
        case LIBTABFS_ENTRYTYPE_KERNEL: {
            // write to continuous file

            libtabfs_lba_28_t fileContent_lba = entry->data.lba_and_size.lba;
            unsigned int fileContent_size = entry->data.lba_and_size.size;
            if (offset >= fileContent_size) {
                *bytesWritten = 0;
                return LIBTABFS_ERR_OFFSET_AFTER_FILE_END;
            }

            int real_len = len;
            if ((offset + real_len) > fileContent_size) {
                real_len = fileContent_size - offset;
            }

            // NOTE: this use is a bit hacky, since it relays on the fact that the blocks are chained and the write method dont do
            //       any sort of checks or similar
            libtabfs_write_device(
                volume->__dev_data,
                fileContent_lba, volume->flags.absolute_lbas,
                offset, buffer, real_len
            );

            *bytesWritten = real_len;
            return LIBTABFS_ERR_NONE;
        }

        case LIBTABFS_ENTRYTYPE_FILE_FAT:
            return LIBTABFS_ERR_GENERIC;

        case LIBTABFS_ENTRYTYPE_FILE_SEG:
            return LIBTABFS_ERR_GENERIC;

        default:
            return LIBTABFS_ERR_ARGS;
    }
}