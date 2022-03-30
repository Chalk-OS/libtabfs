#ifndef __LIBTABFS_COMMON_H__
#define __LIBTABFS_COMMON_H__

#include <stdbool.h>
#include <stddef.h>

//===========================================================================
// Misc.
//===========================================================================
#define LIBTABFS_PACKED    __attribute__((packed))
#define LIBTABFS_PTR_SIZE  sizeof(void*)
#define LIBTABFS_INT_SIZE  sizeof(int)
#define LIBTABFS_LONG_LONG_SIZE    sizeof(long long)

typedef union libtabfs_time {
    unsigned char i8_data[8];
    unsigned short i16_data[4];
    unsigned int i32_data[2];
    unsigned long long i64_data;
} libtabfs_time_t;

//===========================================================================
// LBA types
//===========================================================================
union libtabfs_lba_48 {
    unsigned long long i64;
    unsigned int i32[2];
};
typedef union libtabfs_lba_48 libtabfs_lba_48_t;
#define LIBTABFS_LBA48_TOI(lba) (long long)( lba.i64 )

typedef unsigned int libtabfs_lba_28_t;

#define LIBTABFS_LBA48_TO_LBA28(lba)    (libtabfs_lba_28_t)( lba.i32[0] & 0x0fffffff )

#define LIBTABFS_INVALID_LBA28          0x80000000
#define LIBTABFS_IS_INVALID_LBA28(lba)  ((lba & 0x80000000) != 0)

//===========================================================================
// Errorcodes
//===========================================================================
typedef unsigned int libtabfs_error;
#define LIBTABFS_ERR_NONE           0
#define LIBTABFS_ERR_ARGS           1
#define LIBTABFS_ERR_GENERIC        2
#define LIBTABFS_ERR_NO_BOOTSIG     3
#define LIBTABFS_ERR_WRONG_MAGIC    4
#define LIBTABFS_ERR_LABEL_TOLONG   5
#define LIBTABFS_ERR_RANGE_NOSPACE  6
#define LIBTABFS_ERR_DEVICE_NOSPACE 7
#define LIBTABFS_ERR_NAME_TOLONG    8
#define LIBTABFS_ERR_IS_NO_DIR      9
#define LIBTABFS_ERR_NO_PERM        10
#define LIBTABFS_ERR_DIR_FULL       11
#define LIBTABFS_ERR_NOT_FOUND      12
#define LIBTABFS_ERR_OFFSET_AFTER_FILE_END  13
#define LIBTABFS_ERR_FAT_FULL       14

/**
 * @brief converts an error number into an error string
 * 
 * @param error 
 * @return const char* 
 */
const char* libtabfs_errstr(libtabfs_error error);

//===========================================================================
// Internal symbols
//===========================================================================
#define PTR_SIZE    sizeof(void*)

#endif // __LIBTABFS_COMMON_H__