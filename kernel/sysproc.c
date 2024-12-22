#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "fcntl.h"
#include "file.h"

extern void mmapunmap(pagetable_t pagetable, uint64 va, uint64 npages,
                      struct mmap_VMA *v);

uint64 sys_exit(void) {
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64 sys_sbrk(void) {
  uint64 addr;
  int    n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int  n;
  uint ticks0;

  argint(0, &n);
  if (n < 0) n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

static struct mmap_VMA *get_vma(uint64 addr, uint64 len, struct proc *p) {
  for (int i = 0; i < MAX_VMA; i++) {
    if (p->vma[i].valid == 0) continue;
    if (p->vma[i].addr <= addr && p->vma[i].addr + p->vma[i].len >= addr) {
      return &p->vma[i];
    }
  }
  return 0;
}

// 从用户地址空间找到一段连续的虚拟内存地址,并且addr与len都要是PGROUNDUP的
static void get_addr_len(uint64 *addr, uint64 len, struct proc *p) {
  // 检查能否向右端插入长度为len的虚拟内存
  uint64 begin = MMAPEND;
  for (int i = 0; i < MAX_VMA; i++) {
    if (p->vma[i].valid == 0) continue;
    uint64 start = p->vma[i].addr;
    uint64 end = start + p->vma[i].len;
    uint64 rightsize = MMAPEND - end;
    begin = begin < start ? begin : start;
    if (rightsize <= len) continue;
    for (int j = 0; j < MAX_VMA; j++) {
      if (p->vma[j].valid == 0 || i == j) continue;
      uint64 start2 = p->vma[j].addr;
      uint64 r;
      if ((r = start2 - end) > 0) {
        rightsize = r < rightsize ? r : rightsize;
      }
    }
    if (rightsize >= len) {
      *addr = end;
      return;
    }
  }
  // 向左插入
  *addr = (begin - len);
}

// void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t
// offset);
uint64 sys_mmap(void) {
  uint64 addr;
  size_t len;
  int    prot;
  int    flags;
  int    fd;
  off_t  offset;

  argaddr(0, &addr);
  argaddr(1, (uint64 *)&len);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argint(5, (int *)&offset);

  struct proc *p = myproc();
  struct file *f = p->ofile[fd];

  if ((!f->readable && (prot & PROT_READ)) ||
      (!f->writable && (prot & PROT_WRITE) && !(flags & MAP_PRIVATE))) {
    return -1;
  }

  // we assume the addr is always 0 which means the kernel will decide the
  // address
  if (addr != 0) panic("mmap: addr is not 0");
  // find a free virtual address space for mmap
  len = PGROUNDUP(len);
  get_addr_len(&addr, len, p);

  for (int i = 0; i < MAX_VMA; i++) {
    if (p->vma[i].valid == 0) {
      p->vma[i].addr = addr;
      p->vma[i].len = len;
      p->vma[i].prot = prot;
      p->vma[i].flags = flags;
      if (f == 0) {
        return -1;
      }
      filedup(f);
      p->vma[i].file = f;
      p->vma[i].offset = offset;
      p->vma[i].valid = 1;
      return addr;
    }
  }
  return -1;
}
// int   munmap(void *addr, size_t len);
uint64 sys_munmap(void) {
  uint64 addr;
  size_t len;

  argaddr(0, &addr);
  argaddr(1, (uint64 *)&len);

  addr = PGROUNDDOWN(addr);
  len = PGROUNDUP(len);

  struct proc *p = myproc();

  struct mmap_VMA *v = get_vma(addr, len, p);
  if (v == 0) {
    printf("addr = %p, len = %d\n", (void *)addr, (int)len);
    printf("v not found\n");
    return -1;
  };
  if (addr > v->addr && addr + len < v->addr + v->len) {
    printf("inside address");
    return -1;
  }

  mmapunmap(p->pagetable, addr, len / PGSIZE, v);

  if (addr == v->addr) {
    v->offset += len;
    v->addr += len;
  }
  v->len -= len;
  if (v->len == 0) {
    fileclose(v->file);
    v->valid = 0;
  }
  return 0;
}
