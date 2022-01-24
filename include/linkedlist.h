#ifndef __LIBTABFS_LINKEDLIST_H__
#define __LIBTABFS_LINKEDLIST_H__

#include <stdbool.h>

struct libtabfs_linkedlist_entry {
    struct libtabfs_linkedlist_entry* next;
    void* data;
};
typedef struct libtabfs_linkedlist_entry libtabfs_linkedlist_entry_t;

typedef void (*libtabfs_free_callback)(void* data);
typedef bool (*libtabfs_find_callback)(libtabfs_linkedlist_entry_t* entry);

struct libtabfs_linkedlist {
    libtabfs_free_callback free_callback;
    libtabfs_linkedlist_entry_t* head;
    libtabfs_linkedlist_entry_t* tail;
};
typedef struct libtabfs_linkedlist libtabfs_linkedlist_t;

/**
 * @brief creates an new linked list
 * 
 * @param free_callback an callback, that gets called when an entry needs to be freed; argument is the entry's data pointer
 * @return libtabfs_linkedlist_t* 
 */
libtabfs_linkedlist_t* libtabfs_linkedlist_create(libtabfs_free_callback free_callback);

void libtabfs_linkedlist_destroy(libtabfs_linkedlist_t* list);

void libtabfs_linkedlist_add(libtabfs_linkedlist_t* list, void* data);

libtabfs_linkedlist_entry_t* libtabfs_linkedlist_find(libtabfs_linkedlist_t* list, libtabfs_find_callback callback);

void libtabfs_linkedlist_remove(libtabfs_linkedlist_t* list, libtabfs_linkedlist_entry_t* entry);

void libtabfs_linkedlist_remove_data(libtabfs_linkedlist_t* list, void* data);

#endif // __LIBTABFS_LINKEDLIST_H__