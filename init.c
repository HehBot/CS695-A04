// init: The initial user-level program

#include "fcntl.h"
#include "stat.h"
#include "types.h"
#include "user.h"

void setup_std(void)
{
    int fd_stdin, fd_stdout, fd_stderr;

    if ((fd_stdin = open("/console", O_RDONLY)) < 0) {
        mknod("console", 1, 1);
        fd_stdin = open("/console", O_RDONLY);
    }
    fd_stdout = open("/console", O_WRONLY);
    fd_stderr = open("/console", O_WRONLY);

    if (fd_stdin != 0) {
        dup2(fd_stdin, 0);
        close(fd_stdin);
    }
    if (fd_stdout != 1) {
        dup2(fd_stdout, 1);
        close(fd_stdout);
    }
    if (fd_stderr != 2) {
        dup2(fd_stderr, 2);
        close(fd_stderr);
    }
}

int main(void)
{
    setup_std();

    int pid, wpid;
    for (;;) {
        printf(1, "init: starting sh\n");
        pid = fork();
        if (pid < 0) {
            printf(1, "init: fork failed\n");
            exit();
        }
        if (pid == 0) {
            char* argv[] = { "sh", NULL };
            exec("sh", argv);
            printf(1, "init: exec sh failed\n");
            exit();
        }
        while ((wpid = wait()) >= 0 && wpid != pid)
            printf(1, "zombie<%d>\n", wpid);
    }
}
