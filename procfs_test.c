#include "stat.h"
#include "types.h"
#include "user.h"

int main(void){
    // mkdir("abc");
    // mount_procfs("/abc");
    char * ps_bf[] = {"ps"};
    unshare(1);
    if(fork() == 0){
        printf(1, "child:\n");
        sleep(100);
        exec(ps_bf[0], &ps_bf[0]);
    }
    else{
        sleep(500);
        printf(1, "parent:\n");
        exec(ps_bf[0], &ps_bf[0]);
    }
    exit();
}

