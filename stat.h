#ifndef STAT_H
#define STAT_H

#include "types.h"

#define T_DIR 1 // Directory
#define T_FILE 2 // File
#define T_DEV 3 // Device

#ifndef BUILD_MKFS
struct stat {
    short type; // Type of file
    int dev; // File system's disk device
    uint ino; // Inode number
    short nlink; // Number of links to file
    uint size; // Size of file in bytes
};
#endif

#endif // STAT_H
