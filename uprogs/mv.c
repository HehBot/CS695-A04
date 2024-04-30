#include "fcntl.h"
#include "stat.h"
#include "user.h"

char const* basename(char const* path)
{
    char const* last = path;
    while (*path != '\0') {
        if (*path == '/')
            last = path + 1;
        path++;
    }
    return last;
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf(2, "Usage: %s <old path> <new path>\n", argv[0]);
        exit();
    }
    int new = open(argv[2], O_RDONLY);
    if (new == -1) {
        if (rename(argv[1], argv[2]) < 0)
            printf(2, "mv: failed\n");
    } else {
        struct stat st;
        fstat(new, &st);
        close(new);
        if (st.type != T_DIR) {
            printf(2, "mv: unable to move to %s: file exists", argv[2]);
            exit();
        }
        char newname[128];
        int l = strlen(argv[2]);
        char const* base = basename(argv[1]);
        if (l + 1 + strlen(base) + 1 > 128) {
            printf(2, "mv: destination path too long\n");
            exit();
        }
        strcpy(&newname[0], argv[2]);
        newname[l] = '/';
        strcpy(&newname[l + 1], base);
        if (rename(argv[1], newname) < 0)
            printf(2, "mv: failed\n");
    }
    exit();
}
