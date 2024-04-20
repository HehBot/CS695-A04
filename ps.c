#include "fcntl.h"
#include "fs.h"
#include "stat.h"
#include "types.h"
#include "user.h"

void ps()
{
    char buf[512], *p, *store;
    int fd, fd2;
    struct dirent de;
    struct stat st;

    if ((fd = open("/proc", O_RDONLY)) < 0) {
        printf(2, "ps: cannot open /proc\n");
        return;
    }

    if (fstat(fd, &st) < 0) {
        printf(2, "ps: cannot stat /proc\n");
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE:
        printf(2, "ps: procfs not mounted\n");
        break;

    case T_DIR:
        printf(1, "PID\t CMD\n");
        strcpy(buf, "/proc");
        p = buf + strlen(buf);
        *p++ = '/';
        store = p;
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            p = store;
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p += strlen(de.name);
            memmove(p, "/cmd", 4);
            p += 4;
            *p++ = '\0';
            if ((fd2 = open(buf, O_RDONLY)) < 0) {
                printf(2, "ps: cannot open %s\n", buf);
                return;
            }
            char cmd[16];
            read(fd2, cmd, sizeof(cmd));
            
            printf(1, "%s\t %s\n", de.name, cmd);
            close(fd2);
        }
        break;
    }
    close(fd);
}

int main(int argc, char* argv[])
{
    ps();
    exit();
}
