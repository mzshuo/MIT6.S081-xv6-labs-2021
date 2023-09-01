## Lab: locks

#### 1. memory allocator (moderate)

> Implement per-CPU freelists, and stealing when a CPU's free list is empty.

1. allocate `struct kmem` for each cpu

   ```c
   // kernel/kalloc.c
   struct {
     struct spinlock lock;
     struct run *freelist;
   } kmem[NCPU];
   ```

   initialize each lock separately

   ```c
   // kernel/kalloc.c
   void
   kinit()
   {
     // initlock(&kmem.lock, "kmem");
     for (int i = 0; i < NCPU; ++i) {
       char lockname[6];
       snprintf(lockname, 5, "kmem%d", i);
       initlock(&kmem[i].lock, lockname);
     }
     freerange(end, (void*)PHYSTOP);
   }
   ```

2. redesign `kalloc` and `free` using lock

   ```c
   // kernel/kalloc.c
   void
   kfree(void *pa)
   {
     struct run *r;

     if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
       panic("kfree");

     // Fill with junk to catch dangling refs.
     memset(pa, 1, PGSIZE);

     r = (struct run*)pa;

     push_off();
     int cpu = cpuid();

     acquire(&kmem[cpu].lock);
     r->next = kmem[cpu].freelist;
     kmem[cpu].freelist = r;
     release(&kmem[cpu].lock);
     pop_off();
   }
   ```
   
   ```c
   // kernel/kalloc.c
   void *
   kalloc(void)
   {
     struct run *r;

     push_off();
     int cpu = cpuid();

     acquire(&kmem[cpu].lock);
     r = kmem[cpu].freelist;
     if(r){
       kmem[cpu].freelist = r->next;
       release(&kmem[cpu].lock);
     } else {
       release(&kmem[cpu].lock);
       for (int i = 0; i < NCPU && i != cpu; ++i) {
         acquire(&kmem[i].lock);
         r = kmem[i].freelist;
         if(r){
           kmem[i].freelist = r->next;
           release(&kmem[i].lock);
           break;
         }
         release(&kmem[i].lock);
       }
     }
     pop_off();

     if(r)
       memset((char*)r, 5, PGSIZE); // fill with junk
     return (void*)r;
   }
   ```
