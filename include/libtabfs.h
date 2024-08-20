#ifndef __LIBTABFS_H__
#define __LIBTABFS_H__

#include "./common.h"
#include "./linkedlist.h"
#include "./volume.h"
#include "./bat.h"
#include "./entrytable.h"
#include "./fatfile.h"

#define LIBTABFS_VERSION "v0.3"
#define LIBTABFS_VERSION_MAJOR 0
#define LIBTABFS_VERSION_MINOR 3

/**
 * @brief Returns the version of libtabfs provided by the library
 * 
 * @param[out] major major
 * @param[out] minor minor
 * @param[out] patch patchlevel
 * @return string representation of the version
 */
const char* libtabfs_getVersion(int* major, int* minor);

#endif