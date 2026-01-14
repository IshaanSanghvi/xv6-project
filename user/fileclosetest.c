#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(void)
{
  int fd = open("s2.txt", O_CREATE | O_TRUNC | O_RDWR);
  if(fd < 0){
    printf("open failed\n");
    exit(1);
  }

  char *page = malloc(4096);
  if(page == 0){
    printf("malloc failed\n");
    close(fd);
    exit(1);
  }

  for(int i = 0; i < 4096; i++) page[i] = 'A';
  int n = write(fd, page, 4096);
  free(page);

  if(n != 4096){
    printf("write failed: %d\n", n);
    close(fd);
    exit(1);
  }

  void *p = (void*)mmap(0, 4096, 1, MAP_PRIVATE, fd, 0);
  if(p == (void*)-1){
    printf("mmap failed\n");
    close(fd);
    exit(1);
  }

  close(fd);
  printf("OK: mmap=%p, closed fd, did not touch mapping\n", p);
  exit(0);
}
