#ifndef FILE_H
#define FILE_H

#include "fs.h"
#include "sleeplock.h"
#include "types.h"

struct file {
    enum { FD_NONE,
           FD_PIPE,
           FD_INODE,
           FD_SOCKET } type;
    int ref; // reference count
    char readable;
    char writable;
    struct pipe* pipe;
    struct inode* ip;
    struct socket* socket;
    uint off;
};

struct inode_functions {  
  void (*ipopulate)(struct inode*);                   // fill in inode details
  void (*iupdate)(struct inode*);                     // write inode details back to disk
  int (*readi)(struct inode*, char*, uint, uint);     // read from file contents
  int (*writei)(struct inode*, char*, uint, uint);    // write to file contents
}; 

// in-memory copy of an inode
struct inode {
    uint dev; // Device number
    uint inum; // Inode number
    int ref; // Reference count
    struct sleeplock lock; // protects everything below here
    int valid; // inode has been read from disk?

    struct inode_functions *i_func;

    short type; // copy of disk inode
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

// table mapping major device number to
// device functions
struct devsw {
    int (*read)(struct inode*, char*, int);
    int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1

#endif // FILE_H
