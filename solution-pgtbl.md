## Lab: page tables

#### 1. speed up system calls (easy)

> Some operating systems (e.g., Linux) speed up certain system calls by sharing data in a read-only region between userspace and the kernel. This eliminates the need for kernel crossings when performing these system calls. The first task is to implement this optimization for the getpid() system call in xv6.

1. add `struct usyscall *usyscall` to `struct proc` for each process
   ```c
   // kernel/proc.h
   // Per-process state
   struct proc {
     // ...
     struct usyscall *usyscall;   // shared data page to speed up system calls.
     // ...
   };
   ```

   ```c
   // kernel/memlayout.h
   // definition of struct usyscall
   #ifdef LAB_PGTBL
   #define USYSCALL (TRAPFRAME - PGSIZE)

   struct usyscall {
     int pid;  // Process ID
   };
   #endif
   ```

2. allocate a physical memory page to `p->usyscall` using function `kalloc()`, and initiate it
   
   ```c
   // kernel/proc.c
   static struct proc*
   allocproc(void)
   {
     // ...

     // Allocate a usyscall page.
     if ((p->usyscall = (struct usyscall *)kalloc()) == 0) {
       freeproc(p);
       release(&p->lock);
       return 0;
     }
     p->usyscall->pid = p->pid;

     // ...
   }
   ```

3. map the `USYSCALL` below `TRAPFRAME` to `p->usyscall` with `PTE_U | PTE_R` permission using `mappages()` function
   ```c
   // kernel/proc.c
   pagetable_t
   proc_pagetable(struct proc *p)
   {
     //...

     // map the USYSCALL just below TRAPFRAME, for speeding up system calls
     if(mappages(pagetable, USYSCALL, PGSIZE,
                 (uint64)(p->usyscall), PTE_U | PTE_R) < 0) {
       uvmunmap(pagetable, TRAMPOLINE, 1, 0);
       uvmfree(pagetable, 0);
       return 0;
     }

     // ...
   }
   ```

4. free the usyscall page in `freeproc()` and unmap it in `proc_freepagetable`
   ```c
   // kernel/proc.c
   static void
   freeproc(struct proc *p)
   {
     // ...

     if(p->usyscall)       // free usyscall page
       kfree((void*)p->usyscall);
     p->usyscall = 0;

     // ...
   }

   void
   proc_freepagetable(pagetable_t pagetable, uint64 sz)
   {
     uvmunmap(pagetable, TRAMPOLINE, 1, 0);
     uvmunmap(pagetable, TRAPFRAME, 1, 0);
     uvmunmap(pagetable, USYSCALL, 1, 0);
     uvmfree(pagetable, sz);
   }
   ```

#### 2. print a page table (easy)
> Write a function that prints the contents of a page table.

1. add `vmprint()` function in kernel/vm.c, which serves as a tool to print out a page table
   ```c
   // kernel/vm.c
   void vmprintHelper(pagetable_t pagetable, int level) {
     for (int i = 0; i < 512; ++i) {
       pte_t pte = pagetable[i];
       if (pte & PTE_V) {
         // print info of pte
         for (int rep = 0; rep < level; ++rep) {
           printf(" ..");
         }
         printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));
       }
       if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
         // this PTE points to a lower-level page table.
         vmprintHelper((pagetable_t)PTE2PA(pte), level+1);
       }
     }
   }

   // Print the contents of page table pointed by pagetable.
   void vmprint(pagetable_t pagetable) {
     printf("page table %p\n", pagetable);
     vmprintHelper(pagetable, 1);
   }
   ```

2. add its declaration
   ```c
   // kernel/defs.h
   void vmprint(pagetable_t);
   ```

3. call `vmprint()` in `exec()` to test the function
   ```c
   // kernel/exec.c
   int
   exec(char *path, char **argv)
   {
     // ...

     // print the page table of the first process
     if (p->pid == 1) {
       vmprint(p->pagetable);
     }

     return argc; 
   }
   ```

####  3. detecting which pages have been accessed (hard)
> Add a new feature to xv6 that detects and reports this information to userspace by inspecting the access bits in the RISC-V page table.

1. define `PTE_A` in *kernel/riscv.h*
   ```c
   // kernel/riscv.h
   #define PTE_A (1L << 6) // 1 -> the virtual page has been read, written, or fetched from 
                           // since the last time the A bit was cleared
   ```

2. implement `sys_pgaccess()` function in kernel/sysproc.c
   ```c
   // kernel/sysproc.c
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
   ```

3. add declaration of `walk()`
     ```c
     // kernel/defs.h
     pte_t*          walk(pagetable_t, uint64, int);
     ```