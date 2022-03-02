// Tool to inspect file that houses an tabfs filesystem

extern "C" {
    #include "libtabfs.h"
}

#include "dump.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <vector>
#include <functional>

extern "C" {

    void* libtabfs_alloc(int size) {
        void* ptr = calloc(size, sizeof(uint8_t));
        return ptr;
    }

    void libtabfs_free(void* ptr, int size) {
        free(ptr);
    }

    void libtabfs_memcpy(void* dest, void* src, int size) {
        memcpy(dest, src, size);
    }

    int libtabfs_strlen(char* str) {
        return strlen(str);
    }

    char* libtabfs_strchr(char* str, char c) {
        return strchr(str, c);
    }

    int libtabfs_strcmp(char* a, char* b) {
        return strcmp(a, b);
    }

    void libtabfs_read_device(void* dev_data, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize) {
        fseek((FILE*) dev_data, (lba_address * 512) + offset, SEEK_SET);
        fread(buffer, sizeof(uint8_t), bufferSize, (FILE*) dev_data);
    }

    void libtabfs_write_device(void* dev_data, long long lba_address, bool is_absolute_lba, int offset, void* buffer, int bufferSize) {
        // no writing when inspecting...
        // fseek((FILE*) dev_data, (lba_address * 512) + offset, SEEK_SET);
        // fwrite(buffer, sizeof(uint8_t), bufferSize, (FILE*) dev_data);
    }

}

struct option loptions[] = {
    {"bat", no_argument, 0, 'b'},
    {"tablecache", no_argument, 0, 'c'},
    {"volume", no_argument, 0, 'v'},
    {"stat", required_argument, 0, 's'},
    {"read", required_argument, 0, 'r'},
    {"uses", required_argument, 0, 'u'},
    {0, 0, 0, 0}
};

struct opt_struct {
    int opt;
    char* optarg;
};

static void print_lvl(int lvl) {
    for (int i = 0; i < lvl; i++) {
        putchar(' '); putchar(' ');
    }
}

static char* concat_path(char* prefix, char* suffix) {
    int prefix_len = strlen(prefix);
    int suffix_len = strlen(suffix);

    int pathlen = prefix_len + suffix_len + 1;
    char* path = (char*) malloc(pathlen + 1);

    strncpy(path, prefix, prefix_len);
    path[prefix_len] = '/';
    strncpy(path + prefix_len + 1, suffix, suffix_len);

    path[pathlen] = '\0';
    return path;
}

typedef std::function<void(char*, libtabfs_entrytable_entry_t*)> record_entry_cb_t;
bool search_lba_uses_in_entrytable(char* abspath, libtabfs_entrytable_t* entrytable, libtabfs_lba_28_t lba_to_search, record_entry_cb_t record_entry_cb) {
    int entryCount = entrytable->__byteSize / 64;
    printf("entrytable for '%s/' on lba 0x%X: ", abspath, entrytable->__lba);
    bool found = false;
    #define REC(str, ...)   if (!found) { putchar('\n'); found = true; } \
                            printf(str "\n" , ## __VA_ARGS__);

    libtabfs_entrytable_tableinfo_t* tabinf = (libtabfs_entrytable_tableinfo_t*) &(entrytable->entries[0]);
    if (tabinf->next_lba   == lba_to_search) { REC("  - tableinfo.next_lba");   }
    if (tabinf->prev_lba   == lba_to_search) { REC("  - tableinfo.prev_lba");   }
    if (tabinf->parent_lba == lba_to_search) { REC("  - tableinfo.parent_lba"); }

    for (int i = 1; i < entryCount; i++) {
        libtabfs_entrytable_entry_t* entry = &(entrytable->entries[i]);
        switch (entry->flags.type) {
            case LIBTABFS_ENTRYTYPE_UNKNOWN: { /* do nothing */ break; }
            case LIBTABFS_ENTRYTYPE_DIR: {
                char* name = NULL;
                libtabfs_entry_get_name(entrytable->__volume, entry, &name);
                if (entry->data.dir.lba == lba_to_search) {
                    REC("  - entry %d / '%s'; lba is link to entrytable", i, name);
                }
                char* abs_name = concat_path(abspath, name);
                record_entry_cb(abs_name, entry);
                break;
            }
            default: {
                REC("  - entry %d: dont know to handle type %d\n", i, entry->flags.type);
            }
        }
    }

    #undef REC

    if (!found) {
        printf("nothing found\n");
    }
    return found;
}

int main(int argc, char* argv[]) {

    std::vector<opt_struct> options;
    int c;
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "bcvs:r:u:", loptions, &option_index);
        if (c == -1) { break; }

        switch (c) {
            case 'b':
            case 'c':
            case 'v':
            {
                options.push_back((opt_struct) { .opt = c, .optarg = NULL });
                break;
            }
            case 's':
            case 'r':
            case 'u':
            {
                options.push_back((opt_struct) { .opt = c, .optarg = optarg });
                break;
            }

            default:
                abort();
        }
    }

    if (!(optind < argc)) {
        puts("Usage: tabfs_inspect [options] <tabfs image to inspect>");
        return 1;
    }

    char* devFile = argv[optind];
    FILE* devFileHandle = fopen(devFile, "rb");
    if (devFileHandle == NULL) {
        perror("Failed to open devicefile");
        return 1;
    }

    libtabfs_volume_t* volume = NULL;
    libtabfs_error err = libtabfs_new_volume(devFileHandle, 0x0, true, &volume);
    if (err != LIBTABFS_ERR_NONE) {
        printf("Failed to open volume: %s (%d)\n", libtabfs_errstr(err), err);
        fclose(devFileHandle);
        return 1;
    }

    for (opt_struct& opt : options) {
        switch (opt.opt) {
            case 'b': {
                libtabfs_bat_t* bat = volume->__bat_root;
                while (bat != NULL) {
                    dump_bat_region(bat);
                    bat = bat->__next_bat;
                }
                putchar('\n');
                break;
            }
            case 'c': {
                dump_entrytable_cache(volume);
                putchar('\n');
                break;
            }
            case 'v': {
                printf("Volume Informations:\n");
                printf("  label: %s\n", volume->volume_label);
                printf("  min LBA: 0x%X\n", volume->min_LBA);
                printf("  bat start LBA: 0x%X\n", volume->bat_LBA);
                printf("  max LBA: 0x%X\n", volume->max_LBA);
                break;
            }
            case 's': {
                if (strcmp(opt.optarg, "/") == 0) {
                    printf("Cant get informations about entry of roottable!\n\n");
                    break;
                }

                libtabfs_entrytable_entry_t* e;
                libtabfs_error err = libtabfs_entrytab_traversetree(volume->__root_table, opt.optarg + 1, false, 0, 0, &e, NULL, NULL);
                if (err != LIBTABFS_ERR_NONE) {
                    printf("Failed to get entry '%s': %s (%d)\n", opt.optarg, libtabfs_errstr(err), err);
                }
                else if (e == NULL) {
                    printf("Could not find entry %s\n", opt.optarg);
                }
                else {
                    printf("%s: ", opt.optarg);
                    dump_entrytable_entry(e, volume);
                }
                putchar('\n');
                break;
            }
            case 'r': {
                if (strcmp(opt.optarg, "/") == 0) {
                    libtabfs_entrytable_t* tab = volume->__root_table;
                    while (tab != NULL) {
                        dump_entrytable_region(tab);
                        tab = libtabfs_entrytable_nextsection(tab);
                    }
                }
                else {
                    libtabfs_entrytable_entry_t* e;
                    libtabfs_error err = libtabfs_entrytab_traversetree(volume->__root_table, opt.optarg + 1, false, 0, 0, &e, NULL, NULL);
                    if (err != LIBTABFS_ERR_NONE) {
                        printf("Failed to get entry '%s': %s (%d)\n", opt.optarg, libtabfs_errstr(err), err);
                    }
                    else if (e == NULL) {
                        printf("Could not find entry %s\n", opt.optarg);
                    }
                    else {
                        switch (e->flags.type) {
                            case LIBTABFS_ENTRYTYPE_DIR: {
                                libtabfs_entrytable_t* tab = libtabfs_get_entrytable(volume, e->data.dir.lba, e->data.dir.size);
                                while (tab != NULL) {
                                    dump_entrytable_region(tab);
                                    tab = libtabfs_entrytable_nextsection(tab);
                                }
                                break;
                            }
                        
                            default: break;
                        }
                    }
                }
                break;
            }
            case 'u': {
                // find the uses of an specific block.
                libtabfs_lba_28_t lba_to_search;
                if (opt.optarg[0] == '0' && opt.optarg[1] == 'x') {
                    lba_to_search = strtoul(opt.optarg, NULL, 16);
                }
                else {
                    puts("Needs an hexadecimal number as argument for --uses / -u");
                    abort();
                }

                if (!libtabfs_bat_isFree(volume, lba_to_search)) {
                    printf("Lba 0x%X is marked as free\n", lba_to_search);
                }

                // search ALL nodes...
                std::vector<std::tuple<char*, libtabfs_entrytable_entry_t*>> stack;
                record_entry_cb_t record_entry_cb = [&stack] (char* path, libtabfs_entrytable_entry_t* e) {
                    stack.push_back( std::make_tuple(path, e) );
                };
                search_lba_uses_in_entrytable("", volume->__root_table, lba_to_search, record_entry_cb);
                for (std::tuple<char*, libtabfs_entrytable_entry_t*> element : stack) {
                    char* path = std::get<0>(element);
                    libtabfs_entrytable_entry_t* entry = std::get<1>(element);

                    if (entry->flags.type == LIBTABFS_ENTRYTYPE_DIR) {
                        libtabfs_entrytable_t* tab = libtabfs_get_entrytable(volume, entry->data.dir.lba, entry->data.dir.size);
                        search_lba_uses_in_entrytable(path, tab, lba_to_search, record_entry_cb);
                    }

                    free(path);
                }

                break;
            }
        }
    }

}