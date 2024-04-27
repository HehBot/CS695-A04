#include "fcntl.h"
#include "stat.h"
#include "types.h"
#include "user.h"

void cp(int fd_src, int fd_dest)
{
    char buf[512];
    int n;
    while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
        if (write(fd_dest, buf, n) != n) {
            printf(2, "cp: write error");
            exit();
        }
    }
    if (n < 0) {
        printf(2, "cp: read error");
        exit();
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf(2, "Usage: cp <src> <dest>\n");
        exit();
    }

    int fd_src = open(argv[1], O_RDONLY);
    if (fd_src < 0) {
        printf(2, "cp: cannot open %s\n", argv[1]);
        exit();
    }
    int fd_dest = open(argv[2], O_WRONLY | O_CREATE);
    if (fd_dest < 0) {
        printf(2, "cp: cannot open %s for writing\n", argv[2]);
        exit();
    }

    cp(fd_src, fd_dest);
    close(fd_src);
    close(fd_dest);

    exit();
}
