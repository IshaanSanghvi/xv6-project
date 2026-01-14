#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "proc.h"

uint64
sys_mmap(void)
{
  uint64 uaddr;
  int length, prot, flags, fd, offset;

  argaddr(0, &uaddr);
  argint(1, &length);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argint(5, &offset);

  if(uaddr != 0)            return (uint64)-1;
  if(length <= 0)           return (uint64)-1;
  if(offset % PGSIZE != 0)  return (uint64)-1;

  struct proc *p = myproc();

  uint64 len = PGROUNDUP((uint64)length);
  if(len == 0) return (uint64)-1;

  // Choose mapping start at the end of current heap
  uint64 start = PGROUNDUP(p->sz);

  // overflow + user VA range checks
  if(start + len < start) return (uint64)-1;
  if(start + len >= MAXVA) return (uint64)-1;

  acquire(&p->lock);

  // ensure no overlap with existing VMAs
  if(vma_overlaps(p, start, len)){
    release(&p->lock);
    return (uint64)-1;
  }

  struct vma *v = vma_alloc(p);
  if(v == 0){
    release(&p->lock);
    return (uint64)-1;
  }

  v->used  = 1;
  v->start = start;
  v->len   = len;
  v->prot  = prot;
  v->flags = flags;
  v->off   = (uint64)offset;
  v->f     = 0;

  // reserve VA space by bumping sz
  p->sz = start + len;

  release(&p->lock);
  return start;
}

uint64
sys_munmap(void)
{
  uint64 uaddr;
  int length;

  argaddr(0, &uaddr);
  argint(1, &length);

  if(length <= 0) return (uint64)-1;
  if(uaddr % PGSIZE != 0) return (uint64)-1;

  uint64 len = PGROUNDUP((uint64)length);
  if(len == 0) return (uint64)-1;

  struct proc *p = myproc();

  acquire(&p->lock);

  // find a VMA whose start exactly matches uaddr
  struct vma *v = 0;
  for(int i = 0; i < NVMA; i++){
    if(p->vmas[i].used && p->vmas[i].start == uaddr){
      v = &p->vmas[i];
      break;
    }
  }

  if(v == 0){
    release(&p->lock);
    return (uint64)-1;
  }

  if(v->len != len){
    release(&p->lock);
    return (uint64)-1;
  }

  uint64 npages = v->len / PGSIZE;
  uvmunmap(p->pagetable, v->start, npages, 1);
  

  v->used  = 0;
  v->start = 0;
  v->len   = 0;
  v->prot  = 0;
  v->flags = 0;
  v->off   = 0;
  v->f     = 0;

  release(&p->lock);
  return 0;
}
