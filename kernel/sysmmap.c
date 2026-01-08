#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"

#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"


uint64
sys_mmap(void)
{
  return (uint64)-1;
}

uint64
sys_munmap(void)
{
  return (uint64)-1;
}
