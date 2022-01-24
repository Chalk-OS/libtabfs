#include "bridge.h"

#include "linkedlist.h"

#include <stddef.h>

libtabfs_linkedlist_t* libtabfs_linkedlist_create(libtabfs_free_callback free_callback) {
    libtabfs_linkedlist_t* list = (libtabfs_linkedlist_t*) libtabfs_alloc(sizeof(libtabfs_linkedlist_t));
    list->head = NULL;
    list->tail = NULL;
    list->free_callback = free_callback;
    return list;
}

void libtabfs_linkedlist_destroy(libtabfs_linkedlist_t* list) {
    libtabfs_linkedlist_entry_t* cur = list->head;
    while (cur != NULL) {
        libtabfs_linkedlist_entry_t* tmp = cur;
        cur = cur->next;
        list->free_callback(tmp->data);
        libtabfs_free(tmp, sizeof(libtabfs_linkedlist_entry_t));
    }
    list->head = NULL;
    list->tail = NULL;
    libtabfs_free(list, sizeof(libtabfs_linkedlist_t));
}

void libtabfs_linkedlist_add(libtabfs_linkedlist_t* list, void* data) {
    libtabfs_linkedlist_entry_t* entry = (libtabfs_linkedlist_entry_t*) libtabfs_alloc(sizeof(libtabfs_linkedlist_entry_t));
    entry->data = data;

    if (list->tail == NULL) {
        list->tail = entry;
        list->head = entry;
    }
    else {
        list->tail->next = entry;
        list->tail = entry;
    }
}

libtabfs_linkedlist_entry_t* libtabfs_linkedlist_find(libtabfs_linkedlist_t* list, libtabfs_find_callback callback) {
    libtabfs_linkedlist_entry_t* cur = list->head;
    while (cur != NULL) {
        if (callback(cur)) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void libtabfs_linkedlist_remove(libtabfs_linkedlist_t* list, libtabfs_linkedlist_entry_t* entry) {
    libtabfs_linkedlist_entry_t* prev = list->head;
    libtabfs_linkedlist_entry_t** prev_link = &(list->head);
    libtabfs_linkedlist_entry_t* cur = list->head;
    while (cur != NULL) {
        if (cur == entry) {
            *prev_link = cur->next;
            if (list->tail == cur) {
                list->tail = prev;
            }
            list->free_callback(cur->data);
            libtabfs_free(cur, sizeof(libtabfs_linkedlist_entry_t));
            return;
        }
        else {
            prev_link = &(cur->next);
            prev = cur;
            cur = cur->next;
        }
    }
}

void libtabfs_linkedlist_remove_data(libtabfs_linkedlist_t* list, void* data) {
    libtabfs_linkedlist_entry_t* prev = list->head;
    libtabfs_linkedlist_entry_t** prev_link = &(list->head);
    libtabfs_linkedlist_entry_t* cur = list->head;
    while (cur != NULL) {
        if (cur->data == data) {
            *prev_link = cur->next;
            if (list->tail == cur) {
                list->tail = prev;
            }
            // no usage of the free calback here since the caller does this!
            libtabfs_free(cur, sizeof(libtabfs_linkedlist_entry_t));
            return;
        }
        else {
            prev_link = &(cur->next);
            prev = cur;
            cur = cur->next;
        }
    }
}