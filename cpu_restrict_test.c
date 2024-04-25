#include "stat.h"
#include "types.h"
#include "user.h"

#define N 2000000000

int workload(){
    int sum = 0;
    for (int i = 0; i < N; i++) {
        sum += i;
    }
    return sum;
}

int main(){
    
    int c1 = fork();
    if(c1 == 0){
        sleep(100);
        int pid = getpid();
        printf(1, "[%d] C1: start \n", pid);
        int w = workload();
        int utime = uptime();
        sleep(100);
        printf(1, "[%d] [%d] C1: finish %d\n", utime, pid, w);
        exit();
    }
    cpu_restrict(c1, 1);

    int c2 = fork();
    if(c2 == 0){
        sleep(100);
        int pid = getpid();
        printf(1, "[%d] C2: start\n", pid);
        int w = workload();
        int utime = uptime();
        sleep(200);
        printf(1, "[%d] [%d] C2: finish %d\n", utime, pid, w);
        exit();
    }
    cpu_restrict(c2, 2);

    sleep(100);
    int p = getpid();
    printf(1, "[%d] P: start\n", p);
    int w = workload();
    int utime = uptime();
    printf(1, "[%d] [%d] P: finish %d\n",utime, p, w);

    exit();
}