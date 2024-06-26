#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exec)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(kill)

SYSCALL(read)
SYSCALL(write)
SYSCALL(pipe)

SYSCALL(open)
SYSCALL(close)
SYSCALL(fstat)
SYSCALL(mknod)

SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(link)
SYSCALL(unlink)
SYSCALL(rename)
SYSCALL(getcwd)

SYSCALL(dup)
SYSCALL(dup2)

SYSCALL(getpid)
SYSCALL(getgpid)

SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)

# ioctl
SYSCALL(ioctl)

# socket
SYSCALL(socket)
SYSCALL(bind)
SYSCALL(listen)
SYSCALL(accept)
SYSCALL(connect)
SYSCALL(recvfrom)
SYSCALL(sendto)

# namespaces
SYSCALL(unshare)
SYSCALL(chroot)

SYSCALL(mount_procfs)
SYSCALL(cpu_restrict)
SYSCALL(veth)
SYSCALL(setns)
SYSCALL(addroute)
