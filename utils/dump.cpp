#include "dump.hpp"
#include <stdio.h>
#include <cctype>
#include <math.h>

void dump_mem(uint8_t* mem, int size) {
    printf("\e[32m---- 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\e[0m\n");
    for (int i = 0; i < ceil(size / 16.0); i++) {
        printf("\e[32m%04x\e[0m ", i * 0x10);
        for (int j = 0; j < 16; j++) {
            int k = i * 16 + j;
            if (k >= size) { printf("   "); continue; }
            printf("%02x ", mem[k]);
        }
        for (int j = 0; j < 16; j++) {
            int k = i * 16 + j;
            if (k >= size) { putchar(' '); continue; }
            char c = mem[k];
            if (std::isprint(c)) {
                printf("%c", c);
            }
            else {
                printf("\e[32m.\e[0m");
            }
        }
        printf("\n");
    }
}

void dump_bat_region(libtabfs_bat_t* bat) {
    int i = 0;
    libtabfs_bat_t* cur = bat->__volume->__bat_root;
    while (cur != bat) { i++; cur = cur->__next_bat; }

    printf("Dumping bat region (%d), laying at lba (0x%x):\n", i, bat->__lba);
    printf("  - block count: %d\n", bat->block_count);
    printf("  - lba of next region: 0x%x\n", bat->next_bat);

    long count = libtabfs_bat_getcount(bat);
    printf("  - lba count: 0x%lx / %ld\n", count, count);

    long long start_lba = libtabfs_bat_getstart(bat);
    printf("  - lba range: 0x%llx - 0x%llx\n", start_lba, start_lba + count - 1);

    printf("  - data:\n");
    // dump_mem(bat->data, libtabfs_bat_getDatabyteCount(bat));

    int last_valueable_byte = libtabfs_bat_getDatabyteCount(bat) - 1;
    while (bat->data[last_valueable_byte] == 0x00) {
        last_valueable_byte--;
    }

    #define BITVAL(pos) ((byte & (0b1 << pos)) ? 'x' : '.')

    printf("\e[32m----");
    for (int i = 0; i < 16; i++) { printf(" %X", i); if (i == 7) { printf(" |"); } }
    printf("\e[0m");

    for (int i = 0; i <= last_valueable_byte; i++) {
        if ((i % 2) == 0) {
            int addr = start_lba + (i * 8);
            printf("\n\e[32m%04X\e[0m ", addr);
        }
        else {
            printf(" | ");
        }
        unsigned char byte = bat->data[i];
        printf(
            "%c %c %c %c %c %c %c %c",
            BITVAL(7), BITVAL(6), BITVAL(5), BITVAL(4),
            BITVAL(3), BITVAL(2), BITVAL(1), BITVAL(0)
        );
    }
    putchar('\n');

    #undef BITVAL

}

void dump_entrytable_cache(libtabfs_volume_t* volume) {
    libtabfs_linkedlist_entry_t* cur = volume->__table_cache->head;
    printf("Entrytable cache:\n");
    while (cur != NULL) {
        libtabfs_entrytable_t* tab = (libtabfs_entrytable_t*) cur->data;
        printf("  - entrytable on lba 0x%x; size %d\n", tab->__lba, tab->__byteSize);
        cur = cur->next;
    }
}

static const char* typenames[] = {
    "unknown", "dir", "fat_file", "segmented_file", "chr_dev", "blk_dev", "fifo", "symlink", "socket",
    "continuous_file", "longname", "unused", "unused", "unused", "next-link", "kernel"
};

#define ACL_STR(acl) \
    ((acl & 0b100) ? 'R' : '-'), ((acl & 0b010) ? 'W' : '-'), ((acl & 0b001) ? 'X' : '-')

void dump_entrytable_entry(libtabfs_entrytable_entry_t* e, libtabfs_volume_t* volume) {
    if (e->flags.type == LIBTABFS_ENTRYTYPE_UNKNOWN) {
        printf("free\n");
    }
    else if (e->flags.type == LIBTABFS_ENTRYTYPE_LONGNAME) {
        libtabfs_entrytable_longname_t* lne = (libtabfs_entrytable_longname_t*) e;
        printf("longname '%s'\n", lne->name);
    }
    else if (e->flags.type == LIBTABFS_ENTRYTYPE_TABLEINFO) {
        libtabfs_entrytable_tableinfo_t* tabinf = (libtabfs_entrytable_tableinfo_t*) e;
        printf("tableinfo\n");
        printf("  - parent: lba 0x%X | size %d\n", tabinf->parent_lba, tabinf->parent_size);
        printf("  - prev section: lba 0x%X | size %d\n", tabinf->prev_lba, tabinf->prev_size);
        printf("  - next section: lba 0x%X | size %d\n", tabinf->next_lba, tabinf->next_size);
    }
    else {
        printf("\n");
        printf("  - rawflags: 0x%X\n", e->rawflags);
        printf("  - type: %s (%d)\n", typenames[e->flags.type], e->flags.type);
        printf(
            "  - sticky: %s | set-uid: %s | set-gid: %s\n",
            (e->flags.sticky ? "yes" : "no"),
            (e->flags.set_uid ? "yes" : "no"),
            (e->flags.set_gid ? "yes" : "no")
        );
        printf(
            "  - user: %c%c%c | group: %c%c%c | other: %c%c%c\n",
            ACL_STR(LIBTABFS_TAB_ENTRY_ACLUSR(e)),
            ACL_STR(LIBTABFS_TAB_ENTRY_ACLGRP(e)),
            ACL_STR(LIBTABFS_TAB_ENTRY_ACLOTH(e))
        );
        printf(
            "  - c_time: %lld | m_time: %lld | a_time: %lld\n",
            e->create_ts.i64_data, e->modify_ts.i64_data, e->access_ts.i64_data
        );

        printf("  - data: \n");
        switch (e->flags.type) {
            case LIBTABFS_ENTRYTYPE_DIR: {
                printf("    - lba: 0x%X | size: %d\n", e->data.dir.lba, e->data.dir.size);
                break;
            }
            // LIBTABFS_ENTRYTYPE_FILE_FAT
            // LIBTABFS_ENTRYTYPE_FILE_SEG
            case LIBTABFS_ENTRYTYPE_DEV_CHR:
            case LIBTABFS_ENTRYTYPE_DEV_BLK: {
                printf("    - dev_id: 0x%X | dev_flags: 0x%X\n", e->data.dev.id, e->data.dev.flags);
                break;
            }
            case LIBTABFS_ENTRYTYPE_FIFO: {
                printf("    - bufferSize: %d\n", e->data.fifo.bufferSize);
                break;
            }
            case LIBTABFS_ENTRYTYPE_SYMLINK: {
                printf("    - offset: %d\n", e->data.link.offset);
                break;
            }
            case LIBTABFS_ENTRYTYPE_SOCKET: {
                printf("    - ipv4_or_serialnum: 0x%x\n", e->data.sock.ipv4_or_serialnum);
                break;
            }
            case LIBTABFS_ENTRYTYPE_FILE_CONTINUOUS:
            case LIBTABFS_ENTRYTYPE_KERNEL: {
                printf("    - lba: 0x%X | size: %d\n", e->data.lba_and_size.lba, e->data.lba_and_size.size);
                break;
            }
        }

        if (e->longname_data.longname_identifier == 0x00) {
            printf("  - name: '%s'\n", e->name);
        }
        else {
            printf("  - name: longname { lba: 0x%X, off: %d }\n", e->longname_data.longname_lba, e->longname_data.longname_offset);

            libtabfs_entrytable_t* tab = libtabfs_get_entrytable(volume, e->longname_data.longname_lba, e->longname_data.longname_lba_size);
            libtabfs_entrytable_longname_t* lne = (libtabfs_entrytable_longname_t*) &( tab->entries[e->longname_data.longname_offset] );
            printf("    - '%s'\n", lne->name);
        }
    }
}

void dump_entrytable_region(libtabfs_entrytable_t* entrytable) {
    int entryCount = entrytable->__byteSize / 64;

    printf("Dumping entrytable region laying on lba (0x%X)\n", entrytable->__lba);
    printf("  - size: %d | entryCount: %d\n", entrytable->__byteSize, entryCount);

    int last_valueable_entry = entryCount - 1;
    while ( entrytable->entries[last_valueable_entry].flags.type == LIBTABFS_ENTRYTYPE_UNKNOWN ) {
        last_valueable_entry--;
    }

    for (int i = 0; i <= last_valueable_entry; i++) {
        libtabfs_entrytable_entry_t* e = &( entrytable->entries[i] );
        if (e->flags.type == LIBTABFS_ENTRYTYPE_UNKNOWN) {
            printf("  - entry %d: free\n", i);
        }
        else if (e->flags.type == LIBTABFS_ENTRYTYPE_LONGNAME) {
            libtabfs_entrytable_longname_t* lne = (libtabfs_entrytable_longname_t*) e;
            printf("  - entry %d: longname '%s'\n", i, lne->name);
        }
        else if (e->flags.type == LIBTABFS_ENTRYTYPE_TABLEINFO) {
            libtabfs_entrytable_tableinfo_t* tabinf = (libtabfs_entrytable_tableinfo_t*) e;
            printf("  - entry %d: tableinfo\n", i);
            printf("    - parent: lba 0x%X | size %d\n", tabinf->parent_lba, tabinf->parent_size);
            printf("    - prev section: lba 0x%X | size %d\n", tabinf->prev_lba, tabinf->prev_size);
            printf("    - next section: lba 0x%X | size %d\n", tabinf->next_lba, tabinf->next_size);
        }
        else {
            printf("  - entry %d: \n", i);
            printf("    - rawflags: 0x%X\n", e->rawflags);
            printf("    - type: %s (%d)\n", typenames[e->flags.type], e->flags.type);
            printf(
                "    - sticky: %s | set-uid: %s | set-gid: %s\n",
                (e->flags.sticky ? "yes" : "no"),
                (e->flags.set_uid ? "yes" : "no"),
                (e->flags.set_gid ? "yes" : "no")
            );
            printf(
                "    - user: %c%c%c | group: %c%c%c | other: %c%c%c\n",
                ACL_STR(LIBTABFS_TAB_ENTRY_ACLUSR(e)),
                ACL_STR(LIBTABFS_TAB_ENTRY_ACLGRP(e)),
                ACL_STR(LIBTABFS_TAB_ENTRY_ACLOTH(e))
            );
            printf(
                "    - c_time: %lld | m_time: %lld | a_time: %lld\n",
                e->create_ts.i64_data, e->modify_ts.i64_data, e->access_ts.i64_data
            );

            printf("    - data: \n");
            switch (e->flags.type) {
                case LIBTABFS_ENTRYTYPE_DIR: {
                    printf("      - lba: 0x%X | size: %d\n", e->data.dir.lba, e->data.dir.size);
                    break;
                }
                // LIBTABFS_ENTRYTYPE_FILE_FAT
                // LIBTABFS_ENTRYTYPE_FILE_SEG
                case LIBTABFS_ENTRYTYPE_DEV_CHR:
                case LIBTABFS_ENTRYTYPE_DEV_BLK: {
                    printf("      - dev_id: 0x%X | dev_flags: 0x%X\n", e->data.dev.id, e->data.dev.flags);
                    break;
                }
                case LIBTABFS_ENTRYTYPE_FIFO: {
                    printf("      - bufferSize: %d\n", e->data.fifo.bufferSize);
                    break;
                }
                case LIBTABFS_ENTRYTYPE_SYMLINK: {
                    printf("      - offset: %d\n", e->data.link.offset);
                    break;
                }
                case LIBTABFS_ENTRYTYPE_SOCKET: {
                    printf("      - ipv4_or_serialnum: 0x%X\n", e->data.sock.ipv4_or_serialnum);
                    break;
                }
                case LIBTABFS_ENTRYTYPE_FILE_CONTINUOUS:
                case LIBTABFS_ENTRYTYPE_KERNEL: {
                    printf("      - lba: 0x%X | size: %d\n", e->data.lba_and_size.lba, e->data.lba_and_size.size);
                    break;
                }
            }

            if (e->longname_data.longname_identifier == 0x00) {
                printf("    - name: '%s'\n", e->name);
            }
            else {
                printf("    - name: longname { lba: 0x%X, off: %d }\n", e->longname_data.longname_lba, e->longname_data.longname_offset);
            }
        }
    }
}

void dump_fat_region(libtabfs_fat_t* fat) {
    int entryCount = (fat->__byteSize / 16) - 1;
    
    printf("Dumping fat region laying on lba (0x%X)\n", fat->__lba);
    printf("  - size: %d | entryCount: %d\n", fat->__byteSize, entryCount);

    int last_valueable_entry = entryCount - 1;
    while ( fat->entries[last_valueable_entry].index == 0 && fat->entries[last_valueable_entry].lba == 0 ) {
        last_valueable_entry--;
    }

    for (int i = 0; i <= last_valueable_entry; i++) {
        libtabfs_fat_entry_t* e = &( fat->entries[i] );
        if (e->index == 0 && e->lba == 0) {
            printf("  - entry %d: free\n", i);
        }
        else {
            printf("  - entry %d: index=%d, lba=0x%X, modify_date=%lld\n", i, e->index, e->lba, e->modify_date.i64_data);
        }
    }
}