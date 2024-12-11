// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int * refcount;
int pages;

uint64 get_index(void *pa)
{
  return (uint64)(pa - (void *)end) / PGSIZE;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  int refcount_size = (PHYSTOP - (uint64)end) / (PGSIZE / sizeof(int) + 1);
  pages = refcount_size / PGSIZE + 1;
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end )
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  // allocate refcount pages
  refcount = (int*)p;
  p += (pages) * PGSIZE;
  memset(refcount, 0, pages * PGSIZE);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int index = get_index(pa);
  if(refcount[index] > 1){
    refcount[index] -= 1;
    return;
  }
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  refcount[index] = 0;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
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
    memset((char*)r, 5, PGSIZE); // fill with junk
    int index = get_index((void*)r);
    refcount[index] = 1;
  }

  return (void*)r;
}

void 
add_ref(void *pa)
{
  int index = get_index(pa);
  refcount[index] += 1;
}