#include "fcntl.h"
#include "fs.h"
#include "stat.h"
#include "types.h"
#include "user.h"

void rm(char* path, bool recursive){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, O_RDONLY)) < 0) {
        printf(2, "rm: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        printf(2, "rm: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if(st.type == T_DIR && recursive){
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf(1, "rm: path too long\n");
            return;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            rm(buf, 1);
        }
    }

    close(fd);
    if (unlink(path) < 0) {
        printf(2, "rm: %s failed to delete\n", path);
    }
}

int main(int argc, char* argv[])
{
    int i = 1;

    if (argc < 2) {
        printf(2, "Usage: rm files...\n");
        exit();
    }

    bool recursive = (strcmp(argv[1],"-r") == 0);
    if(recursive) i++;

    for (; i < argc; i++) {
        rm(argv[i], recursive);
    }

    exit();
}
