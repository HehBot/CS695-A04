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

void setup_net(char* interface, char* ipaddr)
{
    char* argv1[] = { "ifconfig", interface, ipaddr, "netmask", "255.255.255.0", NULL };
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
    mount_procfs("/");
    setup_std();
    setup_net("net1", "172.16.100.2");

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
            ;
    }
}
