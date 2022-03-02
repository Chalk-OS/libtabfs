#include <cxxspec/cxxspec.hpp>

extern "C" {
    #include "libtabfs.h"
}

#include "./utils.hpp"
#include "./bridge_impl.hpp"

libtabfs_volume_t* gVolume;

void my_linkedlist_free(void* data) {}

describe(libtabfs, $ {
    explain("libtabfs_linkedlist", $ {
        explain("libtabfs_linkedlist_create", $ {
            it("should be create- & destroyable", _ {
                libtabfs_linkedlist_t* list = libtabfs_linkedlist_create(my_linkedlist_free);
                expect(list).to_neq(nullptr);
                libtabfs_linkedlist_destroy(list);
            });
        });
        explain("libtabfs_linkedlist_add", $ {
            it("should can add entries", _ {
                libtabfs_linkedlist_t* list = libtabfs_linkedlist_create(my_linkedlist_free);
                expect(list).to_neq(nullptr);
                cleanup([list] () {
                    libtabfs_linkedlist_destroy(list);
                });

                expect(list->head).to_eq(nullptr);
                libtabfs_linkedlist_add(list, NULL);
                expect(list->head).to_neq(nullptr);
            });
        });
        explain("libtabfs_linkedlist_remove", $ {
            it("should remove entries", _ {
                libtabfs_linkedlist_t* list = libtabfs_linkedlist_create(my_linkedlist_free);
                expect(list).to_neq(nullptr);
                cleanup([list] () {
                    libtabfs_linkedlist_destroy(list);
                });

                libtabfs_linkedlist_add(list, (void*)0x1111);
                libtabfs_linkedlist_add(list, (void*)0x2222);
                libtabfs_linkedlist_add(list, (void*)0x3333);

                libtabfs_linkedlist_entry_t* middle = list->head->next;
                expect(middle).to_neq(nullptr);
                expect(middle->data).to_eq((void*)0x2222);

                libtabfs_linkedlist_remove(list, middle);

                expect(list->head).to_neq(nullptr);
                expect(list->head->data).to_eq((void*)0x1111);

                expect(list->head->next).to_neq(nullptr);
                expect(list->head->next->data).to_eq((void*)0x3333);
            });
            it("should remove an entry from the tail and setting tail accordingly", _ {
                libtabfs_linkedlist_t* list = libtabfs_linkedlist_create(my_linkedlist_free);
                expect(list).to_neq(nullptr);
                cleanup([list] () {
                    libtabfs_linkedlist_destroy(list);
                });

                libtabfs_linkedlist_add(list, (void*)0x1111);
                libtabfs_linkedlist_add(list, (void*)0x2222);
                libtabfs_linkedlist_add(list, (void*)0x3333);

                libtabfs_linkedlist_entry_t* last = list->head->next->next;
                expect(last).to_neq(nullptr);
                expect(last->data).to_eq((void*)0x3333);

                libtabfs_linkedlist_remove(list, last);

                expect(list->head).to_neq(nullptr);
                expect(list->head->data).to_eq((void*)0x1111);

                expect(list->tail).to_neq(nullptr);
                expect(list->tail->data).to_eq((void*)0x2222);
            });
        });
    });

    explain("libtabfs_new_volume", $ {
        it("should have a magic", _ {
            expect((char*)gVolume->magic).to_eq((char*)"TABFS-28");
        });
        it("should have a bat on lba 0x2", _ {
            expect(gVolume->bat_LBA).to_eq(0x2);
        });
        it("should have the root table on lba 0x3", _ {
            expect(gVolume->root_LBA).to_eq(0x3);
        });

        explain("libtabfs_volume_get_label", $ {
            it("should be 'This is an awesome volume!'", _ {
                expect(libtabfs_volume_get_label(gVolume)).to_eq((char*)"This is an awesome volume!");
            });
        });

        explain("libtabfs_volume_set_label", $ {
            it("should be setable to 'Hello world!' & sync to disk", _ {
                libtabfs_volume_set_label(gVolume, "Hello world!", true);
                expect(libtabfs_volume_get_label(gVolume)).to_eq((char*)"Hello world!");
                expect(memcmp(example_disk + 512 + 0x50, "Hello world!", 13)).to_eq(0);
            });
        });

        explain("bat - first section", $ {
            it("should be on lba 0x2", _ {
                libtabfs_bat_t* bat = gVolume->__bat_root;
                expect(bat->__lba).to_eq(0x2);
            });
            it("should have a block_count of 1", _ {
                libtabfs_bat_t* bat = gVolume->__bat_root;
                expect(bat->block_count).to_eq(1);
            });
            it("should reference on the next section on lba 0x4", _ {
                libtabfs_bat_t* bat = gVolume->__bat_root;
                expect(bat->next_bat).to_eq(0x4);
            });
        });

        explain("bat - second section", $ {
            it("should be accessible through the first section", _ {
                libtabfs_bat_t* bat = gVolume->__bat_root;
                expect(bat->__next_bat).to_not_eq(nullptr);
            });
            it("should be on lba 0x4", _ {
                libtabfs_bat_t* bat = gVolume->__bat_root;
                expect(bat->__next_bat).to_not_eq(nullptr);
                bat = bat->__next_bat;
                expect(bat->__lba).to_eq(0x4);
            });
            it("should have a block_count of 2", _ {
                libtabfs_bat_t* bat = gVolume->__bat_root;
                expect(bat->__next_bat).to_not_eq(nullptr);
                bat = bat->__next_bat;
                expect(bat->block_count).to_eq(2);
            });
        });

    });

    explain("libtabfs_bat_isFree", $ {
        it("should have non-free blocks on 0x0 - 0x4", _ {
            expect(libtabfs_bat_isFree(gVolume, 0x0)).to_eq(false);
            expect(libtabfs_bat_isFree(gVolume, 0x1)).to_eq(false);
            expect(libtabfs_bat_isFree(gVolume, 0x2)).to_eq(false);
            expect(libtabfs_bat_isFree(gVolume, 0x3)).to_eq(false);
            expect(libtabfs_bat_isFree(gVolume, 0x4)).to_eq(false);
        });
        it("should have free a block on 0x6", _ {
            expect(libtabfs_bat_isFree(gVolume, 0x6)).to_eq(true);
        });
        it("should work accross bat regions", _ {
            expect(libtabfs_bat_isFree(gVolume, 0xfd0 + 0x2)).to_eq(true);
        });
        // it("should create new bat regions if the current regions cannot hold the requested lba", _ {
        //     expect(libtabfs_bat_isFree(gVolume, 0xfd0 + 0x1fd0)).to_eq(true);
        // });
    });

    explain("libtabfs_bat_allocateChainedBlocks", $ {
        it("should allocate an range of 2 blocks on 0x6 - 0x7", _ {
            expect(libtabfs_bat_allocateChainedBlocks(gVolume, 0x2)).to_eq(0x6);
        });
        it("should set 0x6 - 0x7 in bat to non-free", _ {
            expect(libtabfs_bat_isFree(gVolume, 0x6)).to_eq(false);
            expect(libtabfs_bat_isFree(gVolume, 0x7)).to_eq(false);
        });
    });

    explain("libtabfs_bat_freeChainedBlocks", $ {
        it("should set 0x6 - 0x7 in bat to free", _ {
            expect(libtabfs_bat_isFree(gVolume, 0x6)).to_neq(true);
            expect(libtabfs_bat_isFree(gVolume, 0x7)).to_neq(true);
            libtabfs_bat_freeChainedBlocks(gVolume, 2, 0x6);
            expect(libtabfs_bat_isFree(gVolume, 0x6)).to_eq(true);
            expect(libtabfs_bat_isFree(gVolume, 0x7)).to_eq(true);
        });
    });

    explain("libtabfs_create_dir", $ {
        it("should create an directory entry as well as an entrytable on 0x6 - 0x7", _ {
            libtabfs_entrytable_t* myDir_entrytable;
            libtabfs_error err = libtabfs_create_dir(
                gVolume->__root_table, "myDir",
                { .set_uid = true, .user = { .exec = true } },
                {}, 1, 2,
                &myDir_entrytable
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(myDir_entrytable->__lba).to_eq(0x6);
            expect(myDir_entrytable->__byteSize).to_eq(512 * 2);

            libtabfs_entrytable_sync(gVolume->__root_table);

            uint8_t buff[64] = {
                0x18, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
                0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x6D, 0x79, 0x44, 0x69, 0x72, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };
            expect(memcmp(example_disk + (512 * 0x3) + 64, buff, 64)).to_eq(0);
        });
    });

    explain("libtabfs_create_chardevice", $ {
        it("should create an device with id 0x1234 and flags 0x5678", _ {
            libtabfs_error err = libtabfs_create_chardevice(
                gVolume->__root_table, "myChrDev",
                { .set_uid = true, .user = { .write = true } },
                {}, 1, 2,
                0x1234, 0x5678
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);

            libtabfs_entrytable_sync(gVolume->__root_table);
            // TODO: test the byteregion
        });
        it("should create an device with longname entry", _ {
            libtabfs_error err = libtabfs_create_chardevice(
                gVolume->__root_table, "123456789_123456789_123456789_123456789_123456789_123456789_",
                { .set_uid = true, .user = { .exec = true } },
                {}, 1, 2,
                0x1234, 0x5678
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);

            libtabfs_entrytable_sync(gVolume->__root_table);
            // TODO: test the byteregion
        });
    });

    // TODO: add an test to confirm that entry creation automatically creates an new section

    explain("libtabfs_entrytab_traversetree", $ {
        it("should get filenames in the current entrytable", _ {
            libtabfs_entrytable_entry_t* entry = NULL;
            libtabfs_error err = libtabfs_entrytab_traversetree(
                gVolume->__root_table, "myDir", true, 1, 2, &entry, NULL, NULL
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(entry->flags.type).to_eq(LIBTABFS_ENTRYTYPE_DIR);
        });
        it("should find a file in subdirectories", _ {
            libtabfs_entrytable_entry_t* mydir_entry = NULL;
            libtabfs_error err = libtabfs_entrytab_traversetree(
                gVolume->__root_table, "myDir", true, 1, 2, &mydir_entry, NULL, NULL
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(mydir_entry->flags.type).to_eq(LIBTABFS_ENTRYTYPE_DIR);

            err = libtabfs_create_chardevice(
                libtabfs_get_entrytable(gVolume, mydir_entry->data.dir.lba, mydir_entry->data.dir.size), "testChrDev",
                { .set_uid = true, .user = { .write = true } },
                {}, 1, 2,
                0x1234, 0x5678
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);

            libtabfs_entrytable_entry_t* entry = NULL;
            const char* cpath = "myDir/testChrDev";

            char* path = (char*) calloc(strlen(cpath) + 1, sizeof(char));
            cleanup([path] { free(path); });
            strcpy(path, cpath);

            err = libtabfs_entrytab_traversetree(
                gVolume->__root_table, path, true, 1, 2, &entry, NULL, NULL
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(entry->flags.type).to_eq(LIBTABFS_ENTRYTYPE_DEV_CHR);
        });
        it("should find a file with relative path", _ {
            libtabfs_entrytable_entry_t* mydir_entry = NULL;
            libtabfs_error err = libtabfs_entrytab_traversetree(
                gVolume->__root_table, "myDir", true, 1, 2, &mydir_entry, NULL, NULL
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(mydir_entry->flags.type).to_eq(LIBTABFS_ENTRYTYPE_DIR);

            libtabfs_entrytable_entry_t* entry = NULL;
            err = libtabfs_entrytab_traversetree(
                libtabfs_get_entrytable(gVolume, mydir_entry->data.dir.lba, mydir_entry->data.dir.size),
                "../myChrDev", true, 1, 2, &entry, NULL, NULL
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(entry->flags.type).to_eq(LIBTABFS_ENTRYTYPE_DEV_CHR);
        });
        it("can follow symlinks if asked to", _ {

            libtabfs_entrytable_entry_t* mydir_entry = NULL;
            libtabfs_error err = libtabfs_entrytab_traversetree(
                gVolume->__root_table, "myDir", 1, 2, true, &mydir_entry, NULL, NULL
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(mydir_entry->flags.type).to_eq(LIBTABFS_ENTRYTYPE_DIR);

            err = libtabfs_create_symlink(
                libtabfs_get_entrytable(gVolume, mydir_entry->data.dir.lba, mydir_entry->data.dir.size), "testLink",
                { .set_uid = true, .user = { .exec = true } },
                {}, 1, 2,
                "../myChrDev"
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);

            libtabfs_entrytable_entry_t* entry = NULL;
            const char* cpath = "myDir/testLink";

            char* path = (char*) calloc(strlen(cpath) + 1, sizeof(char));
            cleanup([path] { free(path); });
            strcpy(path, cpath);

            err = libtabfs_entrytab_traversetree(
                gVolume->__root_table, path, true, 1, 2, &entry, NULL, NULL
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(entry->flags.type).to_eq(LIBTABFS_ENTRYTYPE_DEV_CHR);

            err = libtabfs_entrytab_traversetree(
                gVolume->__root_table, path, false, 1, 2, &entry, NULL, NULL
            );
            expect(err).to_eq(LIBTABFS_ERR_NONE);
            expect(entry->flags.type).to_eq(LIBTABFS_ENTRYTYPE_SYMLINK);
        });
    });
});

int main(int argc, char** argv) {
    init_example_disk();

    dev_t* dev_data = (dev_t*) calloc(1, sizeof(dev_data));
    *dev_data = 0x1234;

    libtabfs_error err = libtabfs_new_volume(dev_data, 0, true, &gVolume);
    if (err != LIBTABFS_ERR_NONE) {
        printf("Error while calling libtabfs_new_volume(): %s (%d)\n", libtabfs_errstr(err), err);
        return 0;
    }

    dump_mem((uint8_t*)gVolume->__bat_root, sizeof(libtabfs_bat));

    // run specs
    cxxspec::runSpecs(--argc, ++argv);
    return 0;
}