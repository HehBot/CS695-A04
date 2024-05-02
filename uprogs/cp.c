#include "fcntl.h"
#include "fs.h"
#include "stat.h"
#include "types.h"
#include "user.h"

char const* basename(char const* path)
{
    char const* last = path;
    while (*path != '\0') {
        if (*path == '/')
            last = path + 1;
        path++;
    }
    return last;
}

int cp_file(int fd_src, int fd_dest)
{
    char buf[512];
    int n;
    while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
        if (write(fd_dest, buf, n) != n) {
            printf(2, "cp: write error");
            return -1;
        }
    }
    close(fd_src);
    close(fd_dest);
    if (n < 0) {
        printf(2, "cp: read error");
        return -1;
    }
    return 0;
}

int cp(char const* src, char const* dest)
{
    int srcfd = open(src, O_RDONLY);
    if (srcfd < 0)
        return -1;
    struct stat srcstat;
    fstat(srcfd, &srcstat);
    close(srcfd);

    int new;
    int destfd = open(dest, O_RDONLY);
    struct stat deststat;
    if (destfd < 0)
        new = 1;
    else {
        new = 0;
        fstat(destfd, &deststat);
        close(destfd);
    }

    if (new) {
        switch (srcstat.type) {
        case T_FILE: {
            srcfd = open(src, O_RDONLY);
            destfd = open(dest, O_WRONLY | O_CREATE);
            return cp_file(srcfd, destfd);
        }
        case T_DIR: {
            char newdest[128];
            strcpy(newdest, dest);
            int ld = strlen(dest);
            newdest[ld] = '/';

            char newsrc[128];
            strcpy(newsrc, src);
            int ls = strlen(src);
            newsrc[ls] = '/';

            mkdir(dest);
            srcfd = open(src, O_RDONLY);
            struct dirent de;
            while (read(srcfd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0)
                    continue;
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
                strcpy(&newdest[ld + 1], de.name);
                strcpy(&newsrc[ls + 1], de.name);
                cp(newsrc, newdest);
            }
            close(srcfd);
            return 0;
        }
        default:
            printf(2, "Unknown type\n");
            return -1;
        }
    } else if (deststat.type == T_DIR) {
        char newdest[128];
        strcpy(newdest, dest);
        int l = strlen(dest);
        newdest[l] = '/';
        strcpy(&newdest[l + 1], basename(src));
        return cp(src, newdest);
    } else {
        printf(2, "Cannot overwrite non-directory %s with directory %s\n", dest, src);
        return -1;
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        exit();
    }
    cp(argv[1], argv[2]);
    exit();
}
