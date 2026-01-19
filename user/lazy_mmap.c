// user/lazy_mmap.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#ifndef PROT_READ
#define PROT_READ  0x1
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif

static void die(const char *msg) { printf("FAIL: %s\n", msg); exit(1); }
static int ticks_now(void) { return uptime(); }
static volatile int sink;

static int
try_make_file(const char *path, int bytes)
{
  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) return -1;

  char buf[512];
  for(int i = 0; i < (int)sizeof(buf); i++)
    buf[i] = (char)(i & 0xff);

  int left = bytes;
  while(left > 0){
    int n = left > (int)sizeof(buf) ? (int)sizeof(buf) : left;
    int w = write(fd, buf, n);
    if(w != n){
      close(fd);
      unlink(path);
      return -1;
    }
    left -= n;
  }
  close(fd);
  return 0;
}

static void
touch_some_pages(char *p, int npages, int count)
{
  if(count > npages) count = npages;
  int stride = npages / count;
  if(stride < 1) stride = 1;

  for(int i = 0; i < npages && count > 0; i += stride, count--){
    sink += p[i * 4096];
  }
}

static void
touch_all_pages(char *p, int npages)
{
  for(int i = 0; i < npages; i++)
    sink += p[i * 4096];
}

int
main(int argc, char **argv)
{
  int req_pages = 256;   // requested pages (will shrink if FS too small)
  int subset = 8;

  if(argc >= 2){
    req_pages = atoi(argv[1]);
    if(req_pages <= 0) die("pages must be > 0");
  }
  if(argc >= 3){
    subset = atoi(argv[2]);
    if(subset <= 0) die("subset must be > 0");
  }

  const char *path = "mmap_bench_file";

  printf("=== mmap lazy vs eager bench ===\n");
  printf("requested pages=%d, lazy touches=%d pages\n", req_pages, subset);

  int pages = req_pages;
  while(pages > 0){
    int bytes = pages * 4096;
    if(try_make_file(path, bytes) == 0){
      req_pages = pages;
      printf("using pages=%d (bytes=%d)\n", req_pages, bytes);
      break;
    }
    pages /= 2;
  }
  if(pages <= 0) die("could not create file");

  int npages = req_pages;
  int bytes = npages * 4096;

  // ---- LAZY ----
  int fd = open(path, O_RDONLY);
  if(fd < 0) die("open ro failed");

  int t0 = ticks_now();
  char *p = mmap(0, bytes, PROT_READ, MAP_PRIVATE, fd, 0);
  if(p == (char*)-1) die("mmap failed (lazy)");
  close(fd);

  touch_some_pages(p, npages, subset);

  if(munmap(p, bytes) < 0) die("munmap failed (lazy)");
  int t1 = ticks_now();
  int lazy_ticks = t1 - t0;

  printf("LAZY : touch %d/%d pages => %d ticks\n", subset, npages, lazy_ticks);

  // ---- EAGER ----
  fd = open(path, O_RDONLY);
  if(fd < 0) die("open ro failed (eager)");

  int t2 = ticks_now();
  p = mmap(0, bytes, PROT_READ, MAP_PRIVATE, fd, 0);
  if(p == (char*)-1) die("mmap failed (eager)");
  close(fd);

  touch_all_pages(p, npages);

  if(munmap(p, bytes) < 0) die("munmap failed (eager)");
  int t3 = ticks_now();
  int eager_ticks = t3 - t2;

  printf("EAGER: touch %d/%d pages => %d ticks\n", npages, npages, eager_ticks);

  // Integer “speedup”: eager/lazy as percent
  if(lazy_ticks == 0){
    printf("speedup: lazy time = 0 ticks (too small to measure at this size)\n");
  } else {
    int pct = (eager_ticks * 100) / lazy_ticks;
    printf("speedup: eager/lazy ~= %d%% (higher means lazy faster)\n", pct);
  }

  printf("note: lazy is faster when K << N (touching few pages).\n");
  printf("=== done (sink=%d) ===\n", sink);
  exit(0);
}
