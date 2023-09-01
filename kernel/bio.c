// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
extern uint ticks;

static int hash(int blockno) {
  return blockno % NBUCKET;
}

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
  struct spinlock headlock[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }

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

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int id = hash(blockno);

  // acquire(&bcache.lock);

  // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
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
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
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

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  // release(&bcache.lock);

  if (--b->refcnt == 0) {
    b->timestamp = ticks;
  }
}

void
bpin(struct buf *b) {
  // acquire(&bcache.lock);
  // b->refcnt++;
  // release(&bcache.lock);
  int id = hash(b->blockno);
  acquire(&bcache.headlock[id]);
  b->refcnt++;
  release(&bcache.headlock[id]);
}

void
bunpin(struct buf *b) {
  // acquire(&bcache.lock);
  // b->refcnt--;
  // release(&bcache.lock);
  int id = hash(b->blockno);
  acquire(&bcache.headlock[id]);
  b->refcnt--;
  release(&bcache.headlock[id]);
}


