#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

// p3-01
#ifdef CS333_P3P4
struct state_lists {
  struct proc *ready[MAXPRIO+1],    *ready_tail[MAXPRIO+1];
  struct proc *free,                *free_tail;
  struct proc *sleep,               *sleep_tail;
  struct proc *zombie,              *zombie_tail;
  struct proc *running,             *running_tail;
  struct proc *embryo,              *embryo_tail;
};

#define TICKS_TO_PROMOTE MAXBUDG*2
#endif

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  // p3-02
#ifdef CS333_P3P4
  struct state_lists pLists;
  int promoteAtTime;
#endif
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

#ifdef CS333_P3P4
static void initProcessLists(void);
static void initFreeList(void);
// __attribute__ ((unused)) suppresses warnings for routines that are not
// currently used but will be used later in the project. This is a necessary
// side-effect of using -Werror in the Makefile.
static void __attribute__ ((unused)) stateListAdd(struct proc** head, struct proc** tail, struct proc* p);
static void __attribute__ ((unused)) stateListAddAtHead(struct proc** head, struct proc** tail, struct proc* p);
static int __attribute__ ((unused)) stateListRemove(struct proc** head, struct proc** tail, struct proc* p);
static void assertState(struct proc *p, enum procstate s);

// p4
int __attribute__ ((unused)) setpriority(int pid, int priority);
int __attribute__ ((unused)) getpriority(int pid);
//
static void __attribute__ ((unused)) promoteAll();
static void __attribute__ ((unused)) promoteProc(struct proc* p);
static void __attribute__ ((unused))  demoteProc(struct proc* p);

static void assertPriority(void);
#endif

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

#ifndef CS333_P3P4
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
#else
  acquire(&ptable.lock);
  if (ptable.pLists.free != 0) {
    p = ptable.pLists.free;
    goto found;
  }
  release(&ptable.lock);
#endif

  return 0;

found:
  // TRANSITION
#ifndef CS333_P3P4
  p->state = EMBRYO;
#else
  if (stateListRemove(&ptable.pLists.free, &ptable.pLists.free_tail, p) < 0)
    panic("alloc - remove from free failed");
  assertState(p, UNUSED);

  p->state = EMBRYO;

  stateListAddAtHead(&ptable.pLists.embryo, &ptable.pLists.embryo_tail, p);
  assertState(p, EMBRYO);
#endif

  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    // TRANSITION
#ifndef CS33_P3P4
    p->state = UNUSED;
#else
    acquire(&ptable.lock);

    if (stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryo_tail, p) < 0)
      panic("alloc - remove from embryo failed");
    assertState(p, EMBRYO);

    p->state = UNUSED;

    stateListAddAtHead(&ptable.pLists.free, &ptable.pLists.free_tail, p);
    assertState(p, UNUSED);
    
    release(&ptable.lock);
#endif

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

  // p1-13
#ifdef CS333_P1
  p->start_ticks = ticks;
#endif

  // p2-14
#ifdef CS333_P2
  p->cpu_ticks_total = 0; 
  p->cpu_ticks_in = 0; 
#endif

#ifndef CS333_P3P4
  p->budget = MAXBUDG;
  p->priority = 0;
#endif

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

#ifdef CS333_P3P4
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
#endif
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
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
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // p2-10
#ifdef CS333_P2
  p->uid = PROC_UID;
  p->gid = PROC_GID;

  p->parent = p;
#endif

  // TRANSITION P4
#ifndef CS333_P3P4
  p->state = RUNNABLE;
#else
  acquire(&ptable.lock);

  if (stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryo_tail, p) < 0)
    panic("userinit - remove from embryo failed");
  assertState(p, EMBRYO);

  p->state = RUNNABLE;

  stateListAdd(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);
  assertState(p, RUNNABLE);

  release(&ptable.lock);
#endif
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    // TRANSITION
#ifndef CS333_P3P4
    np->state = UNUSED;
#else
    acquire(&ptable.lock);

    if (stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryo_tail, np) < 0)
      panic("fork - remove from embryo failed");
    assertState(np, EMBRYO);

    np->state = UNUSED;

    stateListAddAtHead(&ptable.pLists.free, &ptable.pLists.free_tail, np);
    assertState(np, UNUSED);

    release(&ptable.lock);
#endif

    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  // p2-11
#ifdef CS333_P2
  np->uid = proc->uid;
  np->gid = proc->gid;
#endif

  pid = np->pid;

#ifndef CS333_P3P4
  np->priority = 0;
  np->budget = MAXBUDG;
#endif

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  // TRANSITION P4
#ifndef CS333_P3P4
  np->state = RUNNABLE;
#else
  if (stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryo_tail, np))
    panic("userinit - remove from embryo failed");
  assertState(np, EMBRYO);

  np->state = RUNNABLE;

  stateListAdd(&ptable.pLists.ready[np->priority], &ptable.pLists.ready_tail[np->priority], np);
  assertState(np, RUNNABLE);
#endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifndef CS333_P3P4
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  // TRANSITION
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.pLists.embryo; p != 0; p = p->next)
    if(p->parent == proc)
      p->parent = initproc;

  // Pass abandoned children to init.
  for (int i = 0; i < MAXPRIO+1; i++) {
    for(p = ptable.pLists.ready[i]; p != 0; p = p->next)
      if(p->parent == proc)
        p->parent = initproc;
  }

  // Pass abandoned children to init.
  for(p = ptable.pLists.running; p != 0; p = p->next)
    if(p->parent == proc)
      p->parent = initproc; 

  // Pass abandoned children to init.
  for(p = ptable.pLists.sleep; p != 0; p = p->next)
    if(p->parent == proc)
      p->parent = initproc;

  // Pass abandoned children to init.
  for(p = ptable.pLists.zombie; p != 0; p = p->next){
    if(p->parent == proc){
      p->parent = initproc;
      wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.

  // TRANSITION
  #ifndef CS333_P3P4  
  proc->state = ZOMBIE;
  #else
  stateListRemove(&ptable.pLists.running, &ptable.pLists.running_tail, proc);
  assertState(proc, RUNNING);

  proc->state = ZOMBIE;

  stateListAdd(&ptable.pLists.zombie, &ptable.pLists.zombie_tail, proc);
  assertState(proc, ZOMBIE);

  demoteProc(proc);
  #endif

  sched();
  panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifndef CS333_P3P4
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        // TRANSITION
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.pLists.zombie; p != 0; p = p->next){
      if(p->parent != proc)
        continue;
      havekids = 1;
      // Found one.
      pid = p->pid;
      kfree(p->kstack);
      p->kstack = 0;
      freevm(p->pgdir);

      // TRANSITION
      #ifndef CS333_P3P4
      p->state = UNUSED;
      #else
      if (stateListRemove(&ptable.pLists.zombie, &ptable.pLists.zombie_tail, p) < 0)
        panic("wait - remove from zombie failed");
      assertState(p, ZOMBIE);

      p->state = UNUSED;

      stateListAdd(&ptable.pLists.free, &ptable.pLists.free_tail, p);
      assertState(p, UNUSED);
      #endif

      p->pid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->killed = 0;
      release(&ptable.lock);
      return pid;
    }

    // No point waiting if we don't have any children.
    for (p = ptable.pLists.embryo; p != 0; p = p->next) {
      if (p->parent == proc) {
        havekids = 1;
      }
    }
    // No point waiting if we don't have any children.
    for (int i = 0; i < MAXPRIO+1; i++) {
      for (p = ptable.pLists.ready[i]; p != 0; p = p->next) {
        if (p->parent == proc) {
          havekids = 1;
        }
      }  
    }
    // No point waiting if we don't have any children.
    for (p = ptable.pLists.running; p != 0; p = p->next) {
      if (p->parent == proc) {
        havekids = 1;
      }
    }
    // No point waiting if we don't have any children.
    for (p = ptable.pLists.sleep; p != 0; p = p->next) {
      if (p->parent == proc) {
        havekids = 1;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifndef CS333_P3P4
// original xv6 scheduler. Use if CS333_P3P4 NOT defined.
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;

      // p2-16
      #ifdef CS333_P2
      p->cpu_ticks_in = ticks;
      #endif

      switchuvm(p);
      // TRANSITION
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}

#else
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    assertPriority();
    promoteAll();

    // p4 loop for lists
    for (int i = 0; i < MAXPRIO+1; i++) {

      p = ptable.pLists.ready[i];
      if (p == 0)
        continue;
        
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;

      // p2-16
      #ifdef CS333_P2
      p->cpu_ticks_in = ticks;
      #endif

      switchuvm(p);

      // TRANSITION P4
      #ifndef CS333_P3P4
      p->state = RUNNING;
      #else
      assertPriority();
       
      if (stateListRemove(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p) != 0) {

        cprintf("%d:%d:%d\n", p->pid, p->priority, ptable.pLists.ready[p->priority]);
        panic("scheduler - remove from ready failed");
      }
      assertState(p, RUNNABLE);

      p->state = RUNNING;

      stateListAddAtHead(&ptable.pLists.running, &ptable.pLists.running_tail, p);
      assertState(p, RUNNING);
      #endif

      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
      
  // p2-17
  #ifdef CS333_P2
  proc->cpu_ticks_total += ticks - proc->cpu_ticks_in;
  #endif

  // p4
  #ifdef CS333_P3P4
  proc->budget -= ticks - proc->cpu_ticks_in;
  #endif

  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  
  //struct proc *p = proc;

  // TRANSITION P4
  #ifndef CS333_P3P4
  proc->state = RUNNABLE;
  #else
  if (stateListRemove(&ptable.pLists.running, &ptable.pLists.running_tail, proc))
    panic("sched - remove from running failed");
  assertState(proc, RUNNING);

  proc->state = RUNNABLE;

  stateListAdd(&ptable.pLists.ready[proc->priority], &ptable.pLists.ready_tail[proc->priority], proc);
  assertState(proc, RUNNABLE);
  #endif

  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
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
// 2016/12/28: ticklock removed from xv6. sleep() changed to
// accept a NULL lock to accommodate.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    if (lk) release(lk);
  }

  // Go to sleep.
  proc->chan = chan;

  // TRANSITION
  #ifndef CS333_P3P4
  proc->state = SLEEPING;
  #else
  if (stateListRemove(&ptable.pLists.running, &ptable.pLists.running_tail, proc))
    panic("sleep - remove from running failed");
  assertState(proc, RUNNING);

  proc->state = SLEEPING;

  stateListAdd(&ptable.pLists.sleep, &ptable.pLists.sleep_tail, proc);
  assertState(proc, SLEEPING);
  
  demoteProc(proc);
  #endif

  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#ifndef CS333_P3P4
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      // TRANSITION
      p->state = RUNNABLE;
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.pLists.sleep; p != 0; p = p->next) {
    if(p->chan == chan) {
      // TRANSITION P4
      #ifndef CS333_P3P4
      p->state = RUNNABLE;
      #else
      if (stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleep_tail, p))
        panic("wakeup1 - remove from sleep failed");
      assertState(p, SLEEPING);

      p->state = RUNNABLE;

      stateListAdd(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);
      assertState(p, RUNNABLE);
      #endif
    }
  }
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifndef CS333_P3P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        // TRANSITION P4
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);

  for(p = ptable.pLists.embryo; p != 0; p = p->next){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
  }

  for (int i = 0; i < MAXPRIO+1; i++) {
    for(p = ptable.pLists.ready[i]; p != 0; p = p->next){
      if(p->pid == pid){
        p->killed = 1;
        release(&ptable.lock);
        return 0;
      }
    }
  }

  for(p = ptable.pLists.running; p != 0; p = p->next) {
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
  }

  for(p = ptable.pLists.sleep; p != 0; p = p->next) {
    if(p->pid == pid){
      p->killed = 1;

      // TRANSITION P4
      #ifndef CS333_P3P4
      p->state = RUNNABLE;
      #else
      stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleep_tail, p);
      assertState(p, SLEEPING);

      p->state = RUNNABLE;

      stateListAdd(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);
      assertState(p, RUNNABLE);
      #endif

      release(&ptable.lock);
      return 0;
    }
  }

  release(&ptable.lock);
  return -1;
}
#endif

static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
};

// p1-14
#ifdef CS333_P1
void 
print_ticks(int time) 
{
  // prints time (in ticks) in seconds using leading zeros
  cprintf("%d.", time        / 1000);
  cprintf("%d",  time % 1000 / 100);
  cprintf("%d",  time % 100  / 10);
  cprintf("%d",  time % 10   / 1);
  cprintf("s\t");
}
#endif 

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

// p2-31
#ifdef CS333_P2
  cprintf("PID\tName\tUID\tGID\tPPID\tPrio\tElapsed\tCPU\tSTATE\tPCs\n");
// p1-15
#elif CS333_P1
  cprintf("PID Name State Elapsed PCs\n");
#endif

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

// p2-32
#ifdef CS333_P2
    cprintf("%d\t%s\t%d\t%d\t%d\t%d\t", 
      p->pid, p->name, p->uid, p->gid, p->parent->pid, p->priority);
    print_ticks(ticks - p->start_ticks);
    print_ticks(p->cpu_ticks_total);
    cprintf("%s ", state);
// p1-16
#elif CS333_P1
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
    print_ticks(ticks - p->start_ticks);
#else
    cprintf("%d %s %s", p->pid, state, p->name);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// p2-26
#ifdef CS333_P2

#include "uproc.h"

int
getprocs(int max, struct uproc *t)
{
  int size = 0;
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if (size >= max) break;
    
    if (p->state != EMBRYO && p->state != UNUSED) {

      t[size].pid = p->pid;
      t[size].uid = p->uid;
      t[size].gid = p->gid;
      t[size].ppid = p->parent->pid;
      t[size].prio = p->priority;

      t[size].elapsed_ticks = ticks - p->start_ticks;
      t[size].CPU_total_ticks = p->cpu_ticks_total;
      t[size].size = p->sz;
      
      safestrcpy(t[size].state, states[p->state], STRMAX);
      safestrcpy(t[size].name, p->name, STRMAX);

      size++;
    }  
  }

  return size;
}
#endif

#ifdef CS333_P3P4
static void
assertState(struct proc *p, enum procstate s) {
  if (p->state != s)
    panic("assertState - wrong state");
}

static void
stateListAdd(struct proc** head, struct proc** tail, struct proc* p)
{
  if(*head == 0){
    *head = p;
    *tail = p;
    p->next = 0;
  } else{
    (*tail)->next = p;
    *tail = (*tail)->next;
    (*tail)->next = 0;
  }
}

static void
stateListAddAtHead(struct proc** head, struct proc** tail, struct proc* p)
{
  if(*head == 0){
    *head = p;
    *tail = p;
    p->next = 0;
  } else {
    p->next = *head;
    *head = p;
  }
}

static int
stateListRemove(struct proc** head, struct proc** tail, struct proc* p)
{
  if(*head == 0 || *tail == 0 || p == 0){
    if(*head == 0) cprintf("head - null pointer");
    if(*tail == 0) cprintf("tail - null pointer");
    if(p     == 0) cprintf("p - null pointer");
    return -1;
  }
  struct proc* current = *head;
  struct proc* previous = 0;

  if(current == p){
    *head = (*head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if(*tail == p){
      *tail = 0;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found, hit eject.
  if(current == 0){
    cprintf("process 404");
    return -1;
  }

  // Process found. Set the appropriate next pointer.
  if(current == *tail){
    *tail = previous;
    (*tail)->next = 0;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = 0;

  return 0;
}

static void
initProcessLists(void)
{
  for (int i = 0; i <= MAXPRIO; i++) {
    ptable.pLists.ready[i] = 0;
    ptable.pLists.ready_tail[i] = 0;
  } 
  ptable.pLists.free = 0;
  ptable.pLists.free_tail = 0;
  ptable.pLists.sleep = 0;
  ptable.pLists.sleep_tail = 0;
  ptable.pLists.zombie = 0;
  ptable.pLists.zombie_tail = 0;
  ptable.pLists.running = 0;
  ptable.pLists.running_tail = 0;
  ptable.pLists.embryo = 0;
  ptable.pLists.embryo_tail = 0;
}

static void
initFreeList(void)
{
  if(!holding(&ptable.lock)){
    panic("acquire the ptable lock before calling initFreeList\n");
  }

  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    // TRANSITION
    p->state = UNUSED;
    stateListAdd(&ptable.pLists.free, &ptable.pLists.free_tail, p);
  }
}

int
countList(struct proc *head) 
{
  struct proc *p = head;
  int i = 0;

  while (p != 0) {
    p = p->next;
    i++;
  }

  return i;
}

// p3-04
void 
printFree() 
{
  cprintf("Free List:\nF -> %d procs.\n", countList(ptable.pLists.free));
}

void 
printSleep() 
{
  struct proc *p;
  cprintf("Sleep List:\nS -> ");

  for (p = ptable.pLists.sleep; p != 0 ; p = p->next) {
    cprintf("%d -> ", p->pid);
  }
  cprintf(".\n");
}

void 
printReady() 
{
  struct proc *p;
  cprintf("Ready Lists:\n");
  
  for (int i = 0; i < MAXPRIO+1; i++) {
    cprintf("R%d -> ", i);

    for (p = ptable.pLists.ready[i]; p != 0 ; p = p->next) {
      cprintf("%d -> ", p->pid);
    }

    cprintf(".\n");
  }
}

void 
printZombie() 
{
  struct proc *p;
  cprintf("Zombie List:\nZ -> ");

  for (p = ptable.pLists.zombie; p != 0 ; p = p->next) {
    cprintf("(%d, %d) -> ", p->pid, p->parent->pid);
  }

  cprintf(".\n");

}

// p4 functions
static void
promoteAll() 
{

  int locked = 0;
  if(!holding(&ptable.lock)) {
    acquire(&ptable.lock);
    locked = 1;
  }

  // if not time, return out
  // 1..10..100..1000..1..10..100..1000
  if (ticks < ptable.promoteAtTime)
    return;

  cprintf("promoteAll - promoting procs\n");
  int v[2][MAXPRIO+1][2];
  for (int i = 0; i < MAXPRIO+1; i++) {
    v[0][i][0] = (int)&ptable.pLists.ready[i];
    v[0][i][1] = (int)&ptable.pLists.ready_tail[i];
  }

  // set up the first list, append the second to it,
  if (ptable.pLists.ready[0] && ptable.pLists.ready_tail[0]) {
    ptable.pLists.ready_tail[0]->next = ptable.pLists.ready[1];
    ptable.pLists.ready_tail[0]       = ptable.pLists.ready_tail[1];

    ptable.pLists.ready[1] = 0;
    ptable.pLists.ready_tail[1] = 0;
  } 
  else {
    ptable.pLists.ready[0] = ptable.pLists.ready[1];
    ptable.pLists.ready_tail[0] = ptable.pLists.ready_tail[1];

    ptable.pLists.ready[1] = 0;
    ptable.pLists.ready_tail[1] = 0;
  }
  // reset budgets
  for (struct proc *h = ptable.pLists.ready[0]; h != 0; h = h->next) {
    h->budget = MAXBUDG;
    h->priority = 0;
  }
  // done with first list

  // loop through the other lists, passing the value of the
  // lower pointers up.
  for (int i = 1; i < MAXPRIO; i++) {

    ptable.pLists.ready[i]      = ptable.pLists.ready[i+1];
    ptable.pLists.ready_tail[i] = ptable.pLists.ready_tail[i+1];

    for (struct proc *h = ptable.pLists.ready[i]; h != 0; h = h->next) {
      h->budget = MAXBUDG;
      h->priority = i-1;
    }
  }

  ptable.pLists.ready[MAXPRIO] = 0;
  ptable.pLists.ready_tail[MAXPRIO] = 0;

  for (int i = 0; i < MAXPRIO+1; i++) {
    v[1][i][0] = (int)&ptable.pLists.ready[i];
    v[1][i][1] = (int)&ptable.pLists.ready_tail[i];
    cprintf("%d -> %d >> %d -> %d\n", v[0][i][0], v[0][i][1], v[1][i][0], v[1][i][1]);
  }


  // reset sleepers
  for (struct proc *p = ptable.pLists.sleep; p != 0; p = p->next) {
    if (p->priority > 0) 
      p->priority--;
    p->budget = MAXBUDG;
  }

  // reset running procs
  for (struct proc *p = ptable.pLists.running; p != 0; p = p->next) {
    if (p->priority > 0) 
      p->priority--;
    p->budget = MAXBUDG;
  }

  // increment the promote timer
  ptable.promoteAtTime = ticks + TICKS_TO_PROMOTE;

  if(locked == 1)
    release(&ptable.lock);
}

static void
promoteProc(struct proc* p)
{
  // search for p in ready[p->priority] list
  // remove it off the list, dec the priority add
  // it onto the new list, then reset the budget.
  int locked = 0;

  // grab the lock if we dont have it
  if(!holding(&ptable.lock)) {
    acquire(&ptable.lock);
    locked = 1;
  }

  // remove the process, increase priority if poss, reset budget
  if (p->state == RUNNABLE)
    stateListRemove(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);

  if (p->priority < 0) p->priority--;
  p->budget = MAXBUDG;

  // add it to the new list
  if (p->state == RUNNABLE)
    stateListAdd(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);  

  // if we locked the lock, unlock
  if(locked == 1)
    release(&ptable.lock);
}

static void
demoteProc(struct proc* p)
{
  
  // search for p in ready[p->priority] list
  // remove it off the list, inc the priority add
  // it onto the new list, then reset the budget.
  int locked = 0;
  
  // check that this process is demotable
  if (p->budget <= MAXBUDG)
    return;

  cprintf("demoteProc - demoting procs\n");

  // grab the lock if we dont have it
  if(!holding(&ptable.lock)) {
    acquire(&ptable.lock);
    locked = 1;
  }

  //if (p->state == RUNNABLE)
    //stateListRemove(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);

  cprintf("decreasing process from %d to %d\n", p->priority, p->priority+1);
  if (p->priority < MAXPRIO) p->priority++;

  //if (p->state == RUNNABLE)
    //stateListAddAtHead(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);

  p->budget = MAXBUDG;

  // if we locked the lock, unlock
  if(locked == 1)
    release(&ptable.lock);

}

int 
setPriority(int pid, int priority)
{
  int i, locked = 0;
  struct proc *p;

  for (i = 0; i < MAXPRIO+1; i++) {
    for (p = ptable.pLists.ready[i]; p != 0; p = p->next) {

      if (p->pid == pid) {

        if (!holding(&ptable.lock)) {
          acquire(&ptable.lock);
          locked = 1;
        }
        
        // remove the process, increase priority if poss, reset budget
        if (p->state == RUNNABLE)
          stateListRemove(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);

        if (priority <= MAXPRIO && priority >= 0)
          p->priority = priority;
        else
          panic("setpriority - bad priority value");

        p->budget = MAXBUDG;

        // add it to the new list
        if (p->state == RUNNABLE)
          stateListAdd(&ptable.pLists.ready[p->priority], &ptable.pLists.ready_tail[p->priority], p);  

        if (locked == 1)
          release(&ptable.lock);

        return 0;
      }
    }
  }
  
  return -1;
}

int 
getPriority(int pid)
{  
  struct proc *p;
  int i = 0;

  for (i = 0; i <= MAXPRIO+1; i++) {
    for (p = ptable.pLists.ready[i]; p != 0; p = p->next) {
      if (p->pid == pid) 
        return p->priority;
    }
  }

  return -1;
}

static void
assertPriority(void)
{
  int locked = 0;

  if (!holding(&ptable.lock)) {
    printReady();
    locked = 1;
    acquire(&ptable.lock);     
  }

  for (int i = 0; i < MAXPRIO+1; i++) {
    for (struct proc *p = ptable.pLists.ready[i]; p != 0; p = p->next) {
      if (p->priority != i) {
        cprintf("%d,%d ", p->priority, i);
        panic("assertPriority - p not in right priority list");
      }
    }
  }
  
  if (locked == 1)
    release(&ptable.lock);
}
#endif
