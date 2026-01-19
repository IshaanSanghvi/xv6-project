// user/munmap_partial_test.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#ifndef PROT_READ
#define PROT_READ  0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 0x2
#endif
#ifndef MAP_SHARED
#define MAP_SHARED  0x01
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif

static void die(const char *msg) { printf("FAIL: %s\n", msg); exit(1); }
static void ok(const char *msg)  { printf("OK: %s\n", msg); }

static void
fill_file(int fd, int nbytes)
{
  char buf[512];
  int off = 0;
  while(off < nbytes){
    int chunk = nbytes - off;
    if(chunk > (int)sizeof(buf)) chunk = sizeof(buf);
    for(int i = 0; i < chunk; i++)
      buf[i] = (char)((off + i) & 0xff);
    if(write(fd, buf, chunk) != chunk) die("fill_file write failed");
    off += chunk;
  }
}

static void
read_exact(int fd, char *buf, int nbytes)
{
  int off = 0;
  while(off < nbytes){
    int n = read(fd, buf + off, nbytes - off);
    if(n < 0) die("read failed");
    if(n == 0) die("unexpected EOF");
    off += n;
  }
}

// child tries to read/write at addr; parent expects child to die if unmapped
static void
expect_fault_on_access(char *addr, int do_write, const char *what)
{
  printf("parent: expect fault (%s): addr=%p op=%s\n",
         what, addr, do_write ? "write" : "read");

  int pid = fork();
  if(pid < 0) die("fork failed");

  if(pid == 0){
    printf("child: about to %s at %p (%s)\n",
           do_write ? "write" : "read", addr, what);

    volatile char x = addr[0];
    (void)x;

    if(do_write){
      addr[0] = 'Z';
    }

    printf("child: SURVIVED access (BUG): %s\n", what);
    exit(1);
  }

  int st = 0;
  wait(&st);
  printf("parent: wait returned st=%d (%s)\n", st, what);

  if(st == 0) die("expected child to die, but it exited cleanly");
  ok(what);
}

static void
test_shrink_right(void)
{
  const char *path = "pmunmap_r";
  int sz = 2 * 4096;

  printf("\n--- test_shrink_right ---\n");

  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) die("open create failed");
  printf("created %s fd=%d\n", path, fd);
  fill_file(fd, sz);
  close(fd);

  fd = open(path, O_RDWR);
  if(fd < 0) die("open rw failed");
  printf("reopened %s fd=%d\n", path, fd);

  char *p = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  printf("mmap -> %p (sz=%d)\n", p, sz);
  if(p == (char*)-1) die("mmap failed");

  printf("touching both pages...\n");
  p[0] = 'A';
  p[4096] = 'B';

  printf("munmap shrink-right: addr=%p len=%d\n", p + 4096, 4096);
  int r = munmap(p + 4096, 4096);
  printf("munmap returned %d\n", r);
  if(r < 0) die("munmap shrink-right failed");

  printf("writing first page after shrink-right...\n");
  p[0] = 'C';

  expect_fault_on_access(p + 4096, 0, "shrink-right: accessing unmapped right page faults");

  printf("cleanup remaining mapping: munmap(%p,%d)\n", p, 4096);
  r = munmap(p, 4096);
  printf("cleanup munmap returned %d\n", r);
  if(r < 0) die("munmap cleanup failed");

  close(fd);
  ok("shrink-right: left page remains mapped");
}

static void
test_shrink_left_and_offset(void)
{
  const char *path = "pmunmap_l";
  int sz = 2 * 4096;

  printf("\n--- test_shrink_left_and_offset ---\n");

  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) die("open create failed");
  printf("created %s fd=%d\n", path, fd);
  fill_file(fd, sz);
  close(fd);

  fd = open(path, O_RDWR);
  if(fd < 0) die("open rw failed");
  printf("reopened %s fd=%d\n", path, fd);

  char *p = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  printf("mmap -> %p (sz=%d)\n", p, sz);
  if(p == (char*)-1) die("mmap failed");

  printf("touching both pages to fault them in...\n");
  (void)p[0];
  (void)p[4096];

  printf("munmap shrink-left: addr=%p len=%d\n", p, 4096);
  int r = munmap(p, 4096);
  printf("munmap returned %d\n", r);
  if(r < 0) die("munmap shrink-left failed");

  expect_fault_on_access(p, 0, "shrink-left: accessing unmapped left page faults");

  printf("checking remaining page content at p+4096=%p ...\n", p + 4096);
  unsigned char b = (unsigned char)p[4096];
  printf("byte read = %d (0x%x), expected %d (0x%x)\n",
         b, b, (4096 & 0xff), (4096 & 0xff));
  if(b != (4096 & 0xff)) die("shrink-left: remaining page content mismatch (offset shift wrong?)");

  printf("writing 'Q' to remaining page and unmapping it...\n");
  p[4096] = 'Q';

  printf("munmap remaining page: addr=%p len=%d\n", p + 4096, 4096);
  r = munmap(p + 4096, 4096);
  printf("munmap returned %d\n", r);
  if(r < 0) die("munmap remaining page failed");

  close(fd);

  printf("verifying file offset 4096 updated...\n");
  fd = open(path, O_RDONLY);
  if(fd < 0) die("open ro failed");

  char buf[4096 + 1];
  read_exact(fd, buf, 4096 + 1);
  close(fd);

  printf("file[4096]=%d (0x%x) expected 'Q'=%d (0x%x)\n",
         (unsigned char)buf[4096], (unsigned char)buf[4096],
         (unsigned char)'Q', (unsigned char)'Q');

  if(buf[4096] != 'Q') die("shrink-left: writeback went to wrong file offset (off shift bug)");
  ok("shrink-left: remaining page uses shifted file offset + writeback correct");
}

static void
test_middle_split_rejected(void)
{
  const char *path = "pmunmap_mid";
  int sz = 3 * 4096;

  printf("\n--- test_middle_split_rejected ---\n");

  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) die("open create failed");
  printf("created %s fd=%d\n", path, fd);
  fill_file(fd, sz);
  close(fd);

  fd = open(path, O_RDWR);
  if(fd < 0) die("open rw failed");
  printf("reopened %s fd=%d\n", path, fd);

  char *p = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  printf("mmap -> %p (sz=%d)\n", p, sz);
  if(p == (char*)-1) die("mmap failed");

  printf("attempting middle munmap: addr=%p len=%d (should return -1 for Level 1)\n",
         p + 4096, 4096);
  int r = munmap(p + 4096, 4096);
  printf("middle munmap returned %d\n", r);
  if(r != -1) die("middle split was not rejected (expected -1 for Level 1)");

  printf("cleanup whole mapping: tail then head...\n");
  r = munmap(p + 8192, 4096);
  printf("cleanup tail returned %d\n", r);
  if(r < 0) die("cleanup tail failed");

  r = munmap(p, 8192);
  printf("cleanup head returned %d\n", r);
  if(r < 0) die("cleanup head failed");

  close(fd);
  ok("middle-split is rejected (Level 1 behavior)");
}

int
main(void)
{
  printf("=== Step 6 partial munmap (Level 1) tests (debug build) ===\n");

  printf(">>> start shrink-right\n");
  test_shrink_right();
  printf(">>> done shrink-right\n");

  printf(">>> start shrink-left\n");
  test_shrink_left_and_offset();
  printf(">>> done shrink-left\n");

  printf(">>> start middle-reject\n");
  test_middle_split_rejected();
  printf(">>> done middle-reject\n");

  printf("=== done ===\n");
  exit(0);
}
