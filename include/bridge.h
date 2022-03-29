#ifndef __LIBTABFS_BRIDGE_H__
#define __LIBTABFS_BRIDGE_H__

#include "common.h"

/**
 * @brief reads data from a device
 * 
 * @param dev_data the devicedata provided in the call to libtabfs_new_volume
 * @param lba the lba to read from
 * @param is_absolute_lba true if the lba is absolute; false otherwise (relative to partition or similar)
 * @param offset offset into the lba block
 * @param buffer buffer to read into
 * @param buffer_size size of the buffer; also the count how many bytes to read
 */
extern void libtabfs_read_device(
    void* dev_data,
    libtabfs_lba_28_t lba, bool is_absolute_lba, int offset,
    void* buffer, int buffer_size
);

/**
 * @brief writes data to a device
 * 
 * @param dev_data the devicedata provided in the call to libtabfs_new_volume
 * @param lba the lba to write to
 * @param is_absolute_lba true if the lba is absolute; false otherwise (relative to partition or similar)
 * @param offset offset into the lba block
 * @param buffer buffer to read from
 * @param buffer_size size of the buffer; also the count how many bytes to write
 */
extern void libtabfs_write_device(
    void* dev_data,
    libtabfs_lba_28_t lba, bool is_absolute_lba, int offset,
    void* buffer, int buffer_size
);

/**
 * @brief sets a byterange to an specific value. if offset is 0 and size is equal the block size,
 * then this function sets an whole block to the specific value
 * 
 * @param dev_data the devicedata provided in the call to libtabfs_new_volume
 * @param lba the lba to write to
 * @param is_absolute_lba true if the lba is absolute; false otherwise (relative to partition or similar)
 * @param offset offset into the lba block
 * @param b value all bytes in the range should be set to
 * @param size how many bytes should be set
 */
extern void libtabfs_set_range_device(
    void* dev_data,
    libtabfs_lba_28_t lba, bool is_absolute_lba, int offset,
    unsigned char b, int size
);

/**
 * @brief gets the length of an zero-terminated string without the null-byte (0x00 or '\0')
 * 
 * @param str the string to get the length of
 * @return the length of the string
 */
extern int libtabfs_strlen(char* str);

/**
 * @brief searches an string after the first occurence of an character;
 * it should *NOT* be assumed that the given pointer of the string is the beginning of the string itself
 * 
 * @param str the string to find the character in
 * @param c the character to search for
 * @return pointer to the first occurence of the searched character; if none could be found, NULL is returned
 */
extern char* libtabfs_strchr(char* str, char c);

/**
 * @brief compares two zero-terminated strings with each other. return 0 on equal
 * 
 * @param a first string to compare
 * @param b second string to compare
 * @return 0 on equal; unequal is not specified from libtabfs other than to be non-zero
 */
extern int libtabfs_strcmp(char* a, char* b);

/**
 * @brief copies count bytes from src to dest
 * 
 * @param dest buffer to copy into
 * @param src buffer to copy from
 * @param count number of bytes to copy
 */
extern void libtabfs_memcpy(void* dest, void* src, int count);

/**
 * @brief allocates an region of memory
 * 
 * @param size bytecount to allocate
 * @return pointer to an memoryregion with an minimum amount of bytes available, specified in size
 */
extern void* libtabfs_alloc(int size);

/**
 * @brief frees / deallocates an region of memory
 * 
 * @param ptr the pointer to deallocate
 * @param size the bytecount to deallocate; is the same size as the call to libtabfs_alloc that produced the ptr argument
 */
extern void libtabfs_free(void* ptr, int size);

#ifndef libtabfs_realloc
    void* libtabfs_realloc(void* old, int old_size, int new_size);
    #define LIBTABFS_DEFAULT_REALLOC
#endif

#endif //__LIBTABFS_BRIDGE_H__