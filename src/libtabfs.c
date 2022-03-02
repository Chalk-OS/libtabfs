#include "common.h"
#include "bridge.h"
#include "volume.h"
#include "bat.h"
#include "entrytable.h"

const char* libtabfs_errstr(libtabfs_error error) {
    switch (error) {
        case LIBTABFS_ERR_NONE: return "none";
        case LIBTABFS_ERR_ARGS: return "argument error";
        case LIBTABFS_ERR_GENERIC: return "generic error";
        case LIBTABFS_ERR_NO_BOOTSIG: return "no bootsignature";
        case LIBTABFS_ERR_WRONG_MAGIC: return "missing or wrong magic";
        case LIBTABFS_ERR_LABEL_TOLONG: return "volume label to long";
        case LIBTABFS_ERR_RANGE_NOSPACE: return "no space at position to satisfy range";
        case LIBTABFS_ERR_DEVICE_NOSPACE: return "no space on disk left";
        case LIBTABFS_ERR_NAME_TOLONG: return "name to long";
        case LIBTABFS_ERR_IS_NO_DIR: return "entry is no directory";
        case LIBTABFS_ERR_NO_PERM: return "no permission";
        case LIBTABFS_ERR_DIR_FULL: return "directory is full";
        case LIBTABFS_ERR_NOT_FOUND: return "could not find entry";
        case LIBTABFS_ERR_OFFSET_AFTER_FILE_END: return "offset is after end of file";
        default: return "unknown error code";
    }
}

#ifdef LIBTABFS_DEFAULT_REALLOC
    void* libtabfs_realloc(void* old, int old_size, int new_size) {
        void* new_mem = libtabfs_alloc(new_size);
        libtabfs_memcpy( new_mem, old, old_size );
        libtabfs_free(old, old_size);
        return new_mem;
    }
#endif