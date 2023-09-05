#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

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

uint64
sys_mmap(void)
{
  uint64 addr;
  int length, prot, flags, fd, offset;
  struct proc *p = myproc();
  struct vma *vp = 0;

  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 ||
     argint(3, &flags) < 0 || argint(4, &fd) < 0 || argint(5, &offset) < 0){
      return -1;
  }

  // mmap doesn't allow read/write mapping of a file opened read-only.
  if(p->ofile[fd]->writable == 0 && (flags & MAP_SHARED) && (prot & PROT_WRITE)){
    return -1;
  }

  int i;
  for(i = 0; i < NVMA; ++i){
    if(p->vma[i].vm_start == 0){
      vp = &p->vma[i];
      break;
    }
  }
  if(vp == 0){
    printf("mmap(): no empty vma\n");
    return -1;
  }

  if(addr == 0){
    p->topaddr -= length;
    vp->vm_start = p->topaddr;
    vp->vm_end = vp->vm_start + length;
    vp->prot = prot;
    vp->flags = flags;
    vp->fp = filedup(p->ofile[fd]);
    vp->off = offset;
    return p->topaddr;
  }
  
  return -1;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  int length;
  struct proc *p = myproc();
  struct vma *vp = 0;

  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;

  int i;
  for(i = 0; i < NVMA; ++i){
    if(addr >= p->vma[i].vm_start && addr < p->vma[i].vm_end){
      vp = &p->vma[i];
      break;
    }
    if(i == NVMA - 1){
      printf("munmap(): unmapped virtual memory %p\n", addr);
      return -1;
    }
  }

  // assert(addr % PGSIZE == 0);   // otherwise it cannot be implemented using uvmunmap()
  if(walkaddr(p->pagetable, addr) != 0){
    if(vp->flags & MAP_SHARED)
      filewrite(vp->fp, addr, length);
    uvmunmap(p->pagetable, addr, length/PGSIZE, 1);

    if(addr == vp->vm_start){
      vp->vm_start += length;
    }
    else if(addr + length == vp->vm_end){
      vp->vm_end -= length;
    }

    if(vp->vm_start == vp->vm_end){
      fileclose(vp->fp);
      vp->vm_start = 0;
    }
  }

  return 0;
}
