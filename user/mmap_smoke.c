#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  void *a = mmap(0, 4096, 0, 0, -1, 0);
  printf("mmap -> %p\n", a);

  if(a == (void*)-1){
    printf("mmap failed\n");
    exit(1);
  }

  int r = munmap(a, 4096);
  printf("munmap -> %d\n", r);

  exit(0);
}
