#include "types.h"
#include "user.h"

int main()
{
#define MAXPATHSZ 512
    char buf[MAXPATHSZ];
    getcwd(buf, MAXPATHSZ);
    printf(1, "%s\n", buf);
    exit();
}
