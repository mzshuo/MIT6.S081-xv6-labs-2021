## Lab: mmap (hard)

> Implement Unix `mmap()` and `munmap()` function:
> 
> `void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);`
> `int munmap(addr, length);`
>
> mmap can be called in many ways, but this lab requires only a subset of its features relevant to memory-mapping a file. You can assume that addr will always be zero, meaning that the kernel should decide the virtual address at which to map the file. mmap returns that address, or 0xffffffffffffffff if it fails. length is the number of bytes to map; it might not be the same as the file's length. prot indicates whether the memory should be mapped readable, writeable, and/or executable; you can assume that prot is PROT_READ or PROT_WRITE or both. flags will be either MAP_SHARED, meaning that modifications to the mapped memory should be written back to the file, or MAP_PRIVATE, meaning that they should not. You don't have to implement any other bits in flags. fd is the open file descriptor of the file to map. You can assume offset is zero (it's the starting point in the file at which to map).
>
> munmap(addr, length) should remove mmap mappings in the indicated address range. If the process has modified the memory and has it mapped MAP_SHARED, the modifications should first be written to the file. An munmap call might cover only a portion of an mmap-ed region, but you can assume that it will either unmap at the start, or at the end, or the whole region (but not punch a hole in the middle of a region).

1. add system call interface

   ```c
   // user/user.h
   void* mmap(void *addr, int, int, int, int, int);
   int munmap(void *addr, int);
   ```

   ```c
   // user/usys.pl
   entry("mmap");
   entry("munmap");
   ```

   ```c
   // kernel/syscall.h
   #define SYS_mmap   22
   #define SYS_munmap 23
   ```

   ```c
   // kernel/syscall.c
   extern uint64 sys_mmap(void);
   extern uint64 sys_munmap(void);

   static uint64 (*syscalls[])(void) = {
   // ...
   [SYS_mmap]    sys_mmap,
   [SYS_munmap]  sys_munmap,
   };

   ```

2. add variables needed to `struct proc` and initiate them in `allocproc()`

   ```c
   // kernel/proc.h
   struct vma {
     uint64 vm_start;
     uint64 vm_end;
     int prot;
     int flags;
     struct file *fp;
     int off;
   };

   #define NVMA 16

   struct proc {
     // ...
     struct vma vma[NVMA];
     uint64 topaddr;
   };
   ```

   ```c
   // kernel/proc.c
   static struct proc*
   allocproc(void)
   {
     // ...
     for(int i = 0; i < NVMA; ++i){
       p->vma[i].vm_start = 0;
     }
     p->topaddr = TRAPFRAME;
     // ...
   }
   ```

3. implement `sys_mmap()` function

   ```c
   // kernel/sysproc.c
   #include "fcntl.h"
   #include "sleeplock.h"
   #include "fs.h"
   #include "file.h"

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
   ```

4. handle page fault in `usertrap()`

   ```c
   // kernel/trap.c
   #include "fcntl.h"
   #include "sleeplock.h"
   #include "fs.h"
   #include "file.h"

   void
   usertrap(void)
   {
     // ...
     else if(r_scause() == 0xd || r_scause() == 0xf){
       uint64 va = r_stval();
       if(va >= p->topaddr && va < TRAPFRAME){
         // page fault
         struct vma *vp = 0;
         for(int i = 0; i < NVMA; ++i){
           if(va >= p->vma[i].vm_start && va < p->vma[i].vm_end){
             vp = &p->vma[i];
             break;
           }
           if(i == NVMA - 1){
             panic("usertrap(): vma not found");
           }
         }

         uint64 pa = (uint64)kalloc();
         memset((void*)pa, 0, PGSIZE);
         ilock(vp->fp->ip);
         readi(vp->fp->ip, 0, pa, va-(vp->vm_start), PGSIZE);
         iunlock(vp->fp->ip);

         int perm = 0 | PTE_V | PTE_U;
         if(vp->prot & PROT_READ)
           perm |= PTE_R;
         if(vp->prot & PROT_WRITE)
           perm |= PTE_W;
         if(vp->prot & PROT_EXEC)
           perm |= PTE_X;

         mappages(p->pagetable, va, PGSIZE, pa, perm);      
       } 
     }
   }
   ```

5. implement `munmap()` function

   ```c
   // kernel/sysproc.c
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
   ```

6. modify `exit()` and `fork()` correspondingly

   ```c
   // kernel/proc.c
   #include "fcntl.h"
   #include "sleeplock.h"
   #include "fs.h"
   #include "file.h"

   void
   exit(int status)
   {
     // ...
     for(int i = 0; i < NVMA; ++i){
       if(p->vma[i].vm_start != 0 && walkaddr(p->pagetable, p->vma[i].vm_start) != 0){
         struct vma *vp = &p->vma[i];
         int len = vp->vm_end - vp->vm_start;
         if(vp->flags & MAP_SHARED)
           filewrite(vp->fp, vp->vm_start, len);
         fileclose(vp->fp);
         uvmunmap(p->pagetable, vp->vm_start, len/PGSIZE, 1);
         vp->vm_start = 0;
       }
     }
     // ...
   }

   int
   fork(void)
   {
     // ...
     for(int i = 0; i < NVMA; ++i){
       if(p->vma[i].vm_start != 0){
         memmove(&np->vma[i], &p->vma[i], sizeof(struct vma));
         filedup(p->vma[i].fp);
       }
     }
     np->topaddr = p->topaddr;
     // ...
   }
   ```
