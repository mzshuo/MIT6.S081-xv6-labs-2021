## Lab: Copy-on-Write Fork for xv6

#### 1. Implement copy-on-write (hard)

> Implement copy-on-write fork in the xv6 kernel.

1. add `PTE_C` as a mark bit for copy-on-write page
   ```c
   // kernel/riscv.h
   #define PTE_C (1L << 8) // 1 -> copy-on-write mode
   ```

2. modify `uvmcopy()` to delay memory allocation for forked child until it tries to write to the shared memory

   ```c
   // kernel/vm.c
   int
   uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
   {
     pte_t *pte;
     uint64 pa, i;
     uint flags;
     // char *mem;

     for(i = 0; i < sz; i += PGSIZE){
       if((pte = walk(old, i, 0)) == 0)
         panic("uvmcopy: pte should exist");
       if((*pte & PTE_V) == 0)
         panic("uvmcopy: page not present");
       pa = PTE2PA(*pte);
       flags = PTE_FLAGS(*pte);

       // if((mem = kalloc()) == 0)
       //   goto err;
       // memmove(mem, (char*)pa, PGSIZE);
       // if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
       //   kfree(mem);
       //   goto err;
       // }

       if(flags & PTE_W)
         flags = (flags | PTE_C) & (~PTE_W);
       *pte = PA2PTE(pa) | flags;
       if(mappages(new, i, PGSIZE, pa, flags) != 0)
         goto err;
       inc_refcount(pa);
     }
     return 0;

    err:
     uvmunmap(new, 0, i / PGSIZE, 1);
     return -1;
   }
   ```

3. set store page fault handler to allocate a new page of physical memory to cow process

   ```c
   // kernel/trap.c
   int cowfault(pagetable_t pagetable, uint64 va) {
     va = PGROUNDDOWN(va);
     if(va >= MAXVA || va >= myproc()->sz)
       return -1;

     pte_t *pte = walk(pagetable, va, 0);
     uint64 pa = PTE2PA(*pte);
     uint64 flags = PTE_FLAGS(*pte);

     if(pte == 0)
       return -1;
     if((flags & PTE_C) == 0)
       return -1;
     if((flags & PTE_V) == 0)
       return -1;
     
     flags = (flags | PTE_W) & (~PTE_C);
     if(get_refcount(pa) == 1){
       *pte = PA2PTE(pa) | flags;
     } else {
       uint64 npa = (uint64)kalloc();
       if(npa == 0)
         return -1;    // out of memory

       *pte = PA2PTE(npa) | flags;
       memmove((void*)npa, (const void*)pa, PGSIZE);
       kfree((void*)pa);
     }

     return 0;
   }

   void
   usertrap(void)
   {
     // ...
     } else if(r_scause() == 15){
       uint64 va = r_stval();
       if(cowfault(p->pagetable, va) < 0) 
         p->killed = 1;
     } // ...
   }
   ```

4. modify `copyout()` to avoid writing to a cow page

   ```c
   // kernel/vm.c
   int
   copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
   {
     uint64 n, va0, pa0;

     while(len > 0){
       va0 = PGROUNDDOWN(dstva);
       pa0 = walkaddr(pagetable, va0);
       if(pa0 == 0)
         return -1;

       // beginning of modification
       pte_t *pte = walk(pagetable, va0, 0);
       uint64 flags = PTE_FLAGS(*pte);
       if((flags & PTE_C))
         if(cowfault(pagetable, va0) < 0)
           return -1;
       pa0 = walkaddr(pagetable, va0);
       // end of modification
         
       n = PGSIZE - (dstva - va0);
       if(n > len)
         n = len;
       memmove((void *)(pa0 + (dstva - va0)), src, n);

       len -= n;
       src += n;
       dstva = va0 + PGSIZE;
     }
     return 0;
   }
   ```

5. add reference count function

   ```c
   // kernel/kalloc.c
   struct {
     struct spinlock lock;
     int rc[PHYSTOP/PGSIZE];      // NPROC * NCPU = 64 * 8 = 512
   } refcount;

   void inc_refcount(uint64 pa) {
     acquire(&refcount.lock);
     ++refcount.rc[PA2IDX(pa)];
     release(&refcount.lock);
   }

   int get_refcount(uint64 pa) {
     return refcount.rc[PA2IDX(pa)];
   }

   void
   kinit()
   {
     initlock(&refcount.lock, "refcount");           // initialization
     memset(&refcount.rc, 0, sizeof(refcount.rc));   // initialization

     initlock(&kmem.lock, "kmem");
     freerange(end, (void*)PHYSTOP);
   }

   void
   freerange(void *pa_start, void *pa_end)
   {
     char *p;
     p = (char*)PGROUNDUP((uint64)pa_start);
     for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
       inc_refcount((uint64)p);     // make it can be free
       kfree(p);
     }
   }

   void
   kfree(void *pa)
   {
     struct run *r;

     if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
       panic("kfree");

     // atomic area
     acquire(&refcount.lock);
     int rc = --refcount.rc[PA2IDX((uint64)pa)];
     release(&refcount.lock);
     if(rc > 0)
       return;
     // atomic area

     // Fill with junk to catch dangling refs.
     memset(pa, 1, PGSIZE);

     r = (struct run*)pa;

     acquire(&kmem.lock);
     r->next = kmem.freelist;
     kmem.freelist = r;
     release(&kmem.lock);
   }

   void *
   kalloc(void)
   {
     struct run *r;

     acquire(&kmem.lock);
     r = kmem.freelist;
     if(r)
       kmem.freelist = r->next;
     release(&kmem.lock);

     if(r){
       inc_refcount((uint64)r);       // increase reference count
       memset((char*)r, 5, PGSIZE);
     }
     return (void*)r;
   }
   ```

6. supply functions used during modifications in head file

   ```c
   // kernel/defs.h

   // kalloc.c
   void            inc_refcount(uint64);
   int             get_refcount(uint64);

   // vm.c
   pte_t*          walk(pagetable_t, uint64, int);

   #define PA2IDX(pa) ((pa)/PGSIZE)

   ```