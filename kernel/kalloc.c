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

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.

struct run {
  struct run *next;
};

struct Kmem {
  struct spinlock lock;
  struct run     *freelist;
};

struct Kmem kmems[NCPU];

int get_cpuid() {
  push_off();
  int cpu_id = cpuid();
  pop_off();
  return cpu_id;
}

void kinit() {
  // initlock(&kmem.lock, "kmem");
  // 为每个CPU初始化一个kmem
  for (int i = 0; i < NCPU; i++) {
    char buf[8];
    snprintf(buf, 8, "kmem%d", i);
    initlock(&kmems[i].lock, "kmem");
  }
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  int cpu_id = get_cpuid();
  r = (struct run *)pa;

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

#define MAX_STEAL_TIMES 0

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  struct run *r;

  int cpu_id = get_cpuid();

  acquire(&kmems[cpu_id].lock);
  r = kmems[cpu_id].freelist;
  if (r)
    kmems[cpu_id].freelist = r->next;
  else {  // 询问其他CPU是否有空闲内存
    for (int i = 0; i < NCPU; i++) {
      int find = 0;
      if (i == cpu_id) continue;
      push_off();
      acquire(&kmems[i].lock);

      r = kmems[i].freelist;
      if (r) {  // 窃取内存
        find = 1;
        kmems[i].freelist = r->next;
        kmems[cpu_id].freelist = r;
        kmems[cpu_id].freelist->next = 0;

        for (int j = 0; j < MAX_STEAL_TIMES - 1;
             j++) {  // 尝试窃取MAX_STEAL_TIMES次

          r = kmems[i].freelist;
          if (r) {
            kmems[i].freelist = r->next;
            r->next = kmems[cpu_id].freelist;
            kmems[cpu_id].freelist = r;
          } else {
            break;
          }
        }
      }
      release(&kmems[i].lock);
      pop_off();
      if (find) break;
    }
    // 在窃取完所有CPU的内存后，再次尝试分配
    r = kmems[cpu_id].freelist;
    if (r) kmems[cpu_id].freelist = r->next;
  }
  release(&kmems[cpu_id].lock);

  if (r) memset((char *)r, 5, PGSIZE);  // fill with junk
  return (void *)r;
}
