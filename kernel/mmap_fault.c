#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "proc.h"

#include "sleeplock.h"
#include "fs.h"
#include "file.h"

#include "mmap.h"


static int
prot_to_pte(int prot)
{
  int perm = PTE_U;   // user accessible

  // You must match these PROT_* values to whatever you use in sys_mmap.
  // If you haven't defined them, define them consistently (see note below).
  if(prot & 0x1) perm |= PTE_R; // PROT_READ
  if(prot & 0x2) perm |= PTE_W; // PROT_WRITE
  if(prot & 0x4) perm |= PTE_X; // PROT_EXEC

  return perm;
}

int
handle_mmap_fault(struct proc *p, uint64 va, uint64 scause)
{
  // Only handle user addresses.
  if(va >= MAXVA)
    return -1;

  uint64 pageva = PGROUNDDOWN(va);

  // Find VMA and snapshot fields we need (don't hold p->lock during disk I/O).
  uint64 v_start, v_off;
  int v_prot;
  struct file *f;

  acquire(&p->lock);
  struct vma *v = vma_find(p, pageva);
  if(v == 0){
    release(&p->lock);
    return -1;
  }

  // Permission check based on fault type.
  // 13 = load fault, 15 = store fault, 12 = instruction fault
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

  // If already mapped, this isn't the "missing page" kind of fault.
  pte_t *pte = walk(p->pagetable, pageva, 0);
  if(pte && (*pte & PTE_V))
    return -1;

  // Allocate a physical page.
  char *mem = kalloc();
  if(mem == 0)
    return -1;

  // Zero-fill first so short reads leave zero tail.
  memset(mem, 0, PGSIZE);

  // File-backed? page-in from file.
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
    // if n < PGSIZE, remainder stays zero due to memset above
  }

  // Map it into user pagetable.
  if(mappages(p->pagetable, pageva, PGSIZE, (uint64)mem, perm) != 0){
    kfree(mem);
    return -1;
  }

  return 0; // handled
}
