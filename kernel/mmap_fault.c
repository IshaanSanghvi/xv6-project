#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

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

  // Look up the VMA that covers this VA.
  acquire(&p->lock);
  struct vma *v = vma_find(p, va);
  if(v == 0){
    release(&p->lock);
    return -1;
  }

  // Permission check based on fault type.
  // 13 = load fault, 15 = store fault, 12 = instruction fault
  if(scause == 15){
    // write fault => require PROT_WRITE (0x2)
    if((v->prot & 0x2) == 0){
      release(&p->lock);
      return -1;
    }
  } else if(scause == 13){
    // read fault => require PROT_READ (0x1)
    if((v->prot & 0x1) == 0){
      release(&p->lock);
      return -1;
    }
  } else if(scause == 12){
    // exec fault => require PROT_EXEC (0x4)
    if((v->prot & 0x4) == 0){
      release(&p->lock);
      return -1;
    }
  } else {
    release(&p->lock);
    return -1;
  }

  int perm = prot_to_pte(v->prot);
  release(&p->lock);

  // If already mapped, this isn't the "missing page" kind of fault.
  pte_t *pte = walk(p->pagetable, pageva, 0);
  if(pte && (*pte & PTE_V))
    return -1;

  // Allocate a physical page.
  char *mem = kalloc();
  if(mem == 0)
    return -1;

  memset(mem, 0, PGSIZE);

  // Map it into user pagetable.
  if(mappages(p->pagetable, pageva, PGSIZE, (uint64)mem, perm) != 0){
    kfree(mem);
    return -1;
  }

  return 0; // handled
}
