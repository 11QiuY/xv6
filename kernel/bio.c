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

#define NHASH              29
#define MAX_HASH_BUF_COUNT 10
#define HASH(dev, blockno) (((dev) ^ (blockno)) % NHASH)

struct {
  // struct spinlock lock;
  struct buf buf[NBUF];
  struct {
    struct spinlock hlock;
    struct buf     *bufs[MAX_HASH_BUF_COUNT];
    int             count;
  } hash[NHASH];
} bcache;

// Linked list of all buffers, through prev/next.
// Sorted by how recently the buffer was used.
// head.next is most recent, head.prev is least.
// struct buf head;

void binit(void) {
  struct buf *b;
  int         n3 = NBUF % NHASH;

  // initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // 把每个buf放入对应的hash桶中
  for (int i = 0; i < NHASH; i++) {
    initlock(&bcache.hash[i].hlock, "bcache.hash");
    bcache.hash[i].count = 0;
    if (i < n3) {
      for (int j = 0; j < NBUF / NHASH + 1; j++) {
        b = bcache.buf + i * (NBUF / NHASH + 1) + j;
        initsleeplock(&b->lock, "buffer");
        bcache.hash[i].bufs[j] = b;
        bcache.hash[i].count++;
      }
    } else {
      for (int j = 0; j < NBUF / NHASH; j++) {
        b = bcache.buf + n3 * (NBUF / NHASH + 1) + (i - n3) * (NBUF / NHASH) +
            j;
        initsleeplock(&b->lock, "buffer");
        bcache.hash[i].bufs[j] = b;
        bcache.hash[i].count++;
      }
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;

  int h = HASH(dev, blockno);

  acquire(&bcache.hash[h].hlock);
  int n = bcache.hash[h].count;

  for (int i = 0; i < n; i++) {
    b = bcache.hash[h].bufs[i];
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.hash[h].hlock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (int i = 0; i < n; i++) {
    b = bcache.hash[h].bufs[i];
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.hash[h].hlock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.hash[h].hlock);
  // 从其他地方窃取一个buf
  for (int i = 0; i < NHASH; i++) {
    if (i == h) continue;
    acquire(&bcache.hash[i].hlock);
    n = bcache.hash[i].count;
    for (int j = 0; j < n; j++) {
      b = bcache.hash[i].bufs[j];
      if (b->refcnt == 0) {
        bcache.hash[i].count--;
        for (int k = j; k < bcache.hash[i].count; k++) {
          bcache.hash[i].bufs[k] = bcache.hash[i].bufs[k + 1];
        }
        release(&bcache.hash[i].hlock);
        acquire(&bcache.hash[h].hlock);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        bcache.hash[h].bufs[bcache.hash[h].count++] = b;
        release(&bcache.hash[h].hlock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.hash[i].hlock);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock)) panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock)) panic("brelse");

  releasesleep(&b->lock);

  int h = HASH(b->dev, b->blockno);

  acquire(&bcache.hash[h].hlock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
  }

  release(&bcache.hash[h].hlock);
}

void bpin(struct buf *b) {
  int h = HASH(b->dev, b->blockno);
  acquire(&bcache.hash[h].hlock);
  b->refcnt++;
  release(&bcache.hash[h].hlock);
}

void bunpin(struct buf *b) {
  int h = HASH(b->dev, b->blockno);
  acquire(&bcache.hash[h].hlock);
  b->refcnt--;
  release(&bcache.hash[h].hlock);
}
