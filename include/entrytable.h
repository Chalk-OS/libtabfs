#ifndef __LIBTABFS_ENTRYTABLE_H__
#define __LIBTABFS_ENTRYTABLE_H__

#include "./common.h"

typedef union libtabfs_time {
    unsigned char i8_data[8];
    unsigned short i16_data[4];
    unsigned int i32_data[2];
    unsigned long long i64_data;
} libtabfs_time_t;

union libtabfs_entrytable_entry_data {
    unsigned char rawdata[8];
    struct {
        libtabfs_lba_28_t lba;
        unsigned int size;
    } LIBTABFS_PACKED dir;
    struct {
        libtabfs_lba_28_t lba;
        unsigned int size;
    } LIBTABFS_PACKED lba_and_size;
    struct {
        unsigned int id;
        unsigned int flags;
    } LIBTABFS_PACKED dev;
    struct {
        unsigned int offset;
    } LIBTABFS_PACKED link;
    struct {
        unsigned int ipv4_or_serialnum;
    } LIBTABFS_PACKED sock;
    struct {
        unsigned int bufferSize;
    } LIBTABFS_PACKED fifo;
};
typedef union libtabfs_entrytable_entry_data libtabfs_entrytable_entry_data_t;

struct libtabfs_fileflags {
    bool sticky;
    bool set_gid;
    bool set_uid;
    union {
        struct {
            bool exec : 1;
            bool write : 1;
            bool read : 1;
        } user;
        unsigned char raw_user;
    };
    union {
        struct {
            bool exec : 1;
            bool write : 1;
            bool read : 1;
        } group;
        unsigned char raw_group;
    };
    union {
        struct {
            bool exec : 1;
            bool write : 1;
            bool read : 1;
        } other;
        unsigned char raw_other;
    };
} LIBTABFS_PACKED;
typedef struct libtabfs_fileflags libtabfs_fileflags_t;

struct libtabfs_entrytable_longname {
    union {
        struct {
            char __unused : 4;
            unsigned char type  : 4;
        } flags;
        unsigned char rawflags;
    };
    unsigned char name[63];
} LIBTABFS_PACKED;
typedef struct libtabfs_entrytable_longname libtabfs_entrytable_longname_t;

struct libtabfs_entrytable_tableinfo {
    union {
        struct {
            char __unused : 4;
            unsigned char type  : 4;
        } flags;
        unsigned char rawflags;
    };
    unsigned char __unused[39];
    libtabfs_lba_28_t parent_lba; int parent_size;
    libtabfs_lba_28_t prev_lba; int prev_size;
    libtabfs_lba_28_t next_lba; int next_size;
} LIBTABFS_PACKED;
typedef struct libtabfs_entrytable_tableinfo libtabfs_entrytable_tableinfo_t;

#define LIBTABFS_TAB_ENTRY_ACLUSR(e)    ((e->rawflags & 0b1) << 2 | (((e->rawflags >> 8) & 0b11000000) >> 6))
#define LIBTABFS_TAB_ENTRY_ACLGRP(e)    (((e->rawflags >> 8) & 0b00111000) >> 3)
#define LIBTABFS_TAB_ENTRY_ACLOTH(e)    ((e->rawflags >> 8) & 0b00000111)

struct libtabfs_entrytable_entry {
    union {
        struct {
            bool __unused : 1;
            bool sticky         : 1;
            bool set_gid        : 1;
            bool set_uid        : 1;
            unsigned char type  : 4;
        } flags;
        unsigned short rawflags;
    };
    libtabfs_time_t create_ts;
    libtabfs_time_t modify_ts;
    libtabfs_time_t access_ts;
    unsigned int user_id;
    unsigned int group_id;
    libtabfs_entrytable_entry_data_t data;
    union {
        char name[22];
        struct {
            char unused[9];
            libtabfs_lba_28_t longname_lba;
            int longname_lba_size;
            int longname_offset;
            unsigned char longname_identifier;
        } LIBTABFS_PACKED longname_data;
    };
} LIBTABFS_PACKED;
typedef struct libtabfs_entrytable_entry libtabfs_entrytable_entry_t;

struct libtabfs_entrytable {
    libtabfs_volume_t* __volume;
    unsigned int __byteSize;
    libtabfs_lba_28_t __lba;

    // --------------------------------
    libtabfs_entrytable_entry_t entries[1]; // actually more than one;
} LIBTABFS_PACKED;
typedef struct libtabfs_entrytable libtabfs_entrytable_t;

struct libtabfs_acl {
    bool exec  : 1;
    bool write : 1;
    bool read  : 1;
};
typedef struct libtabfs_acl libtabfs_acl_t;

#define LIBTABFS_MAKE_ACL(acl)  (libtabfs_acl_t)(raw)

#define LIBTABFS_ENTRYTYPE_UNKNOWN          0x0
#define LIBTABFS_ENTRYTYPE_DIR              0x1
#define LIBTABFS_ENTRYTYPE_FILE_FAT         0x2
#define LIBTABFS_ENTRYTYPE_FILE_SEG         0x3
#define LIBTABFS_ENTRYTYPE_DEV_CHR          0x4
#define LIBTABFS_ENTRYTYPE_DEV_BLK          0x5
#define LIBTABFS_ENTRYTYPE_FIFO             0x6
#define LIBTABFS_ENTRYTYPE_SYMLINK          0x7
#define LIBTABFS_ENTRYTYPE_SOCKET           0x8
#define LIBTABFS_ENTRYTYPE_FILE_CONTINUOUS  0x9
#define LIBTABFS_ENTRYTYPE_LONGNAME         0xA
#define LIBTABFS_ENTRYTYPE_TABLEINFO        0xE
#define LIBTABFS_ENTRYTYPE_KERNEL           0xF

#define LIBTABFS_GENERIC_ENTRY_ARGS \
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags, \
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid

#define LIBTABFS_FORWARD_GENERIC_ENTRY_ARGS     entrytable, name, fileflags, create_ts, userid, groupid

//--------------------------------------------------------------------------------
// Entrytable creation, sync & destroying
//--------------------------------------------------------------------------------

/**
 * @brief reads an entrytable section from disk; this dosnt uses the tablecache! use libtabfs_get_entrytable instead
 * this method registers the loaded section into the tablecache
 * 
 * @param volume the volume to operate on
 * @param lba the lba of the entrytable section
 * @param size the bytesize of the entrytable section
 * @return the loaded entrytable section
 */
libtabfs_entrytable_t* libtabfs_read_entrytable(libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size);

/**
 * @brief retrieves an entrytable section by first quering the tablecache; if not found, its loaded from disk using libtabfs_read_entrytable
 * 
 * @param volume the volume to operate on
 * @param lba the lba of the entrytable section
 * @param size the bytesize of the entrytable section
 * @return the requested entrytable section
 */
libtabfs_entrytable_t* libtabfs_get_entrytable(libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size);

/**
 * @brief creates an new entrytable section; adds the result to the tablecache
 * 
 * @param volume the volume to operate on
 * @param lba the lba of the entrytable section
 * @param size the bytesize of the entrytable section
 * @param parent_table the parent entrytable; should always be the first section
 * @return libtabfs_entrytable_t* 
 */
libtabfs_entrytable_t* libtabfs_create_entrytable(
    libtabfs_volume_t* volume, libtabfs_lba_28_t lba, unsigned int size, libtabfs_entrytable_t* parent_table
);

/**
 * @brief destroys / frees / unloads an entrytable section
 * 
 * @param entrytable the entrytable section to destroy
 */
void libtabfs_entrytable_destroy(libtabfs_entrytable_t* entrytable);

/**
 * @brief writes an entrytable section to disk
 * 
 * @param entrytable the entrytable section to sync to disk
 */
void libtabfs_entrytab_sync(libtabfs_entrytable_t* entrytable);

//--------------------------------------------------------------------------------
// Helper
//--------------------------------------------------------------------------------

/**
 * @brief sets the fileflags for an given entry
 * 
 * @param fileflags the fileflags to set
 * @param entry the entry to set them on
 */
void libtabfs_fileflags_to_entry(libtabfs_fileflags_t fileflags, libtabfs_entrytable_entry_t* entry);

/**
 * @brief checks acl permissions; first checks on user, then on group and lastly on other
 * 
 * @param entry the entry to check
 * @param userid the userid to perform the check as
 * @param groupid the groupid to perform the check as
 * @param perm the permission bits to check
 * @return true if the check succeeds; false otherwise
 */
bool libtabfs_check_perm(libtabfs_entrytable_entry_t* entry, unsigned int userid, unsigned int groupid, unsigned char perm);

/**
 * @brief sets the user and group owners of an entry
 * 
 * @param entry the entry to modfiy
 * @param userid the new userid
 * @param groupid the new groupid
 */
void libtabfs_entry_chown(libtabfs_entrytable_entry_t* entry, unsigned int userid, unsigned int groupid);

/**
 * @brief sets the all times of an entry
 * 
 * @param entry the entry to modify
 * @param m_time the new modfification time
 * @param a_time the new access time
 */
void libtabfs_entry_touch(libtabfs_entrytable_entry_t* entry, libtabfs_time_t m_time, libtabfs_time_t a_time);

/**
 * @brief queries the tablecache for an cached entrytable section. it does not load it from disk when it cannot be found!
 * use libtabfs_get_entrytable instead.
 * 
 * @param volume the volume to operate on
 * @param entrytable_lba the lba of the entrytable section to search for
 * @return the requested entrytable section or NULL if it couldn't be found
 */
libtabfs_entrytable_t* libtabfs_find_cached_entrytable(libtabfs_volume_t* volume, libtabfs_lba_28_t entrytable_lba);

/**
 * @brief follows all prev-link's in all tableinfo entrys to find the first section of an entrytable
 * 
 * @param section the section to start searching
 * @return the first section of the entrytable (where the prev-link is not set)
 */
libtabfs_entrytable_t* libtabfs_entrytable_get_first_section(libtabfs_entrytable_t* section);

/**
 * @brief follows the parent-link to obtain the first section of the parent entrytable of an entrytable
 * 
 * @param section the entrytable to get the parent of
 * @return the first section of the parent entrytable
 */
libtabfs_entrytable_t* libtabfs_entrytable_get_parent(libtabfs_entrytable_t* section);

/**
 * @brief returns the entrytable specified via the next-link
 * 
 * @param section the entrytable section to get the next section of
 * @return the entrytable section specified via the next-link or NULL if no next-link was configures
 */
libtabfs_entrytable_t* libtabfs_entrytable_nextsection(libtabfs_entrytable_t* section);

/**
 * @brief searches after an free entry inside an entrytable section
 * 
 * @param entrytable the entrytable section to start searching from
 * @param entry_out pointer which will be set to the found entry on success
 * @param entrytable_out optional pointer which will be set to the entrytable section containing the free entry (only on success)
 * @param offset_out optional pointer which will be set to the offset of the entry into its entrytable section (only on success)
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_entrytab_findfree(
    libtabfs_entrytable_t* entrytable,
    libtabfs_entrytable_entry_t** entry_out,
    libtabfs_entrytable_t** entrytable_out,
    int* offset_out
);

//--------------------------------------------------------------------------------
// Find entrys inside an entrytable
//--------------------------------------------------------------------------------

/**
 * @brief searches an entrytable for an given entry by it's name
 * 
 * @param entrytable the entrytable section to start searching from
 * @param name the name of the entry; must be smaller than 63 (max size of an longname)
 * @param entry_out pointer which will be set to the found entry on success
 * @param entrytable_out optional pointer which will be set to the entrytable section containing the free entry (only on success)
 * @param offset_out optional pointer which will be set to the offset of the entry into its entrytable section (only on success)
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_entrytab_findentry(
    libtabfs_entrytable_t* entrytable, char* name,
    libtabfs_entrytable_entry_t** entry_out,
    libtabfs_entrytable_t** entrytable_out,
    int* offset_out
);

/**
 * @brief searches after the target of an symlink
 * 
 * @param entrytable the entrytable section that contained the symlink
 * @param symlink_entry the entry of the symlink
 * @param userid the userid to perform the action as
 * @param groupid the groupid to perform the action as
 * @param entry_out pointer which will be set to the found entry on success
 * @param entrytable_out optional pointer which will be set to the entrytable section containing the free entry (only on success)
 * @param offset_out optional pointer which will be set to the offset of the entry into its entrytable section (only on success)
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_entrytab_getsymlinktarget(
    libtabfs_entrytable_t* entrytable, libtabfs_entrytable_entry_t* symlink_entry,
    unsigned int userid, unsigned int groupid,
    libtabfs_entrytable_entry_t** entry_out,
    libtabfs_entrytable_t** entrytable_out,
    int* offset_out
);

/**
 * @brief traverses an given path an obtains the entry its pointing to; follows symlink if specified
 * 
 * @param entrytable the entrytable section to start from
 * @param relative_path path to traverse; must be relative, so '/' at the beginning to use the root table instead dosnt work! supports './' as well as '../'
 * @param follow_symlink if set to true, this function will follow symlinks along the path; if set to false, it treats symlinks as files
 * @param userid the userid to perform the action as
 * @param groupid the groupid to perform the action as
 * @param entry_out pointer which will be set to the found entry on success
 * @param entrytable_out optional pointer which will be set to the entrytable section containing the free entry (only on success)
 * @param offset_out optional pointer which will be set to the offset of the entry into its entrytable section (only on success)
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_entrytab_traversetree(
    libtabfs_entrytable_t* entrytable, char* relative_path, bool follow_symlink,
    unsigned int userid, unsigned int groupid,
    libtabfs_entrytable_entry_t** entry_out,
    libtabfs_entrytable_t** entrytable_out,
    int* offset_out
);

//--------------------------------------------------------------------------------
// Entry creation
//--------------------------------------------------------------------------------

/**
 * @brief generic function to create entries in an entrytable section;
 * this is an internal function. you should never use it, and instead use the create functions that directly creates
 * the wanted entry!
 * 
 * @param entrytable the entrytable section to start searching for a free spot
 * @param name the name of the entry; if longer than 21, an longname entry is created an linked; must not be longer than 61!
 * @param entry_out pointer which will be set to the created entry on success
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_entry(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_entrytable_entry_t** entry_out
);

/**
 * @brief creates a new directory inside the given entrytable
 * 
 * @param entrytable the entrytable to create in
 * @param name name of the directory to create
 * @param fileflags the fileflags for the directory
 * @param create_ts creation timestamp
 * @param userid owning user
 * @param groupid owning group
 * @param entrytable_newdir_out pointer that will be set to the first entrytable section of the created directory
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_dir(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    libtabfs_entrytable_t** entrytable_newdir_out
);

/**
 * @brief creates a new device file to an character device
 * 
 * @param entrytable the entrytable to create in
 * @param name name of the directory to create
 * @param fileflags the fileflags for the directory
 * @param create_ts creation timestamp
 * @param userid owning user
 * @param groupid owning group
 * @param dev_id the device identifier
 * @param dev_flags the device flags for this devicefile
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_chardevice(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    unsigned int dev_id, unsigned int dev_flags
);

/**
 * @brief creates a new device file to an block device
 * 
 * @param entrytable the entrytable to create in
 * @param name name of the directory to create
 * @param fileflags the fileflags for the directory
 * @param create_ts creation timestamp
 * @param userid owning user
 * @param groupid owning group
 * @param dev_id the device identifier
 * @param dev_flags the device flags for this devicefile
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_blockdevice(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    unsigned int dev_id, unsigned int dev_flags
);

/**
 * @brief creates a fifo (first-in-first-out) buffer
 * 
 * @param entrytable the entrytable to create in
 * @param name name of the directory to create
 * @param fileflags the fileflags for the directory
 * @param create_ts creation timestamp
 * @param userid owning user
 * @param groupid owning group
 * @param bufferSize the size of the buffer
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_fifo(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    unsigned int bufferSize
);

/**
 * @brief creates an symlink
 * 
 * @param entrytable the entrytable to create in
 * @param name name of the directory to create
 * @param fileflags the fileflags for the directory
 * @param create_ts creation timestamp
 * @param userid owning user
 * @param groupid owning group
 * @param path the path to the target
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_symlink(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    char* path
);

/**
 * @brief creates an socket file; either an network (IPv4) or an general one (serial number)
 * 
 * @param entrytable the entrytable to create in
 * @param name name of the directory to create
 * @param fileflags the fileflags for the directory
 * @param create_ts creation timestamp
 * @param userid owning user
 * @param groupid owning group
 * @param ipv4_or_serialnum an IPv4 address in binary form or an serial number
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_socket(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    unsigned int ipv4_or_serialnum
);

/**
 * @brief creates an symlink
 * 
 * @param entrytable the entrytable to create in
 * @param name name of the directory to create
 * @param fileflags the fileflags for the directory
 * @param create_ts creation timestamp
 * @param userid owning user
 * @param groupid owning group
 * @param iskernel true if the continuous file is an kernel (type will then set to kernel (0xF))
 * @param entry_out pointer which will be set to the new entry on success
 * @return LIBTABFS_ERR_NONE if the operation was successfull; other errorcode otherwise
 */
libtabfs_error libtabfs_create_continuousfile(
    libtabfs_entrytable_t* entrytable, char* name, libtabfs_fileflags_t fileflags,
    libtabfs_time_t create_ts, unsigned int userid, unsigned int groupid,
    bool iskernel, libtabfs_entrytable_entry_t** entry_out
);

#endif // __LIBTABFS_ENTRYTABLE_H__