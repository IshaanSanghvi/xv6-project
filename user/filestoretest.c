#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(void)
{
  const char *path = "mmap_s2_a.txt";

  int fd = open(path, O_CREATE | O_TRUNC | O_RDWR);
  if(fd < 0){
    printf("open failed\n");
    exit(1);
  }

  char msg[] = "hello mmap step2\n";
  if(write(fd, msg, sizeof(msg)) != sizeof(msg)){
    printf("write failed\n");
    close(fd);
    exit(1);
  }

  void *p = (void*)mmap(0, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
  if(p == (void*)-1){
    printf("mmap failed\n");
    close(fd);
    exit(1);
  }

  printf("mmap returned addr=%p\n", p);

  if(munmap(p, 4096) < 0){
    printf("munmap failed\n");
    close(fd);
    exit(1);
  }

  close(fd);
  printf("OK\n");
  exit(0);
}
