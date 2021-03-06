#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  xticks = ticks;
  return xticks;
}

//Turn off the computer
int
sys_halt(void)
{
  cprintf("Shutting down ...\n");
  outw( 0x604, 0x0 | 0x2000);
  return 0;
}

// p1-10
#if defined(CS333_P1)
int
sys_date(void)
{
  struct rtcdate *d;

  if ( argptr(0, (void*)&d, sizeof(struct rtcdate)) < 0 )
    return -1;

  else
    cmostime(d);

  return 0;
}
#endif

// p2-08
#ifdef CS333_P2
int
sys_getuid(void)
{
  return proc->uid;
}

int
sys_getgid(void)
{
  return proc->gid;
}

int
sys_getppid(void)
{
  return proc->parent->pid;
}

int
sys_setuid(void)
{
  int n;

  if(argint(0, &n) < 0)
    return -1;

  if (n < 0 || n > 32767)
    return -2;
   
  proc->uid = n;

  return 0;
}

int
sys_setgid(void)
{
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  if (n < 0 || n > 32767)
    return -2;

  proc->gid = n;

  return 0;
}

// p2-25
#include "uproc.h"

int 
sys_getprocs(void)
{
  struct uproc *t;
  int n;

  if(argint(0, &n) < 0)
    return -1;

  if (n < 0 || n > 32767)
    return -1;

  if ( argptr(1, (void*)&t, sizeof(struct uproc)*n) < 0 )
    return -2;
  
  n = getprocs(n,t);

  return n;
}
#endif

// p4
#ifdef CS333_P3P4
int 
sys_getPriority(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;

  if (pid < 0 || pid > 32767)
    return -1;


  return getPriority(pid);

}

int 
sys_setPriority(void)
{
  int pid, nprio;

  if(argint(0, &pid) < 0) return -1;
  if(argint(1, &nprio) < 0) return -1;

  if (pid < 0 || pid > 32767) return -1;
  if (nprio < 0 || nprio > 32767) return -1;

  return setPriority(pid, nprio);
}
#endif
