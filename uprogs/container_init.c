// init: The initial user-level program

#include "fcntl.h"
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

void setup_if(char* interface, char* ipaddr, char* nmask)
{
    char* argv1[] = { "ifconfig", interface, ipaddr, "netmask", nmask, NULL };
    if (fork() == 0)
        exec(argv1[0], argv1);
    wait();

    char* argv2[] = { "ifconfig", interface, "up", NULL };
    if (fork() == 0)
        exec(argv2[0], argv2);
    wait();
}

int main(void)
{
    setup_std();
    close(0);

    setup_if("lo", "127.0.0.1", "255.0.0.0");

    for (;;)
        while (wait() >= 0)
            ;
}
