#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"


static void
bench_touch_repeat(int npages, int rounds)
{
  int sz = npages * 4096;
  char *p = sbrk(sz);
  if(p == (char*)-1){
    printf("sbrk failed\n");
    return;
  }

  int t0 = uptime();
  for(int r = 0; r < rounds; r++){
    for(int i = 0; i < sz; i += 4096){
      p[i] ^= 1;
    }
  }
  int t1 = uptime();

  printf("[touch] %d pages * %d rounds in %d ticks\n", npages, rounds, t1 - t0);
}


static void
bench_read_repeat(const char *path, int rounds)
{
  char buf[1024];
  uint64 total = 0;

  int t0 = uptime();
  for(int r = 0; r < rounds; r++){
    int fd = open(path, O_RDONLY);
    if(fd < 0){
      printf("open failed: %s\n", path);
      return;
    }

    for(;;){
      int n = read(fd, buf, sizeof(buf));
      if(n <= 0) break;
      total += (uint64)n;
    }
    close(fd);
  }
  int t1 = uptime();

  printf("[read] %llu bytes from %s over %d rounds in %d ticks\n",
         (unsigned long long)total, path, rounds, t1 - t0);
}

int
main(int argc, char **argv)
{
  bench_touch_repeat(2048, 200);
  bench_read_repeat("usertests", 200);
  exit(0);
}
