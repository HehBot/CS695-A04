#include "defs.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "types.h"
#include "x86.h"

struct pid_ns {
    struct pid_ns* parent;
    struct proc* initproc;
    int next_pid;

    struct spinlock lock;
    int nr_proc;
};

struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

int root_proc_inum;
extern int proc_inode_counter;

static pid_ns_t root_pid_ns;

struct {
    struct spinlock lock; // protects table and each entry of free
                          // individual table entries are protected by individual locks
    pid_ns_t table[32];
    int free[32];
} pid_nss;

static pid_ns_t* alloc_pid_ns(pid_ns_t* parent)
{
    acquire(&pid_nss.lock);
    for (int i = 0; i < 32; ++i)
        if (pid_nss.free[i]) {
            pid_nss.free[i] = 0;
            pid_nss.table[i].parent = parent;
            pid_nss.table[i].next_pid = 1;
            pid_nss.table[i].initproc = NULL;
            pid_nss.table[i].nr_proc = 0;
            release(&pid_nss.lock);
            return &pid_nss.table[i];
        }
    panic("alloc_pid_ns");
}
static void pid_ns_put(pid_ns_t* p)
{
    int free = 0;
    int i = p - &pid_nss.table[0];
    acquire(&p->lock);
    p->nr_proc--;
    if (p->nr_proc == 0)
        free = 1;
    release(&p->lock);

    if (free) {
        acquire(&pid_nss.lock);
        pid_nss.free[i] = 1;
        release(&pid_nss.lock);
    }
}
static pid_ns_t* pid_ns_get(pid_ns_t* p)
{
    acquire(&p->lock);
    p->nr_proc++;
    release(&p->lock);
    return p;
}
static int pid_ns_next_pid(pid_ns_t* p)
{
    acquire(&p->lock);
    int l = p->next_pid++;
    release(&p->lock);
    return l;
}

int namespace_depth(pid_ns_t* ancestor, pid_ns_t* curr)
{
    if (curr == NULL || ancestor == NULL)
        return -1;
    // panic("wtf");
    int i = 0;
    acquire(&curr->lock);
    while (1) {
        if (curr == ancestor) {
            release(&curr->lock);
            return i;
        }
        i++;
        pid_ns_t* new = curr->parent;
        if (new == NULL) {
            release(&curr->lock);
            return -1;
        }
        acquire(&new->lock);
        release(&curr->lock);
        curr = new;
    }
}

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void* chan);

void pinit(void)
{
    root_pid_ns.parent = NULL;
    root_pid_ns.initproc = NULL;
    root_pid_ns.next_pid = 1;

    for (int i = 0; i < 32; ++i) {
        initlock(&pid_nss.table[i].lock, "pid_nss.table[i]");
        pid_nss.free[i] = 1;
        root_proc_inum = ++proc_inode_counter;
    }
    initlock(&pid_nss.lock, "pid_nss.lock");
    initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
    return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
    int apicid, i;

    if (readeflags() & FL_IF)
        panic("mycpu called with interrupts enabled\n");

    apicid = lapicid();
    // APIC IDs are not guaranteed to be contiguous. Maybe we should have
    // a reverse map, or reserve a register to store &cpus[i].
    for (i = 0; i < ncpu; ++i) {
        if (cpus[i].apicid == apicid)
            return &cpus[i];
    }
    panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void)
{
    struct cpu* c;
    struct proc* p;
    pushcli();
    c = mycpu();
    p = c->proc;
    popcli();
    return p;
}

// PAGEBREAK: 32
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc*
allocproc(void)
{
    struct proc* p;
    char* sp;

    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == UNUSED)
            goto found;

    release(&ptable.lock);
    return 0;

found:
    p->state = EMBRYO;
    // pids need to be set by caller

    release(&ptable.lock);

    // Allocate kernel stack.
    if ((p->kstack = kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }
    sp = p->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *p->tf;
    p->tf = (struct trapframe*)sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint*)sp = (uint)trapret;

    sp -= sizeof *p->context;
    p->context = (struct context*)sp;
    memset(p->context, 0, sizeof *p->context);
    p->context->eip = (uint)forkret;

    p->procfs_nums[0] = ++proc_inode_counter;
    p->procfs_nums[1] = ++proc_inode_counter;

    p->cpu_mask = ((1 << ncpu) - 1);

    return p;
}

// PAGEBREAK: 32
//  Set up first user process.
void userinit(void)
{
    struct proc* p;
    extern char _binary_initcode_start[], _binary_initcode_size[];

    p = allocproc();

    if ((p->pgdir = setupkvm()) == 0)
        panic("userinit: out of memory?");
    inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
    p->sz = PGSIZE;
    memset(p->tf, 0, sizeof(*p->tf));
    p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
    p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
    p->tf->es = p->tf->ds;
    p->tf->ss = p->tf->ds;
    p->tf->eflags = FL_IF;
    p->tf->esp = PGSIZE;
    p->tf->eip = 0; // beginning of initcode.S

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->root = namei("/");
    p->cwd = idup(p->root);

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    acquire(&ptable.lock);

    p->state = RUNNABLE;

    // set up root pid ns
    p->pid_ns = p->child_pid_ns = pid_ns_get(&root_pid_ns);

    p->pid_ns->initproc = p;
    p->pid[0] = pid_ns_next_pid(p->pid_ns);

    p->global_pid = p->pid[0];

    release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
    uint sz;
    struct proc* curproc = myproc();

    sz = curproc->sz;
    if (n > 0) {
        if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    } else if (n < 0) {
        if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    }
    curproc->sz = sz;
    switchuvm(curproc);
    return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
    int pid;
    struct proc* np;
    struct proc* curproc = myproc();

    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }

    // Set pids
    np->pid_ns = np->child_pid_ns = pid_ns_get(curproc->child_pid_ns);

    int entering_new_pid_ns = 0, new_pid_ns_init = 0;
    acquire(&np->pid_ns->lock);
    if (np->pid_ns != curproc->pid_ns) {
        // child will be in a different pid ns to parent
        entering_new_pid_ns = 1;
        if (np->pid_ns->next_pid == 1) {
            // child will be init of new ns
            new_pid_ns_init = 1;
            assert(np->pid_ns->initproc == NULL);
            np->pid_ns->initproc = np;
        }
    }
    release(&np->pid_ns->lock);

    pid_ns_t* iter = np->pid_ns;
    int i = 0;
    if (iter != NULL) {
        acquire(&iter->lock);
        while (1) {
            np->pid[i] = iter->next_pid++;
            pid_ns_t* next = iter->parent;
            if (next == NULL) {
                release(&iter->lock);
                break;
            }
            acquire(&next->lock);
            release(&iter->lock);
            iter = next;
            i++;
        }
    }
    np->global_pid = np->pid[i];

    // Copy process state from proc.
    if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
        kfree(np->kstack);
        np->kstack = 0;
        np->state = UNUSED;
        return -1;
    }
    np->sz = curproc->sz;
    if (!entering_new_pid_ns)
        np->parent = curproc;
    else if (new_pid_ns_init)
        np->parent = NULL;
    else
        np->parent = np->pid_ns->initproc;
    *np->tf = *curproc->tf;

    // Clear %eax so that fork returns 0 in the child.
    np->tf->eax = 0;

    for (int i = 0; i < NOFILE; i++)
        if (curproc->ofile[i])
            np->ofile[i] = filedup(curproc->ofile[i]);
    np->cwd = idup(curproc->cwd);
    np->root = idup(curproc->root);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    acquire(&ptable.lock);

    np->state = RUNNABLE;

    release(&ptable.lock);

    if (entering_new_pid_ns)
        return np->pid[1];
    else
        return np->pid[0];
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
//
//   If an init process exits all the subprocesses
//   are made ZOMBIEs and moved into global pid ns
//   to be reaped by global init
void exit(void)
{
    struct proc* curproc = myproc();
    int fd;

    // init process of a namespace is exiting
    if (curproc == curproc->pid_ns->initproc) {
        // if root namespace, panic
        if (curproc->pid_ns == &root_pid_ns)
            panic("init exiting");

        acquire(&ptable.lock);
        for (struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != UNUSED) {
                int i = namespace_depth(curproc->pid_ns, p->pid_ns);
                if (i == -1)
                    continue;

                // move process to global namespace
                // can't call pid_ns_put yet as the process may be running on another cpu
                p->pid[0] = p->global_pid;
                p->parent = root_pid_ns.initproc;
                p->killed = 1;
            }
        }
        release(&ptable.lock);
    }

    // Close all open files.
    for (fd = 0; fd < NOFILE; fd++) {
        if (curproc->ofile[fd]) {
            fileclose(curproc->ofile[fd]);
            curproc->ofile[fd] = NULL;
        }
    }

    pid_ns_put(curproc->pid_ns);

    begin_op();
    iput(curproc->cwd);
    iput(curproc->root);
    end_op();

    acquire(&ptable.lock);

    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);

    // Pass abandoned children to init.
    for (struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->parent == curproc) {
            p->parent = curproc->pid_ns->initproc;
            if (p->state == ZOMBIE)
                wakeup1(curproc->pid_ns->initproc);
        }
    }

    // Jump into the scheduler, never to return.
    curproc->state = ZOMBIE;
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
    struct proc* p;
    int havekids, pid;
    struct proc* curproc = myproc();

    acquire(&ptable.lock);
    for (;;) {
        // Scan through table looking for exited children.
        havekids = 0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->parent != curproc)
                continue;
            havekids = 1;
            if (p->state == ZOMBIE) {
                // Found one.

                pid = p->pid[0];
                kfree(p->kstack);
                p->kstack = 0;
                freevm(p->pgdir);
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                p->state = UNUSED;
                release(&ptable.lock);

                // cprintf("%d waited on %d\n", curproc->global_pid, p->global_pid);

                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || curproc->killed) {
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(curproc, &ptable.lock); // DOC: wait-sleep
    }
}

// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.
void scheduler(void)
{
    struct proc* p;
    struct cpu* c = mycpu();
    c->proc = 0;
    int cpu_id = cpuid();

    for (;;) {
        // Enable interrupts on this processor.
        sti();

        // Loop over process table looking for process to run.
        acquire(&ptable.lock);
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != RUNNABLE || ((p->cpu_mask & (1 << cpu_id)) == 0))
                continue;

            // cprintf("PID : %d, cpuID: %d\n", p->pid[0], cpu_id);

            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            c->proc = p;
            switchuvm(p);
            p->state = RUNNING;

            swtch(&(c->scheduler), p->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
        }
        release(&ptable.lock);
    }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
    int intena;
    struct proc* p = myproc();

    if (!holding(&ptable.lock))
        panic("sched ptable.lock");
    if (mycpu()->ncli != 1)
        panic("sched locks");
    if (p->state == RUNNING)
        panic("sched running");
    if (readeflags() & FL_IF)
        panic("sched interruptible");
    intena = mycpu()->intena;
    swtch(&p->context, mycpu()->scheduler);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
    acquire(&ptable.lock); // DOC: yieldlock
    myproc()->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
    static int first = 1;
    // Still holding ptable.lock from scheduler.
    release(&ptable.lock);

    if (first) {
        // Some initialization functions must be run in the context
        // of a regular process (e.g., they call sleep), and thus cannot
        // be run from main().
        first = 0;
        iinit(ROOTDEV);
        initlog(ROOTDEV);
    }

    // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void* chan, struct spinlock* lk)
{
    struct proc* p = myproc();

    if (p == 0)
        panic("sleep");

    if (lk == 0)
        panic("sleep without lk");

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    if (lk != &ptable.lock) { // DOC: sleeplock0
        acquire(&ptable.lock); // DOC: sleeplock1
        release(lk);
    }
    // Go to sleep.
    p->chan = chan;
    p->state = SLEEPING;

    sched();

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.
    if (lk != &ptable.lock) { // DOC: sleeplock2
        release(&ptable.lock);
        acquire(lk);
    }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan.
//  The ptable lock must be held.
static void
wakeup1(void* chan)
{
    struct proc* p;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == SLEEPING && p->chan == chan)
            p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void* chan)
{
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
    struct proc* p;
    struct proc* curproc = myproc();

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state != UNUSED) {
            int i = namespace_depth(curproc->pid_ns, p->pid_ns);
            if (i == -1)
                continue;

            if (p->pid[i] == pid) {
                p->killed = 1;
                // Wake process from sleep if necessary.
                if (p->state == SLEEPING)
                    p->state = RUNNABLE;
                release(&ptable.lock);
                return 0;
            }
        }
    }
    release(&ptable.lock);
    return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging.
//  Runs when user types ^P on console.
//  No lock to avoid wedging a stuck machine further.
void procdump(void)
{
    static char* states[] = {
        [UNUSED] = "unused",
        [EMBRYO] = "embryo",
        [SLEEPING] = "sleep ",
        [RUNNABLE] = "runble",
        [RUNNING] = "run   ",
        [ZOMBIE] = "zombie"
    };
    int i;
    struct proc* p;
    char* state;
    uint pc[10];

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        cprintf("%s %s ", state, p->name);

        pid_ns_t* iter = p->pid_ns;
        int i = 0;
        while (iter != NULL) {
            cprintf("%d[%p]->", p->pid[i], iter);
            i++;
            iter = iter->parent;
        }

        if (p->state == SLEEPING) {
            getcallerpcs((uint*)p->context->ebp + 2, pc);
            for (i = 0; i < 10 && pc[i] != 0; i++)
                cprintf(" %p", pc[i]);
        }
        cprintf("\n");
    }
}

#define NEWNS_PID (1 << 0)
#define NEWNS_NET (1 << 1)

int unshare(int arg)
{
    struct proc* curproc = myproc();
    if (arg & NEWNS_PID)
        curproc->child_pid_ns = alloc_pid_ns(curproc->pid_ns);
    return 0;
}

int cpu_restrict(int pid, int mask)
{
    struct proc* p;
    struct proc* curproc = myproc();

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state != UNUSED) {
            int i = namespace_depth(curproc->pid_ns, p->pid_ns);
            if (i == -1)
                continue;

            if (p->pid[i] == pid) {
                p->cpu_mask = mask;
                release(&ptable.lock);
                return 0;
            }
        }
    }
    release(&ptable.lock);
    return -1;
}
