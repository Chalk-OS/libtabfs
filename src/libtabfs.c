/*#define LIBTABFS_ALLOC
#define LIBTABFS_FREE

#define LIBTABFS_DEVICE_ARGS   int __linux_dev_t

#define LIBTABFS_READ_DEVICE(LIBTABFS_DEVICE_ARGS, lba_addr, off, buf, bufSize)
#define LIBTABFS_WRITE_DEVICE(LIBTABFS_DEVICE_ARGS, lba_addr, off, buf, bufSize)

#define LIBTABFS_DEVICE_PARAMS(header)  header->__linux_dev_t

#define LIBTABFS_HEADER_DEVICE_FIELDS   \
    int __linux_dev_t;

#define LIBTABFS_DEVICE_ARGS_TO_HEADER(header)  \
    header->__linux_dev_t = __linux_dev_t;

#define LIBTABFS_IMPLEMENTATION
#include "libtabfs.h"*/