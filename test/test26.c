/* test26: lseek()		Author: Jan-Mark Wams (jms@cs.vu.nl) */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>

int max_error = 	4;
#include "common.h"

#define ITERATIONS     10

#define System(cmd)   if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)    if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)     if (stat(a,b) != 0) printf("Can't stat %s\n", a)
#define Mkfifo(f)     if (mkfifo(f,0777)!=0) printf("Can't make fifo %s\n", f)


void test26a(void);
void test26b(void);
void test26c(void);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);

  start(26);

  for (i = 0; i < 10; i++) {
	if (m & 0001) test26a();
	if (m & 0002) test26b();
	if (m & 0004) test26c();
  }
  quit();

  return(-1);	/* Unreachable */
}

void test26a()
{				/* Test normal operation. */
  int fd;
  char buf[20];
  int i, j;
  struct stat st;

  subtest = 1;
  System("rm -rf ../DIR_26/*");

  System("echo -n hihaho > hihaho");
  if ((fd = open("hihaho", O_RDONLY)) != 3) e(1);
  if (lseek(fd, (off_t) 3, SEEK_SET) != (off_t) 3) e(2);
  if (read(fd, buf, 1) != 1) e(3);
  if (buf[0] != 'a') e(4);
  if (lseek(fd, (off_t) - 1, SEEK_END) != 5) e(5);
  if (read(fd, buf, 1) != 1) e(6);
  if (buf[0] != 'o') e(7);

  /* Seek past end of file. */
  if (lseek(fd, (off_t) 1000, SEEK_END) != 1006) e(8);
  if (read(fd, buf, 1) != 0) e(9);

  /* Lseek() should not extend the file. */
  if (fstat(fd, &st) != 0) e(10);
  if (st.st_size != (off_t) 6) e(11);
  if (close(fd) != 0) e(12);

  /* Probeer lseek met write. */
  if ((fd = open("hihaho", O_WRONLY)) != 3) e(13);
  if (lseek(fd, (off_t) 3, SEEK_SET) != (off_t) 3) e(14);
  if (write(fd, "e", 1) != 1) e(15);
  if (lseek(fd, (off_t) 1000, SEEK_END) != 1006) e(16);

  /* Lseek() should not extend the file. */
  if (fstat(fd, &st) != 0) e(17);
  if (st.st_size != (off_t) 6) e(18);
  if (write(fd, "e", 1) != 1) e(19);

  /* Lseek() and a subsequent write should! */
  if (fstat(fd, &st) != 0) e(20);
  if (st.st_size != (off_t) 1007) e(21);

  if (close(fd) != 0) e(22);

  /* Check the file, it should start with hiheho. */
  if ((fd = open("hihaho", O_RDONLY)) != 3) e(23);
  if (read(fd, buf, 6) != 6) e(24);
  if (strncmp(buf, "hiheho", 6) != 0) e(25);

  /* The should be zero bytes and a trailing ``e''. */
  if (sizeof(buf) < 10) e(26);
  for (i = 1; i <= 20; i++) {
	if (read(fd, buf, 10) != 10) e(27);
	for (j = 0; j < 10; j++)
		if (buf[j] != '\0') break;
	if (j != 10) e(28);
	if (lseek(fd, (off_t) 15, SEEK_CUR) != (off_t) i * 25 + 6) e(29);
  }

  if (lseek(fd, (off_t) 1006, SEEK_SET) != (off_t) 1006) e(30);
  if (read(fd, buf, sizeof(buf)) != 1) e(31);
  if (buf[0] != 'e') e(32);

  if (lseek(fd, (off_t) - 1, SEEK_END) != (off_t) 1006) e(33);
  if (read(fd, buf, sizeof(buf)) != 1) e(34);
  if (buf[0] != 'e') e(35);

  /* Closing time. */
  if (close(fd) != 0) e(36);
}

void test26b()
{
  int fd1, fd2, fd3;
  int stat_loc;

  subtest = 2;
  System("rm -rf ../DIR_26/*");

  /* See if childs lseek() is effecting the parent. * See also if
   * lseeking() on same file messes things up. */

  /* Creat a file of 11 bytes. */
  if ((fd1 = open("santa", O_WRONLY | O_CREAT, 0777)) != 3) e(1);
  if (write(fd1, "ho ho ho ho", 11) != 11) e(2);
  if (close(fd1) != 0) e(3);

  /* Open it multiple times. */
  if ((fd1 = open("santa", O_RDONLY)) != 3) e(4);
  if ((fd2 = open("santa", O_WRONLY)) != 4) e(5);
  if ((fd3 = open("santa", O_RDWR)) != 5) e(6);

  /* Set all offsets different. */
  if (lseek(fd1, (off_t) 2, SEEK_SET) != 2) e(7);
  if (lseek(fd2, (off_t) 4, SEEK_SET) != 4) e(8);
  if (lseek(fd3, (off_t) 7, SEEK_SET) != 7) e(9);

  /* Have a child process do additional offset changes. */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	if (lseek(fd1, (off_t) 1, SEEK_CUR) != 3) e(10);
	if (lseek(fd2, (off_t) 5, SEEK_SET) != 5) e(11);
	if (lseek(fd3, (off_t) - 4, SEEK_END) != 7) e(12);
	exit(0);
      default:
	wait(&stat_loc);
	if (stat_loc != 0) e(13);	/* Alarm? */
  }

  /* Check if the new offsets are correct. */
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 3) e(14);
  if (lseek(fd2, (off_t) 0, SEEK_CUR) != 5) e(15);
  if (lseek(fd3, (off_t) 0, SEEK_CUR) != 7) e(16);

  /* Close the file. */
  if (close(fd1) != 0) e(17);
  if (close(fd2) != 0) e(18);
  if (close(fd3) != 0) e(19);
}

void test26c()
{				/* Test error returns. */
  int fd;
  int tube[2];
  int i, stat_loc;

  subtest = 3;
  System("rm -rf ../DIR_26/*");

  /* Fifo's can't be lseeked(). */
  Mkfifo("fifo");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(3);		/* Try for max 3 secs. */
	if ((fd = open("fifo", O_RDONLY)) != 3) e(1);
	if (lseek(fd, (off_t) 0, SEEK_SET) != (off_t) - 1) e(2);
	if (errno != ESPIPE) e(3);
	if (close(fd) != 0) e(4);
	exit(0);
      default:
	if ((fd = open("fifo", O_WRONLY)) != 3) e(5);
	wait(&stat_loc);
	if (stat_loc != 0) e(6);/* Alarm? */
	if (close(fd) != 0) e(7);
  }

  /* Pipes can't be lseeked() eigther. */
  if (pipe(tube) != 0) e(8);
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(3);		/* Max 3 sconds wait. */
	if (lseek(tube[0], (off_t) 0, SEEK_SET) != (off_t) - 1) e(9);
	if (errno != ESPIPE) e(10);
	if (lseek(tube[1], (off_t) 0, SEEK_SET) != (off_t) - 1) e(11);
	if (errno != ESPIPE) e(12);
	exit(0);
      default:
	wait(&stat_loc);
	if (stat_loc != 0) e(14);	/* Alarm? */
  }

  /* Close the pipe. */
  if (close(tube[0]) != 0) e(15);
  if (close(tube[1]) != 0) e(16);

  /* Whence arument invalid. */
  System("echo -n contact > file");
  if ((fd = open("file", O_RDWR)) != 3) e(17);
  for (i = -1000; i < 1000; i++) {
	if (i == SEEK_SET || i == SEEK_END || i == SEEK_CUR) continue;
	if (lseek(fd, (off_t) 0, i) != (off_t) -1) e(18);
	if (errno != EINVAL) e(19);
  }
  if (close(fd) != 0) e(20);

  /* EBADF for bad fides. */
  for (i = -1000; i < 1000; i++) {
	if (i >= 0 && i < OPEN_MAX) continue;
	if (lseek(i, (off_t) 0, SEEK_SET) != (off_t) - 1) e(21);
	if (lseek(i, (off_t) 0, SEEK_END) != (off_t) - 1) e(22);
	if (lseek(i, (off_t) 0, SEEK_CUR) != (off_t) - 1) e(23);
  }
}

