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

void cp_dir_without_dots(char* src, char* dest)
{
    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) {
        printf(2, "cp: cannot open %s\n", src);
        exit();
    }

    struct dirent de;
    while (read(fd_src, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
            continue;
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0 || strcmp(de.name, "image") == 0 || strcmp(de.name, "containers") == 0 || strcmp(de.name, "README") == 0 || strcmp(de.name, "console") == 0 || strcmp(de.name, "proc") == 0) {
            continue;
        }
        if (fork() == 0) {
            char srcfile[100];
            strcpy(&srcfile[0], src);
            srcfile[strlen(src)] = '/';
            strcpy(&srcfile[strlen(src) + 1], de.name);
            srcfile[strlen(src) + 1 + strlen(de.name)] = '\0';

            char destfile[100];
            strcpy(&destfile[0], dest);
            destfile[strlen(dest)] = '/';
            strcpy(&destfile[strlen(dest) + 1], de.name);
            destfile[strlen(dest) + 1 + strlen(de.name)] = '\0';

            printf(1, "%s --> %s\n", srcfile, destfile);

            char* argv[] = { "/cp", srcfile, destfile, NULL };
            exec(argv[0], argv);
        }
        while (wait() > 0)
            ;
    }
    close(fd_src);
}

void help()
{
    printf(1, "Usage: \n");
    printf(1, "conductor help: Display this help message\n");
    printf(1, "conductor init: Initialize the image and containers directories\n");
    printf(1, "conductor run <command> [args]: Run a command in a new container\n");
    printf(1, "conductor exec <pid> <command> [args]: Execute a command in an existing container with the given init PID\n");
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
        char container_path[50] = "/containers/1";

        // int_to_string(child_pid, &container_path[strlen(container_path)]);
        mkdir(container_path);

        cp_dir_without_dots("/image/sample", container_path);

        unshare(NS_NET | NS_PID);

        int child_pid = fork();
        if (child_pid == 0) {
            read(p[0], &container_path[12], 30);
            chroot(container_path);
            chdir("/");
            mount_procfs("/");
            char** args = &argv[2];
            exec(args[0], args);
        }

        write(p[1], &container_path[12], 30);

        wait();
    } else if (strcmp(cmd, "exec") == 0) {
        int target_pid = atoi(argv[2]);
        if (setns(target_pid, NS_NET | NS_PID) < 0) {
            printf(2, "Container not running\n");
            exit();
        }

        char container_path[50] = "/containers/1";
        // int_to_string(target_pid, &container_path[12]);

        int child_pid = fork();
        if (child_pid == 0) {
            chroot(container_path);
            chdir("/");
            char** args = &argv[3];
            exec(args[0], args);
        }

        wait();
    } else
        help();
    exit();
}
