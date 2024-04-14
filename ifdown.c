#include "stat.h"
#include "types.h"
#include "user.h"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        exit();
    }
    ifdown(argv[1]);
    exit();
}
