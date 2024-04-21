#include "stat.h"
#include "types.h"
#include "user.h"

int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf(2, "Usage: chroot <newroot> <cmd> [...args]\n");
        exit();
    }
    unshare(1);
    if (fork() == 0) {
        if (chroot(argv[1]) < 0) {
            printf(2, "chroot: chroot to %s failed\n", argv[1]);
            exit();
        }
        chdir("/");
        mount_procfs("/");
        if (exec(argv[2], &argv[2]) < 0) {
            printf(2, "chroot: exec failed\n");
            exit();
        }
    }
    exit();
}
