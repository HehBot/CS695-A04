// Test that checks behaviour of fork and wait across pid namespaces

#include "stat.h"
#include "types.h"
#include "user.h"

#define N 1000

void waitall(void)
{
    int p;
    while ((p = wait()) > 0)
        printf(1, "<%d>%d waited on %d\n", getgpid(), getpid(), p);
}

void test(int shouldwait)
{
    printf(1, "(0<%d>%d)\n", getgpid(), getpid());
    unshare(1);

    if (fork() == 0) {
        printf(1, "(1<%d>%d)\n", getgpid(), getpid());
        unshare(1);

        if (fork() == 0)
            printf(1, "(2<%d>%d)\n", getgpid(), getpid());
    } else {
        if (fork() == 0)
            printf(1, "(3<%d>%d)\n", getgpid(), getpid());
    }

    if (shouldwait)
        waitall();
}

int main(void)
{
    printf(1, "\ntest with wait\n");
    if (fork() == 0) {
        test(1);
        exit();
    }
    waitall();

    sleep(500);

    printf(1, "\ntest without wait\n");
    if (fork() == 0) {
        test(0);
        exit();
    }
    waitall();

    exit();
}
