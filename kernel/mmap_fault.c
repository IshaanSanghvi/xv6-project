#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmap.h"


static inline int
prot_to_pte(int prot)
{
  int perm = PTE_U;

  if(prot & PROT_WRITE)
    perm |= PTE_W | PTE_R;

  if(prot & PROT_READ)
    perm |= PTE_R;

  if(prot & PROT_EXEC)
    perm |= PTE_X;

  return perm;
}


int
handle_mmap_fault(struct proc *p, uint64 va, uint64 scause)
{
  if(va >= MAXVA)
    return -1;

  uint64 pageva = PGROUNDDOWN(va);

  uint64 v_start, v_off;
  int v_prot;
  struct file *f;

  acquire(&p->lock);
  struct vma *v = vma_find(p, pageva);
  if(v == 0){
    release(&p->lock);
    return -1;
  }

  if(scause == 15){
    if((v->prot & PROT_WRITE) == 0){
      release(&p->lock);
      return -1;
    }
  } else if(scause == 13){
    if((v->prot & PROT_READ) == 0){
      release(&p->lock);
      return -1;
    }
  } else if(scause == 12){
    if((v->prot & PROT_EXEC) == 0){
      release(&p->lock);
      return -1;
    }
  } else {
    release(&p->lock);
    return -1;
  }

  v_start = v->start;
  v_off   = v->off;
  v_prot  = v->prot;
  f = v->f;

  int perm = prot_to_pte(v_prot);
  release(&p->lock);

  pte_t *pte = walk(p->pagetable, pageva, 0);
  if(pte && (*pte & PTE_V))
    return -1;

  char *mem = kalloc();
  if(mem == 0)
    return -1;

  memset(mem, 0, PGSIZE);

  if(f != 0){
    uint64 pageoff = pageva - v_start;
    uint64 fileoff = v_off + pageoff;

    struct inode *ip = f->ip;
    ilock(ip);
    int n = readi(ip, 0, (uint64)mem, (uint)fileoff, PGSIZE);
    iunlock(ip);

    if(n < 0){
      kfree(mem);
      return -1;
    }
  }

  if(mappages(p->pagetable, pageva, PGSIZE, (uint64)mem, perm) != 0){
    kfree(mem);
    return -1;
  }

  return 0;
}

static int
mmap_should_writeback(struct vma *v)
{
  if(v == 0) return 0;
  if(v->f == 0) return 0;
  if((v->flags & MAP_SHARED) == 0) return 0;
  if((v->prot & PROT_WRITE) == 0) return 0;
  if(v->f->type != FD_INODE) return 0;
  return 1;
}

int
mmap_writeback_page(struct proc *p, struct vma *v, uint64 va)
{
  if(!mmap_should_writeback(v))
    return 0;

  uint64 a = PGROUNDDOWN(va);

  if(a < v->start || a >= v->start + v->len)
    return 0;

  pte_t *pte = walk(p->pagetable, a, 0);
  if(pte == 0 || (*pte & PTE_V) == 0)
    return 0;

  if((*pte & PTE_D) == 0)
    return 0;

  uint64 pa = PTE2PA(*pte);

  uint off = v->off + (uint)(a - v->start);

  uint n = PGSIZE;
  uint64 vend = v->start + v->len;
  if(a + PGSIZE > vend)
    n = (uint)(vend - a);

  struct inode *ip = v->f->ip;
  if(ip == 0) return -1;

  begin_op();
  ilock(ip);
  int r = writei(ip, 0 , pa, off, n);
  iunlock(ip);
  end_op();

  if(r < 0)
    return -1;

  *pte &= ~PTE_D;

  return 0;
}
