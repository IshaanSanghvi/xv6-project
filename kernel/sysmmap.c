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

  int is_shared  = (flags & MAP_SHARED) != 0;
  int is_private = (flags & MAP_PRIVATE) != 0;
  if(is_shared == is_private) return (uint64)-1;

  struct proc *p = myproc();
  struct file *f = 0;

  if(fd != -1){
    if(fd < 0 || fd >= NOFILE) return (uint64)-1;
  }

  uint64 len = PGROUNDUP((uint64)length);
  if(len == 0) return (uint64)-1;

  uint64 start = PGROUNDUP(p->sz);

  if(start + len < start) return (uint64)-1;
  if(start + len >= MAXVA) return (uint64)-1;

  acquire(&p->lock);

  if(fd != -1){
    if(p->ofile[fd] == 0){
      release(&p->lock);
      return (uint64)-1;
    }
    f = p->ofile[fd];
    filedup(f);
  }

  if(vma_overlaps(p, start, len)){
    if(f) fileclose(f);
    release(&p->lock);
    return (uint64)-1;
  }

  struct vma *v = vma_alloc(p);
  if(v == 0){
    if(f) fileclose(f);
    release(&p->lock);
    return (uint64)-1;
  }

  v->used  = 1;
  v->start = start;
  v->len   = len;
  v->prot  = prot;
  v->flags = flags;
  v->off   = (uint64)offset;
  v->f     = f;

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

  uint64 a0 = uaddr;
  uint64 a1 = uaddr + len;
  if(a1 < a0) return (uint64)-1; 

  struct proc *p = myproc();

  struct vma *v = 0;
  uint64 v0 = 0, v1 = 0, v_off0 = 0;
  int v_prot = 0, v_flags = 0;
  struct file *f = 0;

  acquire(&p->lock);

  for(int i = 0; i < NVMA; i++){
    if(p->vmas[i].used){
      uint64 s = p->vmas[i].start;
      uint64 e = s + p->vmas[i].len;
      if(s <= a0 && a0 < e){
        v = &p->vmas[i];
        v0 = s;
        v1 = e;
        break;
      }
    }
  }

  if(v == 0){
    release(&p->lock);
    return (uint64)-1;
  }

  if(a1 > v1){
    release(&p->lock);
    return (uint64)-1;
  }

  int whole  = (a0 == v0 && a1 == v1);
  int left   = (a0 == v0 && a1 <  v1);
  int right  = (a0 >  v0 && a1 == v1);

  if(!(whole || left || right)){
    release(&p->lock);
    return (uint64)-1;
  }

  v_off0  = v->off;
  v_prot  = v->prot;
  v_flags = v->flags;
  f       = v->f;

  if(whole){
    v->used  = 0;
    v->start = 0;
    v->len   = 0;
    v->prot  = 0;
    v->flags = 0;
    v->off   = 0;
    v->f     = 0;
  } else if(left){
    uint64 delta = a1 - v0;
    v->start = a1;
    v->len   = v1 - a1;
    v->off   = v_off0 + delta;
  } else {
    v->len = a0 - v0;
  }

  release(&p->lock);

  int can_wb = 0;
  if(f != 0 && (v_flags & MAP_SHARED) && (v_prot & PROT_WRITE) && f->type == FD_INODE)
    can_wb = 1;

  for(uint64 a = a0; a < a1; a += PGSIZE){
    pte_t *pte = walk(p->pagetable, a, 0);
    if(pte == 0) continue;
    if((*pte & PTE_V) == 0) continue;

    if(can_wb && ((*pte & PTE_D) != 0)){
      uint64 pa = PTE2PA(*pte);
      uint64 fileoff = v_off0 + (a - v0);

      struct inode *ip = f->ip;
      if(ip){
        begin_op();
        ilock(ip);
        int n = writei(ip, 0 , pa, (uint)fileoff, PGSIZE);
        iunlock(ip);
        end_op();

        if(n < 0){
          setkilled(p);
        }
      }
    }

    uvmunmap(p->pagetable, a, 1, 1);
  }

  if(whole && f)
    fileclose(f);

  return 0;
}
