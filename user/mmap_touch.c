#include "kernel/types.h"
#include "user/user.h"

#define PROT_READ  1
#define PROT_WRITE 2

int
main(void)
{
  char *a = mmap(0, 4096, PROT_READ|PROT_WRITE, 0, -1, 0);
  if(a == (char*)-1){
    printf("mmap failed\n");
    exit(1);
  }

  a[0] = 42;               // should page fault and be handled
  printf("a[0]=%d\n", a[0]);

  printf("munmap=%d\n", munmap(a, 4096));
  exit(0);
}
