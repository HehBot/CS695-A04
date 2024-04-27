#include "user.h"

int main(void)
{
    char* ps_bf[] = { "ps" };
    unshare(1);
    if (fork() == 0) {
        printf(1, "child:\n");
        if (fork() == 0) {
            sleep(100);
            exec(ps_bf[0], &ps_bf[0]);
        }
        wait();
        sleep(100);
    } else {
        printf(1, "parent:\n");
        exec(ps_bf[0], &ps_bf[0]);
    }
    exit();
}
