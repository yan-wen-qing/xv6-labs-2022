#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;
backtrace();
  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_sigreturn(void)
{
  struct proc* proc = myproc();
  // re-store trapframe so that it can return to the interrupt code before.
  *proc->trapframe = proc->saved_trapframe;
  proc->have_return = 1; // true
  return proc->trapframe->a0;
}

uint64
sys_sigalarm(void)
{
  int ticks;
  uint64 handler_va;

  argint(0, &ticks);
  argaddr(1, &handler_va);
  struct proc* proc = myproc();
  proc->alarm_interval = ticks;
  proc->handler_va = handler_va;
  proc->have_return = 1; // true
  return 0;
}

uint64
sys_trace(void)
{
  int mask;

  argint(0, &mask);
  if(mask<0)
  return -1;
  myproc()->syscall_trace|=mask;
  //struct proc *p = myproc();
  //p->mask = mask;
  return 0;
  }

/*uint64
sys_sysinfo(void)
{
  // user pointer to struct sysinfo
  uint64 si_addr;

  argaddr(0, &si_addr);
  int nproc;
  int freemem;

  nproc = proc_not_unsed_num();
  freemem = free_mem_num();

  struct sysinfo sysinfo;
  sysinfo.freemem = freemem;
  sysinfo.nproc = nproc;

  struct proc *p = myproc();
  if (copyout(p->pagetable, si_addr, (char *)&sysinfo, sizeof(sysinfo)) < 0)
    return -1;

  return 0;
}*/

