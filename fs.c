// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "buf.h"
#include "defs.h"
#include "file.h"
#include "fs.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb;

// fs inode functions
void fs_ipopulate(struct inode* ip);
void fs_iupdate(struct inode* ip);
static int fs_readi(struct inode* ip, char* dst, uint off, uint n);
static int fs_writei(struct inode* ip, char* src, uint off, uint n);
struct inode_functions fs_i_func = { fs_ipopulate, fs_iupdate, fs_readi, fs_writei };   

// Procfs inode functions
void procfs_ipopulate(struct inode* ip);
void procfs_iupdate(struct inode* ip);
static int procfs_readi(struct inode* ip, char* dst, uint off, uint n);
static int procfs_writei(struct inode* ip, char* src, uint off, uint n);
struct inode_functions procfs_i_func = { procfs_ipopulate, procfs_iupdate, procfs_readi, procfs_writei }; 

// Placeholder funcitons
static int procfs_writei(struct inode* a, char* b, uint c, uint d){
    return 0;
}
void procfs_iupdate(struct inode* a){}

extern struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

int namespace_depth(pid_ns_t* ancestor, pid_ns_t* curr);

int proc_mountpt_inodes[MAXPROCFSMOUNT];

void add_procfs_mount(int inum)
{
    for (int i = 0; i < MAXPROCFSMOUNT; i++) {
        if (proc_mountpt_inodes[i] == 0) {
            proc_mountpt_inodes[i] = inum;
            return;
        }
    }
    panic("mount: no more mount points available");
}

void remove_procfs_mount(int inum)
{
    for (int i = 0; i < MAXPROCFSMOUNT; i++) {
        if (proc_mountpt_inodes[i] == inum) {
            proc_mountpt_inodes[i] = 0;
            return;
        }
    }
}

extern int root_proc_inum;
int root_proc_blocks[10] = { 0 };

// Used to allocate unique inums for procfs
int proc_inode_counter = 0;
// Used to allocate unique block num for procfs
int proc_block_counter = 0;

// Read the super block.
void readsb(int dev, struct superblock* sb)
{
    struct buf* bp;

    bp = bread(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
    struct buf* bp;

    bp = bread(dev, bno);
    memset(bp->data, 0, BSIZE);
    log_write(bp);
    brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
    int b, bi, m;
    struct buf* bp;

    bp = 0;
    for (b = 0; b < sb.size; b += BPB) {
        bp = bread(dev, BBLOCK(b, sb));
        for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
            m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0) { // Is block free?
                bp->data[bi / 8] |= m; // Mark block in use.
                log_write(bp);
                brelse(bp);
                bzero(dev, b + bi);
                return b + bi;
            }
        }
        brelse(bp);
    }
    panic("balloc: out of blocks");
}

// Free a disk block.
static void
bfree(int dev, uint b)
{
    struct buf* bp;
    int bi, m;

    bp = bread(dev, BBLOCK(b, sb));
    bi = b % BPB;
    m = 1 << (bi % 8);
    if ((bp->data[bi / 8] & m) == 0)
        panic("freeing free block");
    bp->data[bi / 8] &= ~m;
    log_write(bp);
    brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
    struct spinlock lock;
    struct inode inode[NINODE];
} icache;

void iinit(int dev)
{
    int i = 0;

    initlock(&icache.lock, "icache");
    for (i = 0; i < NINODE; i++) {
        initsleeplock(&icache.inode[i].lock, "inode");
    }

    readsb(dev, &sb);
    cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
 inodestart %d bmap start %d\n",
            sb.size, sb.nblocks,
            sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
            sb.bmapstart);
}

static struct inode* iget(uint dev, uint inum, struct inode* parent);

// PAGEBREAK!
//  Allocate an inode on device dev.
//  Mark it as allocated by  giving it type type.
//  Returns an unlocked but allocated and referenced inode.
struct inode*
ialloc(uint dev, short type, struct inode* parent)
{
    int inum;
    struct buf* bp;
    struct dinode* dip;

    if (dev == PROCDEV) {
        struct inode* ip = iget(dev, ++proc_inode_counter, parent);
        ip->type = type;
        return ip;
    }

    for (inum = 1; inum < sb.ninodes; inum++) {
        bp = bread(dev, IBLOCK(inum, sb));
        dip = (struct dinode*)bp->data + inum % IPB;
        if (dip->type == 0) { // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            log_write(bp); // mark it allocated on the disk
            brelse(bp);
            return iget(dev, inum, parent);
        }
        brelse(bp);
    }
    panic("ialloc: no inodes");
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void fs_iupdate(struct inode* ip)
{
    struct buf* bp;
    struct dinode* dip;

    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum % IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    log_write(bp);
    brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum, struct inode* parent)
{
    struct inode *ip, *empty;

    acquire(&icache.lock);

    // Is the inode already cached?
    empty = 0;
    for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
            ip->ref++;
            release(&icache.lock);
            return ip;
        }
        if (empty == 0 && ip->ref == 0) // Remember empty slot.
            empty = ip;
    }

    // Recycle an inode cache entry.
    if (empty == 0)
        panic("iget: no inodes");

    ip = empty;
    ip->dev = dev;
    ip->inum = inum;

    if(ip->dev == PROCDEV){
        ip->i_func = &procfs_i_func;
    }
    else if(parent)
        ip->i_func = parent->i_func;
    else
        ip->i_func = &fs_i_func; // fs is the default file system

    ip->ref = 1;
    ip->valid = 0;
    release(&icache.lock);

    return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode* ip)
{
    acquire(&icache.lock);
    ip->ref++;
    release(&icache.lock);
    return ip;
}

void fs_ipopulate(struct inode* ip)
{
    struct buf* bp;
    struct dinode* dip;

    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum % IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if (ip->type == 0)
        panic("ilock: no type");
}
void procfs_ipopulate(struct inode* ip)
{
    struct proc* p;
    struct proc* curproc = myproc();
    struct buf* bp;
    uint off = 0;
    ip->size = 0;

    if (ip->inum == root_proc_inum) {
        // Assuming always direct entries are sufficient
        int cur_block = 0;
        if (root_proc_blocks[cur_block] == 0) {
            root_proc_blocks[cur_block] = ++proc_block_counter;
        }
        ip->addrs[cur_block] = root_proc_blocks[cur_block];
        bp = bget(PROCDEV, root_proc_blocks[cur_block]);
        bp->flags &= B_DIRTY;

        acquire(&ptable.lock);
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != UNUSED) {
                int depth = namespace_depth(curproc->pid_ns, p->pid_ns);
                if (depth < 0)
                    continue;
                struct dirent de;
                char name[DIRSIZ];
                snprintf(name, 20, "%d", p->pid[depth]);
                strncpy(de.name, name, DIRSIZ);
                de.inum = p->procfs_nums[0];
                if (off + sizeof(de) >= BSIZE) {
                    ip->size += off;
                    cur_block++;
                    if (root_proc_blocks[cur_block] == 0) {
                        root_proc_blocks[cur_block] = ++proc_block_counter;
                    }
                    ip->addrs[cur_block] = root_proc_blocks[cur_block];
                    bp = bget(PROCDEV, root_proc_blocks[cur_block]);
                    bp->flags &= B_DIRTY;
                    off = 0;
                }
                memmove(bp->data + off, &de, sizeof(de));
                off += sizeof(de);
            }
        }
        release(&ptable.lock);
        brelse(bp);
        ip->size += off;
        ip->type = T_DIR;
    } else {
        acquire(&ptable.lock);
        bp = bget(PROCDEV, ip->addrs[0]);
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != UNUSED) {
                if (ip->inum == p->procfs_nums[0]) {
                    ip->addrs[0] = p->procfs_nums[0];
                    struct dirent de;
                    char name[DIRSIZ];

                    snprintf(name, 20, "cmd");
                    strncpy(de.name, name, DIRSIZ);
                    de.inum = p->procfs_nums[1];
                    memmove(bp->data + off, &de, sizeof(de));
                    off += sizeof(de);

                    ip->type = T_DIR;
                    break;
                } else if (ip->inum == p->procfs_nums[1]) {
                    memmove(bp->data + off, p->name, strlen(p->name)+1);
                    off += strlen(p->name)+1;

                    ip->type = T_FILE;
                    break;
                }
            }
        }
        ip->size += off;
        release(&ptable.lock);
        brelse(bp);
    }
}
static void ipopulate(struct inode* ip)
{
    ip->i_func->ipopulate(ip);
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void ilock(struct inode* ip)
{
    if (ip == 0 || ip->ref < 1)
        panic("ilock");

    acquiresleep(&ip->lock);

    if (ip->valid == 0)
        ipopulate(ip);
}

// Unlock the given inode.
void iunlock(struct inode* ip)
{
    if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("iunlock");
    if (ip->dev == PROCDEV) {
        struct buf* bp;
        for (int i = 0; ip->addrs[i] > 0; i++) {
            bp = bget(ip->dev, ip->addrs[i]);
            bp->flags &= (~B_DIRTY);
            brelse(bp);
        }
    }
    releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void iput(struct inode* ip)
{
    acquiresleep(&ip->lock);
    if (ip->valid && ip->nlink == 0) {
        acquire(&icache.lock);
        int r = ip->ref;
        release(&icache.lock);
        if (r == 1) {
            // inode has no links and no other references: truncate and free.
            itrunc(ip);
            ip->type = 0;
            iupdate(ip);
            ip->valid = 0;
        }
    }
    releasesleep(&ip->lock);

    acquire(&icache.lock);
    ip->ref--;
    release(&icache.lock);
}

// Common idiom: unlock, then put.
void iunlockput(struct inode* ip)
{
    iunlock(ip);
    iput(ip);
}

// PAGEBREAK!
//  Inode content
//
//  The content (data) associated with each inode is stored
//  in blocks on the disk. The first NDIRECT block numbers
//  are listed in ip->addrs[].  The next NINDIRECT blocks are
//  listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode* ip, uint bn)
{
    uint addr, *a;
    struct buf* bp;

    if (bn < NDIRECT) {
        if ((addr = ip->addrs[bn]) == 0)
            ip->addrs[bn] = addr = balloc(ip->dev);
        return addr;
    }
    bn -= NDIRECT;

    if (bn < NINDIRECT) {
        // Load indirect block, allocating if necessary.
        if ((addr = ip->addrs[NDIRECT]) == 0)
            ip->addrs[NDIRECT] = addr = balloc(ip->dev);
        bp = bread(ip->dev, addr);
        a = (uint*)bp->data;
        if ((addr = a[bn]) == 0) {
            a[bn] = addr = balloc(ip->dev);
            log_write(bp);
        }
        brelse(bp);
        return addr;
    }

    panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void
itrunc(struct inode* ip)
{
    int i, j;
    struct buf* bp;
    uint* a;

    for (i = 0; i < NDIRECT; i++) {
        if (ip->addrs[i]) {
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT]) {
        bp = bread(ip->dev, ip->addrs[NDIRECT]);
        a = (uint*)bp->data;
        for (j = 0; j < NINDIRECT; j++) {
            if (a[j])
                bfree(ip->dev, a[j]);
        }
        brelse(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    ip->size = 0;
    iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void stati(struct inode* ip, struct stat* st)
{
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->nlink = ip->nlink;
    st->size = ip->size;
}

// PAGEBREAK!
//  Read data from inode.
//  Caller must hold ip->lock.
static int fs_readi(struct inode* ip, char* dst, uint off, uint n)
{
    uint tot, m;
    struct buf* bp;

    if (ip->type == T_DEV) {
        if (ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
            return -1;
        return devsw[ip->major].read(ip, dst, n);
    }

    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > ip->size)
        n = ip->size - off;

    for (tot = 0; tot < n; tot += m, off += m, dst += m) {
        bp = bread(ip->dev, bmap(ip, off / BSIZE));
        m = min(n - tot, BSIZE - off % BSIZE);
        memmove(dst, bp->data + off % BSIZE, m);
        brelse(bp);
    }
    return n;
}

static int procfs_readi(struct inode* ip, char* dst, uint off, uint n)
{
    uint tot, m;
    struct buf* bp;

    ipopulate(ip);
    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > ip->size)
        n = ip->size - off;

    for (tot = 0; tot < n; tot += m, off += m, dst += m) {
        bp = bget(ip->dev, ip->addrs[off / BSIZE]);
        m = min(n - tot, BSIZE - off % BSIZE);
        memmove(dst, bp->data + off % BSIZE, m);
        brelse(bp);
    }
    return n;
}

int readi(struct inode* ip, char* dst, uint off, uint n)
{
    return ip->i_func->readi(ip, dst, off, n);
}

// PAGEBREAK!
// Write data to inode.
// Caller must hold ip->lock.
static int fs_writei(struct inode* ip, char* src, uint off, uint n)
{
    uint tot, m;
    struct buf* bp;

    if (ip->type == T_DEV) {
        if (ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
            return -1;
        return devsw[ip->major].write(ip, src, n);
    }

    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > MAXFILE * BSIZE)
        return -1;

    for (tot = 0; tot < n; tot += m, off += m, src += m) {
        bp = bread(ip->dev, bmap(ip, off / BSIZE));
        m = min(n - tot, BSIZE - off % BSIZE);
        memmove(bp->data + off % BSIZE, src, m);
        log_write(bp);
        brelse(bp);
    }

    if (n > 0 && off > ip->size) {
        ip->size = off;
        iupdate(ip);
    }
    return n;
}
int writei(struct inode* ip, char* src, uint off, uint n)
{
    return ip->i_func->writei(ip, src, off, n);
}
void iupdate(struct inode* ip)
{
    ip->i_func->iupdate(ip);
}

// PAGEBREAK!
//  Directories

int namecmp(const char* s, const char* t)
{
    return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode* dp, char* name, uint* poff)
{
    uint off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
        panic("dirlookup not DIR");

    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup read");
        if (de.inum == 0)
            continue;
        if (namecmp(name, de.name) == 0) {
            // entry matches path element
            if (poff)
                *poff = off;
            inum = de.inum;
            for (int i = 0; i < MAXPROCFSMOUNT; i++) {
                if (dp->inum == proc_mountpt_inodes[i] && namecmp(name, "proc") == 0) {
                    return iget(PROCDEV, inum, dp);
                }
            }
            return iget(dp->dev, inum, dp);
        }
    }

    return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int dirlink(struct inode* dp, char* name, uint inum)
{
    int off;
    struct dirent de;
    struct inode* ip;

    // Check that name is not present.
    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iput(ip);
        return -1;
    }

    // Look for an empty dirent.
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlink read");
        if (de.inum == 0)
            break;
    }

    strncpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
        panic("dirlink");

    return 0;
}

// PAGEBREAK!
//  Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char* path, char* name)
{
    char* s;
    int len;

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char* path, int nameiparent, char* name)
{
    struct inode *ip, *next;

    if (*path == '/') {
        static int first_root_req = 1;
        if (first_root_req) {
            ip = iget(ROOTDEV, ROOTINO, 0);
            first_root_req = 0;
        } else
            ip = idup(myproc()->root);
    } else
        ip = idup(myproc()->cwd);

    while ((path = skipelem(path, name)) != 0) {
        ilock(ip);
        if (ip->type != T_DIR) {
            iunlockput(ip);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // Stop one level early.
            iunlock(ip);
            return ip;
        }
        if (ip == myproc()->root && namecmp(name, "..") == 0) {
            // prevents climbing up chroot
            iunlock(ip);
            continue;
        }
        if ((next = dirlookup(ip, name, 0)) == 0) {
            iunlockput(ip);
            return 0;
        }
        iunlockput(ip);
        ip = next;
    }
    if (nameiparent) {
        iput(ip);
        return 0;
    }
    return ip;
}

struct inode*
namei(char* path)
{
    char name[DIRSIZ];
    return namex(path, 0, name);
}

struct inode*
nameiparent(char* path, char* name)
{
    return namex(path, 1, name);
}
