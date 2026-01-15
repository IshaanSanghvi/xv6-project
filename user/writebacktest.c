// user/mmap_step4_test.c
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
#ifndef PROT_EXEC
#define PROT_EXEC  0x4
#endif

#ifndef MAP_SHARED
#define MAP_SHARED  0x01
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif

static void
die(const char *msg)
{
  printf("FAIL: %s\n", msg);
  exit(1);
}

static void
ok(const char *msg)
{
  printf("OK: %s\n", msg);
}

static void
fill_file(int fd, int nbytes)
{
  char buf[512];
  int off = 0;
  while(off < nbytes){
    int chunk = nbytes - off;
    if(chunk > (int)sizeof(buf)) chunk = sizeof(buf);
    for(int i = 0; i < chunk; i++){
      buf[i] = (char)((off + i) & 0xff);
    }
    if(write(fd, buf, chunk) != chunk)
      die("fill_file write failed");
    off += chunk;
  }
}

static void
read_file(int fd, char *buf, int nbytes)
{

  int off = 0;
  while(off < nbytes){
    int n = read(fd, buf + off, nbytes - off);
    if(n < 0) die("read failed");
    if(n == 0) die("unexpected EOF");
    off += n;
  }
}

static int
same_bytes(const char *a, const char *b, int n)
{
  for(int i = 0; i < n; i++){
    if(a[i] != b[i]) return 0;
  }
  return 1;
}

static void
test_pagein_read(void)
{
  const char *path = "mmap_t0";
  int sz = 2 * 4096;

  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) die("open create failed");
  fill_file(fd, sz);
  close(fd);

  fd = open(path, O_RDWR);
  if(fd < 0) die("open r/w failed");

  char *p = mmap(0, sz, PROT_READ, MAP_SHARED, fd, 0);
  if(p == (char*)-1) die("mmap PROT_READ failed");

  if((unsigned char)p[0] != 0x00) die("page-in mismatch at 0");
  if((unsigned char)p[123] != (123 & 0xff)) die("page-in mismatch at 123");
  if((unsigned char)p[4096] != (4096 & 0xff)) die("page-in mismatch at 4096");
  if((unsigned char)p[4096 + 77] != ((4096 + 77) & 0xff)) die("page-in mismatch at 4096+77");

  munmap(p, sz);
  close(fd);

  ok("file-backed page-in (readi) works for PROT_READ");
}

static void
test_writable_shared_doesnt_crash(void)
{
  const char *path = "mmap_t1";
  int sz = 4096;

  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) die("open create failed");
  fill_file(fd, sz);
  close(fd);

  fd = open(path, O_RDWR);
  if(fd < 0) die("open failed");

  char *p = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(p == (char*)-1) die("mmap shared RW failed");

  p[0] = 'X';
  p[100] = 'Y';

  munmap(p, sz);
  close(fd);

  ok("MAP_SHARED + PROT_WRITE mapping is writable (store succeeds)");
}

static void
test_write_to_readonly_faults(void)
{
  const char *path = "mmap_t2";
  int sz = 4096;

  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) die("open create failed");
  fill_file(fd, sz);
  close(fd);

  int pid = fork();
  if(pid < 0) die("fork failed");

  if(pid == 0){
    fd = open(path, O_RDWR);
    if(fd < 0) die("child open failed");

    char *p = mmap(0, sz, PROT_READ, MAP_SHARED, fd, 0);
    if(p == (char*)-1) die("child mmap failed");

    p[0] = 'Z';

    printf("FAIL: child wrote to PROT_READ mapping without dying\n");
    munmap(p, sz);
    close(fd);
    exit(1);
  }

  int st = 0;
  wait(&st);

  if(st == 0)
    die("write to PROT_READ mapping did NOT fault/kill (unexpected)");

  ok("writing to PROT_READ mapping faults (child killed as expected)");
}

static void
test_shared_writeback_effect(void)
{
  const char *path = "mmap_t3";
  int sz = 4096;

  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) die("open create failed");
  fill_file(fd, sz);
  close(fd);

  char before[64];
  fd = open(path, O_RDONLY);
  if(fd < 0) die("open before failed");
  read_file(fd, before, sizeof(before));
  close(fd);

  fd = open(path, O_RDWR);
  if(fd < 0) die("open rw failed");

  char *p = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(p == (char*)-1) die("mmap shared rw failed");

  p[0] = 'Q';
  p[1] = 'W';
  p[2] = 'E';

  munmap(p, sz);
  close(fd);

  char after[64];
  fd = open(path, O_RDONLY);
  if(fd < 0) die("open after failed");
  read_file(fd, after, sizeof(after));
  close(fd);

  if(after[0] == 'Q' && after[1] == 'W' && after[2] == 'E'){
    ok("MAP_SHARED writeback observed in file (Step 5 appears working)");
  } else {
    printf("NOTE: MAP_SHARED writeback not observed yet (expected until Step 5).\n");
  }
}

static void
test_private_no_writeback(void)
{
  const char *path = "mmap_t4";
  int sz = 4096;

  unlink(path);
  int fd = open(path, O_CREATE | O_RDWR);
  if(fd < 0) die("open create failed");
  fill_file(fd, sz);
  close(fd);

  char before[64];
  fd = open(path, O_RDONLY);
  if(fd < 0) die("open before failed");
  read_file(fd, before, sizeof(before));
  close(fd);

  fd = open(path, O_RDWR);
  if(fd < 0) die("open rw failed");

  char *p = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if(p == (char*)-1) die("mmap private rw failed");

  p[0] = 'Z';
  p[1] = 'Z';

  munmap(p, sz);
  close(fd);

  char after[64];
  fd = open(path, O_RDONLY);
  if(fd < 0) die("open after failed");
  read_file(fd, after, sizeof(after));
  close(fd);

  if(same_bytes(before, after, sizeof(before))){
    ok("MAP_PRIVATE does not modify file (expected behavior)");
  } else {
    printf("FAIL: MAP_PRIVATE modified file (should not).\n");
    exit(1);
  }
}

int
main(void)
{
  printf("=== mmap Step 4/5 test suite ===\n");

  test_pagein_read();
  test_writable_shared_doesnt_crash();
  test_write_to_readonly_faults();

  test_shared_writeback_effect();
  test_private_no_writeback();

  printf("=== done ===\n");
  exit(0);
}
