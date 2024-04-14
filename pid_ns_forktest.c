// Test that checks behaviour of fork and wait across pid namespaces

#include "stat.h"
#include "types.h"
#include "user.h"

#define N 1000

void test(void)
{
    printf(1, "(0<%d>%d)\n", getgpid(), getpid());
    sleep(100);
    unshare(1);

    if (fork() == 0) {
        printf(1, "(1<%d>%d)\n", getgpid(), getpid());
        sleep(100);
        unshare(1);

        if (fork() == 0) {
            printf(1, "(2<%d>%d)\n", getgpid(), getpid());
            sleep(100);
        }
    } else {
        if (fork() == 0) {
            printf(1, "(3<%d>%d)\n", getgpid(), getpid());
            sleep(100);
        }
    }
}

int main(void)
{
    printf(1, "\n\n\ntest with wait\n\n");
    if (fork() == 0) {
        test();
        while (wait() >= 0)
            ;
        exit();
    }
    wait();

    sleep(500);

    printf(1, "\n\n\ntest without wait\n\n");
    if (fork() == 0) {
        test();
        exit();
    }
    wait();

    exit();
}
