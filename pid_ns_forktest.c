// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "types.h"
#include "stat.h"
#include "user.h"

#define N  1000

void
forktest(void)
{
    if (unshare(1) == -1)
        exit();
    int p = fork();
    if (p == 0) {
        int child_pid = getpid();
        printf(1, "hello from child, I see pid %d\n", child_pid);
        sleep(10);
    } else {
        printf(1, "hello from parent, child has pid %d\n", p);
    }
}

int
main(void)
{
  forktest();
  exit();
}
