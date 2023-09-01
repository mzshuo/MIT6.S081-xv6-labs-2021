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
       initlock(&kmem[i].lock, "kmem");
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
       for (int i = 0; i < NCPU; ++i) {
         if (i == cpu) 
           continue;
           
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

#### 2. buffer cache (hard)
> Modify the block cache so that the number of acquire loop iterations for all locks in the bcache is close to zero when running bcachetest.

1. add `timestamp` to `struct buf`

   ```c
   // kernel/buf.h
   struct buf {
     // ...
     uint timestamp;
   };
   ```

2. modify design of buffer cache, hashing each buffer to a bucket

   ```c
   // kernel/bio.c
   #define NBUCKET 13
   extern uint ticks;

   static int hash(int blockno) {
     return blockno % NBUCKET;
   }

   struct {
     struct spinlock lock;
     struct buf buf[NBUF];

     struct buf head[NBUCKET];
     struct spinlock headlock[NBUCKET];
   } bcache;
   ```

   modify initialization function

   ```c
   // kernel/bio.c
   void
   binit(void)
   {
     struct buf *b;
     initlock(&bcache.lock, "bcache");

     for (int i = 0; i < NBUCKET; ++i) {
       initlock(&bcache.headlock[i], "bcache");
       bcache.head[i].prev = &bcache.head[i];
       bcache.head[i].next = &bcache.head[i];
     }

     for (b = bcache.buf; b < bcache.buf + NBUF; ++b) {
       int id = hash(b->blockno);
       b->next = bcache.head[id].next;
       b->prev = &bcache.head[id];
       b->timestamp = ticks;
       initsleeplock(&b->lock, "buffer");
       bcache.head[id].next->prev = b;
       bcache.head[id].next = b;
     }
   }
   ```

3. modify `bget()` function

   ```c
   // kernel/bio.c
   // Look through buffer cache for block on device dev.
   // If not found, allocate a buffer.
   // In either case, return locked buffer.
   static struct buf*
   bget(uint dev, uint blockno)
   {
     struct buf *b;
     int id = hash(blockno);

     // Is the block already cached?
     acquire(&bcache.headlock[id]);
     for (b = bcache.head[id].next; b != &bcache.head[id]; b = b->next) {
       if (b->dev == dev && b->blockno == blockno) {
         b->refcnt++;
         release(&bcache.headlock[id]);
         acquiresleep(&b->lock);
         return b;
       }
     }

     // Not cached.
     uint mintimestamp = ~(uint)0;
     b = 0;
     acquire(&bcache.lock);  // the whole allocating process must be atomic to avoid the same free buffer is allocated by two threads
     for (int i = 0; i < NBUCKET; ++i) {
       for (struct buf *tb = bcache.head[i].next; tb != &bcache.head[i]; tb = tb->next) {
         if (tb->refcnt == 0 && tb->timestamp < mintimestamp) {    // if there is no lock here, b maybe read by another thread and then be overwritten by this thread
           mintimestamp = tb->timestamp;
           b = tb;
         }
       }
     }

     if (!b)
       panic("bget: no buffers");
     b->refcnt = 1;
     release(&bcache.lock);

     int oldid = hash(b->blockno);
     b->dev = dev;
     b->blockno = blockno;
     b->valid = 0;

     if (oldid != id) {
       acquire(&bcache.lock);
       b->prev->next = b->next;
       b->next->prev = b->prev;

       b->next = bcache.head[id].next;
       b->prev = &bcache.head[id];
       bcache.head[id].next->prev = b;
       bcache.head[id].next = b;
       release(&bcache.lock);
     }

     release(&bcache.headlock[id]);    // searching and allocating must be atomic to avoid allocate more than one buffer to the same block 
     acquiresleep(&b->lock);
     return b;
   }
   ```
   modify `brelse()` function

   ```c
   oid
   brelse(struct buf *b)
   {
     if(!holdingsleep(&b->lock))
       panic("brelse");

     releasesleep(&b->lock);

     if (--b->refcnt == 0) {
       b->timestamp = ticks;
     }
   }
   ```

4. modify `bpin()` and `bunpin()`

   ```c
   // kernel/bio.c
   void
   bpin(struct buf *b) {
     int id = hash(b->blockno);
     acquire(&bcache.headlock[id]);
     b->refcnt++;
     release(&bcache.headlock[id]);
   }

   void
   bunpin(struct buf *b) {
     int id = hash(b->blockno);
     acquire(&bcache.headlock[id]);
     b->refcnt--;
     release(&bcache.headlock[id]);
   }
   ```