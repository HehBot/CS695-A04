#include "fcntl.h"
#include "fs.h"
#include "stat.h"
#include "types.h"
#include "user.h"

void int_to_string(int num, char* str)
{
    char* p = str;
    if (num == 0) {
        *p++ = '0';
    } else {
        int num_copy = num;
        while (num_copy != 0) {
            num_copy /= 10;
            p++;
        }
        *p = '\0';
        while (num != 0) {
            *--p = '0' + num % 10;
            num /= 10;
        }
    }
}

void help()
{
    printf(1, "Usage: \n");
    printf(1, "conductor help                               Display this help message\n");
    printf(1, "conductor init                               Initialize the image and containers directories\n");
    printf(1, "conductor run <command> [args...]            Run a command in a new container\n");
    printf(1, "conductor exec <pid> <command> [args...]     Execute a command in an existing container with the given init PID\n");
    printf(1, "conductor stop <pid>                         Stop a running container\n");
}

int main(int argc, char* argv[])
{
    if (argc <= 1) {
        help();
        exit();
    }

    char* cmd = argv[1];
    if (strcmp(cmd, "help") == 0) {
        help();
    } else if (strcmp(cmd, "init") == 0)
        mkdir("/containers");
    else if (strcmp(cmd, "run") == 0) {
        int p[2];
        pipe(p);
        char container_path[50] = "/containers/";

        if (fork() == 0) {
            char* argv[] = { "/cp", "/image/sample", "/image/.temp", NULL };
            exec(argv[0], argv);
        }
        wait();

        unshare(NS_NET | NS_PID);

        int child_pid = fork();
        if (child_pid == 0) {
            read(p[0], &container_path[12], 30);
            chroot(container_path);
            chdir("/");
            // mount_procfs("/");
            char** args = &argv[2];
            exec(args[0], args);
        }

        int_to_string(child_pid, &container_path[strlen(container_path)]);
        rename("/image/.temp", container_path);

        write(p[1], &container_path[12], 30);

        wait();

        char* argv2[] = { "/rm", "-r", container_path, NULL };
        exec(argv2[0], argv2);

    } else if (strcmp(cmd, "exec") == 0) {
        int target_pid = atoi(argv[2]);
        if (setns(target_pid, NS_NET | NS_PID) < 0) {
            printf(2, "Container not running\n");
            exit();
        }

        char container_path[50] = "/containers/";
        int_to_string(target_pid, &container_path[12]);

        int child_pid = fork();
        if (child_pid == 0) {
            chroot(container_path);
            chdir("/");
            char** args = &argv[3];
            exec(args[0], args);
        }

        wait();
    } else if (strcmp(cmd, "stop") == 0) {
        int target_pid = atoi(argv[2]);
        kill(target_pid);
    } else
        help();
    exit();
}
