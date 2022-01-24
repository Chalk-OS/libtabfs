#include "./utils.hpp"
#include <stdio.h>
#include <cctype>
#include <string.h>

extern void* example_disk;

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

void dump_mem(uint8_t* mem, int size) {
    printf("\e[32m---- 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\e[0m\n");
    //for (int i = 0; i < size; i++) {
    //    if ((i % 16) == 0) {
    //        printf("\n\e[32m%02x\e[0m ", i / 16);
    //    }
    //    printf("%02x ", mem[i]);
    //}
    //printf("\n");

    for (int i = 0; i < (size / 16); i++) {
        printf("\e[32m%04x\e[0m ", i * 0x10);
        for (int j = 0; j < 16; j++) {
            printf("%02x ", mem[i * 16 + j]);
        }
        for (int j = 0; j < 16; j++) {
            char c = mem[i * 16 + j];
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
    printf("  - lba count: 0x%x / %d\n", count, count);

    long long start_lba = libtabfs_bat_getstart(bat);
    printf("  - lba range: 0x%x - 0x%x\n", start_lba, start_lba + count - 1);

    printf("  - data:\n");
    dump_mem(bat->data, libtabfs_bat_getDatabyteCount(bat));
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

void dump_entrytable_region(libtabfs_entrytable_t* entrytable) {
    int entryCount = entrytable->__byteSize / 64;
    static const char* typenames[] = {
        "unknown", "dir", "fat_file", "segmented_file", "chr_dev", "blk_dev", "fifo", "symlink", "socket",
        "continuous_file", "longname", "unused", "unused", "unused", "next-link", "kernel"
    };

    #define ACL_STR(acl) \
        ((acl & 0b100) ? 'R' : '-'), ((acl & 0b010) ? 'W' : '-'), ((acl & 0b001) ? 'X' : '-')

    printf("Dumping entrytable region laying on lba (0x%x)\n", entrytable->__lba);
    printf("  - size: %d | entryCount: %d\n", entrytable->__byteSize, entryCount);
    for (int i = 0; i < entryCount; i++) {
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
            printf("    - parent: lba 0x%x | size %d\n", tabinf->parent_lba, tabinf->parent_size);
            printf("    - prev section: lba 0x%x | size %d\n", tabinf->prev_lba, tabinf->prev_size);
            printf("    - next section: lba 0x%x | size %d\n", tabinf->next_lba, tabinf->next_size);
        }
        else {
            printf("  - entry %d: \n", i);
            printf("    - rawflags: 0x%x\n", e->rawflags);
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
                "    - c_time: %ld | m_time: %ld | a_time: %ld\n",
                e->create_ts.i64_data, e->modify_ts.i64_data, e->access_ts.i64_data
            );

            printf("    - data: \n");
            switch (e->flags.type) {
                case LIBTABFS_ENTRYTYPE_DIR: {
                    printf("      - lba: 0x%x | size: %d\n", e->data.dir.lba, e->data.dir.size);
                    break;
                }
                // LIBTABFS_ENTRYTYPE_FILE_FAT
                // LIBTABFS_ENTRYTYPE_FILE_SEG
                case LIBTABFS_ENTRYTYPE_DEV_CHR:
                case LIBTABFS_ENTRYTYPE_DEV_BLK: {
                    printf("      - dev_id: 0x%x | dev_flags: 0x%x\n", e->data.dev.id, e->data.dev.flags);
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
                    printf("      - ipv4_or_serialnum: 0x%x\n", e->data.sock.ipv4_or_serialnum);
                    break;
                }
                case LIBTABFS_ENTRYTYPE_FILE_CONTINUOUS:
                case LIBTABFS_ENTRYTYPE_KERNEL: {
                    printf("      - lba: 0x%x | size: %d\n", e->data.lba_and_size.lba, e->data.lba_and_size.size);
                    break;
                }
            }

            if (e->longname_data.longname_identifier == 0x00) {
                printf("    - name: '%s'\n", e->name);
            }
            else {
                printf("    - name: longname { lba: 0x%x, off: %d }\n", e->longname_data.longname_lba, e->longname_data.longname_offset);
            }
        }
    }
}