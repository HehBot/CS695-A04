#include "defs.h"
#include "proc.h"
#include "types.h"

int sys_fork(void)
{
    return fork();
}

int sys_exit(void)
{
    exit();
    return 0; // not reached
}

int sys_wait(void)
{
    return wait();
}

int sys_kill(void)
{
    int pid;

    if (argint(0, &pid) < 0)
        return -1;
    return kill(pid);
}

int sys_getpid(void)
{
    return myproc()->pid[0];
}
int sys_getgpid(void)
{
    return myproc()->global_pid;
}

int sys_sbrk(void)
{
    int addr;
    int n;

    if (argint(0, &n) < 0)
        return -1;
    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

int sys_sleep(void)
{
    int n;
    uint ticks0;

    if (argint(0, &n) < 0)
        return -1;
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n) {
        if (myproc()->killed) {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}

int sys_unshare(void)
{
    int arg;
    if (argint(0, &arg) < 0)
        return -1;
    return unshare(arg);
}

int sys_cpu_restrict(void)
{
    int pid;
    int mask;
    if (argint(0, &pid) < 0 || argint(1, &mask) < 0)
        return -1;
    if ((uint)mask >= (1 << ncpu))
        return -1;
    return cpu_restrict(pid, mask);
}

int sys_setns(void)
{
    int pid, mask;
    struct proc* target;
    if (argint(0, &pid) < 0 || argint(1, &mask) < 0 || ((target = getproc(pid)) == NULL))
        return -1;
    return setns(target, mask);
}
