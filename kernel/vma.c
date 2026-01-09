#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "proc.h"

// Return 1 if [a0, a1) overlaps [b0, b1)
static int
range_overlap(uint64 a0, uint64 a1, uint64 b0, uint64 b1)
{
  return (a0 < b1) && (b0 < a1);
}

// Find a free VMA slot in p->vmas[].
struct vma*
vma_alloc(struct proc *p)
{
  for(int i = 0; i < NVMA; i++){
    if(p->vmas[i].used == 0)
      return &p->vmas[i];
  }
  return 0;
}

// Find the VMA that contains virtual address va.
// Returns VMA* if start <= va < start+len, else 0.
struct vma*
vma_find(struct proc *p, uint64 va)
{
  for(int i = 0; i < NVMA; i++){
    if(p->vmas[i].used){
      uint64 s = p->vmas[i].start;
      uint64 e = s + p->vmas[i].len;
      if(s <= va && va < e)
        return &p->vmas[i];
    }
  }
  return 0;
}

// Return 1 if [start, start+len) overlaps any existing used VMA in p.
int
vma_overlaps(struct proc *p, uint64 start, uint64 len)
{
  uint64 end = start + len;
  for(int i = 0; i < NVMA; i++){
    if(p->vmas[i].used){
      uint64 s = p->vmas[i].start;
      uint64 e = s + p->vmas[i].len;
      if(range_overlap(start, end, s, e))
        return 1;
    }
  }
  return 0;
}
