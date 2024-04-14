// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "stat.h"
#include "types.h"
#include "user.h"

#define N 1000

void forktest(void)
{
    if (unshare(1) == -1)
        exit();
    int p = fork();
    if (p == 0) {
        printf(1, "[1](%d)\n", getpid());
        if (unshare(1) == -1)
            exit();
        int q = fork();
        if (q == 0) {
            printf(1, "[2](%d)\n", getpid());
        } else
            printf(1, "[3](%d)\n", q);
    } else {
        printf(1, "[4](%d)\n", p);
        int r = fork();
        if (r == 0) {
            printf(1, "[5](%d)\n", getpid());
        } else
            printf(1, "[6](%d)\n", r);
    }
}

int main(void)
{
    printf(1, "[0](%d)\n", getpid());
    forktest();
    sleep(10000);
    exit();
}
