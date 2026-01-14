#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(void)
{
  int fd = open("mread.txt", O_CREATE | O_TRUNC | O_RDWR);
  if(fd < 0){ printf("open failed\n"); exit(1); }

  // Write one page: predictable pattern
  char *buf = malloc(4096);
  if(buf == 0){ printf("malloc failed\n"); close(fd); exit(1); }

  for(int i = 0; i < 4096; i++)
    buf[i] = (char)('A' + (i % 26));

  if(write(fd, buf, 4096) != 4096){
    printf("write failed\n");
    free(buf);
    close(fd);
    exit(1);
  }
  free(buf);

  void *p = (void*)mmap(0, 4096, 1, MAP_PRIVATE, fd, 0);
  if(p == (void*)-1){ printf("mmap failed\n"); close(fd); exit(1); }

  close(fd); // important: proves filedup works + page-in works after close

  char *x = (char*)p;
  // Touch bytes to trigger load faults and verify contents
  if(x[0] != 'A' || x[1] != 'B' || x[25] != 'Z' || x[26] != 'A'){
    printf("BAD: got %c %c %c %c\n", x[0], x[1], x[25], x[26]);
    exit(1);
  }

  printf("OK mmap file read\n");
  exit(0);
}
