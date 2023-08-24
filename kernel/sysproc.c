#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
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


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
/* A system call that reports which pages have been accessed.
 * The system call takes three arguments. 
 * First, it takes the starting virtual address of the first user page to check. 
 * Second, it takes the number of pages to check. 
 * Finally, it takes a user address to a buffer to store the results into a bitmask.
*/
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 base;
  int len;
  uint64 maskaddr;

  if (argaddr(0, &base) < 0)
    return -1;
  if (argint(1, &len) < 0)
    return -1;
  if (argaddr(2, &maskaddr) < 0)
    return -1;

  if (len > 32) {
    printf("Warning: can check only the first 32 pages of %d pages.\n", len);
    len = 32;
  }

  unsigned int mask = 0;
  for (int i = 0; i < len; ++i) {
    uint64 va = base + PGSIZE * i;
    pte_t* pte = walk(myproc()->pagetable, va, 0);
    if (*pte & PTE_A) {
      mask |= (1L << i);
      *pte &= ~PTE_A;
    }
  }
  
  if (copyout(myproc()->pagetable, maskaddr, (char*)&mask, sizeof(mask)) < 0) {
    return -1;
  }
  
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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
