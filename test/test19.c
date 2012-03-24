#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_ERROR 3
#define NB 30L
#define NBOUNDS 6

int subtest, passes, pipesigs;
long t1;

char aa[100];
char b[4] = {0, 1, 2, 3}, c[4] = {10, 20, 30, 40}, d[4] = {6, 7, 8, 9};
long bounds[NBOUNDS] = {7, 9, 50, 519, 520, 40000L};
char buff[30000];

#include "common.c"

int main(int argc, char *argv[]);
void test19a(void);
void test19b(void);
void test19c(void);
void test19d(void);
void test19e(void);
void test19f(void);
void test19g(void);
void clraa(void);
void pipecatcher(int s);

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m;

  start(19);

  m = (argc == 2 ? atoi(argv[1]) : 0xFFFF);

  for (i = 0; i < 4; i++) {
	if (m & 0001) test19a();
	if (m & 0002) test19b();
	if (m & 0004) test19c();
	if (m & 0010) test19d();
	if (m & 0020) test19e();
	if (m & 0040) test19f();
	if (m & 0100) test19g();
	passes++;
  }
  quit();
  return(-1);			/* impossible */
}

void test19a()
{
/* Test open with O_CREAT and O_EXCL. */

  int fd;

  subtest = 1;
  
  if ( (fd = creat("T19.a1", 0777)) != 3) e(1);	/* create test file */
  if (close(fd) != 0) e(2);
  if ( (fd = open("T19.a1", O_RDONLY)) != 3) e(3);
  if (close(fd) != 0) e(4);
  if ( (fd = open("T19.a1", O_WRONLY)) != 3) e(5);
  if (close(fd) != 0) e(6);
  if ( (fd = open("T19.a1", O_RDWR)) != 3) e(7);
  if (close(fd) != 0) e(8);

  /* See if O_CREAT actually creates a file. */
  if ( (fd = open("T19.a2", O_RDONLY)) != -1) e(9);	/* must fail */
  if ( (fd = open("T19.a2", O_RDONLY | O_CREAT, 0444)) != 3) e(10);
  if (close(fd) != 0) e(11);
  if ( (fd = open("T19.a2", O_RDONLY)) != 3) e(12);
  if (close(fd) != 0) e(13);
  if ( (fd = open("T19.a2", O_WRONLY)) != -1) e(14);
  if ( (fd = open("T19.a2", O_RDWR)) != -1) e(15);

  /* See what O_CREAT does on an existing file. */
  if ( (fd = open("T19.a2", O_RDONLY | O_CREAT, 0777)) != 3) e(16);
  if (close(fd) != 0) e(17);
  if ( (fd = open("T19.a2", O_RDONLY)) != 3) e(18);
  if (close(fd) != 0) e(19);
  if ( (fd = open("T19.a2", O_WRONLY)) != -1) e(20);
  if ( (fd = open("T19.a2", O_RDWR)) != -1) e(21);

  /* See if O_EXCL works. */
  if ( (fd = open("T19.a2", O_RDONLY | O_EXCL)) != 3) e(22);
  if (close(fd) != 0) e(23);
  if ( (fd = open("T19.a2", O_WRONLY | O_EXCL)) != -1) e(24);
  if ( (fd = open("T19.a3", O_RDONLY | O_EXCL)) != -1) e(25);
  if ( (fd = open("T19.a3", O_RDONLY | O_CREAT | O_EXCL, 0444)) != 3) e(26);
  if (close(fd) != 0) e(27);
  errno = 0;
  if ( (fd = open("T19.a3", O_RDONLY | O_CREAT | O_EXCL, 0444)) != -1) e(28);
  if (errno != EEXIST) e(29);
 
  if (unlink("T19.a1") != 0) e(30);
  if (unlink("T19.a2") != 0) e(31);
  if (unlink("T19.a3") != 0) e(32);
}

void test19b()
{
/* Test open with O_APPEND and O_TRUNC. */

  int fd;

  subtest = 2;
  
  if ( (fd = creat("T19.b1", 0777)) != 3) e(1);	/* create test file */
  if (write(fd, b, 4) != 4) e(2);
  if (close(fd) != 0) e(3);
  clraa();
  if ( (fd = open("T19.b1", O_RDWR | O_APPEND)) != 3) e(4);
  if (read(fd, aa, 100) != 4) e(5);
  if (aa[0] != 0 || aa[1] != 1 || aa[2] != 2 || aa[3] != 3) e(6);
  if (close(fd) != 0) e(7);
  if ( (fd = open("T19.b1", O_RDWR | O_APPEND)) != 3) e(8);
  if (write(fd, b, 4) != 4) e(9);
  if (lseek(fd, 0L, SEEK_SET) != 0L) e(10);
  clraa();
  if (read(fd, aa, 100) != 8) e(11);
  if (aa[4] != 0 || aa[5] != 1 || aa[6] != 2 || aa[7] != 3) e(12);
  if (close(fd) != 0) e(13);

  if ( (fd = open("T19.b1", O_RDWR | O_TRUNC)) != 3) e(14);
  if (read(fd, aa, 100) != 0) e(15);
  if (close(fd) != 0) e(16);

  unlink("T19.b1");
}

void test19c()
{
/* Test program for open(), close(), creat(), read(), write(), lseek(). */

  int i, n, n1, n2;

  subtest = 3;
  if ((n = creat("foop", 0777)) != 3) e(1);
  if ((n1 = creat("foop", 0777)) != 4) e(2);
  if ((n2 = creat("/", 0777)) != -1) e(3);
  if (close(n) != 0) e(4);
  if ((n = open("foop", O_RDONLY)) != 3) e(5);
  if ((n2 = open("nofile", O_RDONLY)) != -1) e(6);
  if (close(n1) != 0) e(7);

  /* N is the only one open now. */
  for (i = 0; i < 2; i++) {
	n1 = creat("File2", 0777);
	if (n1 != 4) {
		printf("creat yielded fd=%d, expected 4\n", n1);
		e(8);
	}
	if ((n2 = open("File2", O_RDONLY)) != 5) e(9);
	if (close(n1) != 0) e(10);
	if (close(n2) != 0) e(11);
  }
  unlink("File2");
  if (close(n) != 0) e(12);

  /* All files closed now. */
  for (i = 0; i < 2; i++) {
	if ((n = creat("foop", 0777)) != 3) e(13);
	if (close(n) != 0) e(14);
	if ((n = open("foop", O_RDWR)) != 3) e(15);

	/* Read/write tests */
	if (write(n, b, 4) != 4) e(16);
	if (read(n, aa, 4) != 0) e(17);
	if (lseek(n, 0L, SEEK_SET) != 0L) e(18);
	if (read(n, aa, 4) != 4) e(19);
	if (aa[0] != 0 || aa[1] != 1 || aa[2] != 2 || aa[3] != 3) e(20);
	if (lseek(n, 0L, SEEK_SET) != 0L) e(21);
	if (lseek(n, 2L, SEEK_CUR) != 2L) e(22);
	if (read(n, aa, 4) != 2) e(23);
	if (aa[0] != 2 || aa[1] != 3 || aa[2] != 2 || aa[3] != 3) e(24);
	if (lseek(n, 2L, SEEK_SET) != 2L) e(25);
	clraa();
	if (write(n, c, 4) != 4) e(26);
	if (lseek(n, 0L, SEEK_SET) != 0L) e(27);
	if (read(n, aa, 10) != 6) e(28);
	if (aa[0] != 0 || aa[1] != 1 || aa[2] != 10 || aa[3] != 20) e(29);
	if (lseek(n, 16L, SEEK_SET) != 16L) e(30);
	if (lseek(n, 2040L, SEEK_END) != 2046L) e(31);
	if (read(n, aa, 10) != 0) e(32);
	if (lseek(n, 0L, SEEK_CUR) != 2046L) e(33);
	clraa();
	if (write(n, c, 4) != 4) e(34);
	if (lseek(n, 0L, SEEK_CUR) != 2050L) e(35);
	if (lseek(n, 2040L, SEEK_SET) != 2040L) e(36);
	clraa();
	if (read(n, aa, 20) != 10) e(37);
	if (aa[0] != 0 || aa[5] != 0 || aa[6] != 10 || aa[9] != 40) e(38);
	if (lseek(n, 10239L, SEEK_SET) != 10239L) e(39);
	if (write(n, d, 2) != 2) e(40);
	if (lseek(n, -2L, SEEK_END) != 10239L) e(41);
	if (read(n, aa, 2) != 2) e(42);
	if (aa[0] != 6 || aa[1] != 7) e(43);
	if (lseek(n, NB * 1024L - 2L, SEEK_SET) != NB * 1024L - 2L) e(44);
	if (write(n, b, 4) != 4) e(45);
	if (lseek(n, 0L, SEEK_SET) != 0L) e(46);
	if (lseek(n, -6L, SEEK_END) != 1024L * NB - 4) e(47);
	clraa();
	if (read(n, aa, 100) != 6) e(48);
	if (aa[0] != 0 || aa[1] != 0 || aa[3] != 1 || aa[4] != 2|| aa[5] != 3)
		e(49);
	if (lseek(n, 20000L, SEEK_SET) != 20000L) e(50);
	if (write(n, c, 4) != 4) e(51);
	if (lseek(n, -4L, SEEK_CUR) != 20000L) e(52);
	if (read(n, aa, 4) != 4) e(53);
	if (aa[0] != 10 || aa[1] != 20 || aa[2] != 30 || aa[3] != 40) e(54);
	if (close(n) != 0) e(55);
	if ((n1 = creat("foop", 0777)) != 3) e(56);
	if (close(n1) != 0) e(57);
	unlink("foop");

  }
}

void test19d()
{
/* Test read. */

  int i, fd, pd[2];
  char bb[100];

  subtest = 4;
  
  for (i = 0; i < 100; i++) bb[i] = i;
  if ( (fd = creat("T19.d1", 0777)) != 3) e(1);	/* create test file */
  if (write(fd, bb, 100) != 100) e(2);
  if (close(fd) != 0) e(3);
  clraa();
  if ( (fd = open("T19.d1", O_RDONLY)) != 3) e(4);
  errno = 1000;
  if (read(fd, aa, 0) != 0) e(5);
  if (errno != 1000) e(6);
  if (read(fd, aa, 100) != 100) e(7);
  if (lseek(fd, 37L, SEEK_SET) != 37L) e(8);
  if (read(fd, aa, 10) != 10) e(9);
  if (lseek(fd, 0L, SEEK_CUR) != 47L) e(10);
  if (read(fd, aa, 100) != 53) e(11);
  if (aa[0] != 47) e(12);
  if (read(fd, aa, 1) != 0) e(13);
  if (close(fd) != 0) e(14);

  /* Read from pipe with no writer open. */
  if (pipe(pd) != 0) e(15);
  if (close(pd[1]) != 0) e(16);
  errno = 2000;
  if (read(pd[0], aa, 1) != 0) e(17);	/* must return EOF */
  if (errno != 2000) e(18);

  /* Read from a pipe with O_NONBLOCK set. */
  if (fcntl(pd[0], F_SETFL, O_NONBLOCK) != 0) e(19);      /* set O_NONBLOCK */
/*
  if (read(pd[0], aa, 1) != -1) e(20);
  if (errno != EAGAIN) e(21);
*/
  if (close(pd[0]) != 0) e(19);
  if (unlink("T19.d1") != 0) e(20);
}

void test19e()
{
/* Test link, unlink, stat, fstat, dup, umask.  */

  int i, j, n, n1, flag;
  char a[255], b[255];
  struct stat s, s1;

  subtest = 5;
  for (i = 0; i < 2; i++) {
	umask(0);

	if ((n = creat("T3", 0702)) < 0) e(1);
	if (link("T3", "newT3") < 0) e(2);
	if ((n1 = open("newT3", O_RDWR)) < 0) e(3);
	for (j = 0; j < 255; j++) a[j] = j;
	if (write(n, a, 255) != 255) e(4);
	if (read(n1, b, 255) != 255) e(5);
	flag = 0;
	for (j = 0; j < 255; j++)
		if (a[j] != b[j]) flag++;
	if (flag) e(6);
	if (unlink("T3") < 0) e(7);
	if (close(n) < 0) e(8);
	if (close(n1) < 0) e(9);
	if ((n1 = open("newT3", O_RDONLY)) < 0) e(10);
	if (read(n1, b, 255) != 255) e(11);
	flag = 0;
	for (j = 0; j < 255; j++)
		if (a[j] != b[j]) flag++;
	if (flag) e(12);

	/* Now check out stat, fstat. */
	if (stat("newT3", &s) < 0) e(13);
	if (s.st_mode != (mode_t) 0100702) e(14);
				/* The cast was because regular modes are
				 * negative :-(.  Anyway, the magic number
				 * should be (S_IFREG | S_IRWXU | S_IWOTH)
				 * for POSIX.
				 */
	if (s.st_nlink != 1) e(15);
	if (s.st_size != 255L) e(16);
	if (fstat(n1, &s1) < 0) e(17);
	if (s.st_dev != s1.st_dev) e(18);
	if (s.st_ino != s1.st_ino) e(19);
	if (s.st_mode != s1.st_mode) e(20);
	if (s.st_nlink != s1.st_nlink) e(21);
	if (s.st_uid != s1.st_uid) e(22);
	if (s.st_gid != s1.st_gid) e(23);
	if (s.st_rdev != s1.st_rdev) e(24);
	if (s.st_size != s1.st_size) e(25);
	if (s.st_atime != s1.st_atime) e(26);
	if (close(n1) < 0) e(27);
	if (unlink("newT3") < 0) e(28);

	umask(040);
	if ((n = creat("T3a", 0777)) < 0) e(29);
	if (stat("T3a", &s) < 0) e(30);
	if (s.st_mode != (mode_t) 0100737) e(31);	/* negative :-( */
	if (unlink("T3a") < 0) e(32);
	if (close(n1) < 0) e(33);

	/* Dup */
	if ((n = creat("T3b", 0777)) < 0) e(34);
	if (close(n) < 0) e(35);
	if ((n = open("T3b", O_RDWR)) < 0) e(36);
	if ((n1 = dup(n)) != n + 1) e(37);
	if (write(n, a, 255) != 255) e(38);
	read(n1, b, 20);
	if (lseek(n, 0L, SEEK_SET) != 0L) e(39);
	if ((j = read(n1, b, 512)) != 255) e(40);
	if (unlink("T3b") < 0) e(41);
	if (close(n) < 0) e(42);
	if (close(n1) < 0) e(43);

  }
}

void test19f()
{
/* Test large files to see if indirect block stuff works. */

  int fd, i;
  long pos;

  subtest = 6;

  if (passes > 0) return;	/* takes too long to repeat this test */
  for (i = 0; i < NBOUNDS; i ++) {
	pos = 1024L * bounds[i];
	fd = creat("T19f", 0777);
	if (fd < 0) e(10*i+1);
	if (lseek(fd, pos, 0) < 0) e(10*i+2);
	if (write(fd, buff, 30720) != 30720) e(10*i+3);
	if (close(fd) < 0) e(10*i+3);
	if (unlink("T19f") < 0) e(10*i+4);
  }
}

void test19g()
{
/* Test POSIX calls for pipe, read, write, lseek and close. */

  int pipefd[2], n, i, fd;
  char buf[512], buf2[512];

  subtest = 7;

  for (i = 0; i < 512; i++) buf[i] = i % 128;

  if (pipe(pipefd) < 0) e(1);
  if (write(pipefd[1], buf, 512) != 512) e(2);
  if (read(pipefd[0], buf2, 512) != 512) e(3);
  if (close(pipefd[1]) != 0) e(4);
  if (close(pipefd[1]) >= 0) e(5);
  if (read(pipefd[0], buf2, 1) != 0) e(6);
  if (close(pipefd[0]) != 0) e(7);

  /* Test O_NONBLOCK on pipes. */
  if (pipe(pipefd) < 0) e(8);
  if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) != 0) e(9);
  if (read(pipefd[0], buf2, 1) != -1) e(10);
  if (errno != EAGAIN) e(11);
  if (close(pipefd[0]) != 0) e(12);
  if (close(pipefd[1]) != 0) e(13);

  /* Test read and lseek. */
  if ( (fd = creat("T19.g1", 0777)) != 3) e(14);	/* create test file */
  if (write(fd, buf, 512) != 512) e(15);
  errno = 3000;
  if (read(fd, buf2, 512) != -1) e(16);
  if (errno != EBADF) e(17);
  if (close(fd) != 0) e(18);
  if ( (fd = open("T19.g1", O_RDWR)) != 3) e(19);
  if (read(fd, buf2, 512) != 512) e(20);
  if (read(fd, buf2, 512) != 0) e(21);
  if (lseek(fd, 100L, SEEK_SET) != 100L) e(22);
  if (read(fd, buf2, 512) != 412) e(23);
  if (lseek(fd, 1000L, SEEK_SET) != 1000L) e(24);

  /* Test write. */
  if (lseek(fd, -1000L, SEEK_CUR) != 0) e(25);
  if (write(fd, buf, 512) != 512) e(26);
  if (lseek(fd, 2L, SEEK_SET) != 2) e(27);
  if (write(fd, buf, 3) != 3) e(28);
  if (lseek(fd, -2L, SEEK_CUR) != 3) e(29);
  if (write(fd, &buf[30], 1) != 1) e(30);
  if (lseek(fd, 2L, SEEK_CUR) != 6) e(31);
  if (write(fd, &buf[60], 1) != 1) e(32);
  if (lseek(fd, -512L, SEEK_END) != 0) e(33);
  if (read(fd, buf2, 8) != 8) e(34);
  errno = 4000;
  if (buf2[0] != 0 || buf2[1] != 1 || buf2[2] != 0 || buf2[3] != 30) e(35);
  if (buf2[4] != 2 || buf2[5] != 5 || buf2[6] != 60 || buf2[7] != 7) e(36);

  /* Turn the O_APPEND flag on. */
  if (fcntl(fd, F_SETFL, O_APPEND) != 0) e(37);
  if (lseek(fd, 0L, SEEK_SET) != 0) e(38);
  if (write(fd, &buf[100], 1) != 1) e(39);
  if (lseek(fd, 0L, SEEK_SET) != 0) e(40);
  if (read(fd, buf2, 10) != 10) e(41);
  if (buf2[0] != 0) e(42);
  if (lseek(fd, -1L, SEEK_END) != 512) e(43);
  if (read(fd, buf2, 10) != 1) e(44);
  if (buf2[0] != 100) e(45);
  if (close(fd) != 0) e(46);

  /* Now try write with O_NONBLOCK. */
  if (pipe(pipefd) != 0) e(47);
  if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) != 0) e(48);
  if (write(pipefd[1], buf, 512) != 512) e(49);
  if (write(pipefd[1], buf, 512) != 512) e(50);
  errno = 0;
  for (i = 1; i < 20; i++) {
	n = write(pipefd[1], buf, 512);
	if (n == 512) continue;
	if (n != -1 || errno != EAGAIN) {e(51); break;}
  }
  if (read(pipefd[0], buf, 512) != 512) e(52);
  if (close(pipefd[0]) != 0) e(53);

  /* Write to a pipe with no reader.  This should generate a signal. */
  signal(SIGPIPE, pipecatcher);
  errno = 0;
  if (write(pipefd[1], buf, 1) != -1) e(54);
  if (errno != EPIPE) e(55);
  if (pipesigs != passes + 1) e(56);	/* we should have had the sig now */
  if (close(pipefd[1]) != 0) e(57);
  errno = 0;
  if (write(100, buf, 512) != -1) e(58);
  if (errno != EBADF) e(59);
  if (unlink("T19.g1") != 0) e(60);
}

void clraa()
{
  int i;
  for (i = 0; i < 100; i++) aa[i] = 0;
}

void pipecatcher(s)
int s;				/* it is supposed to have an arg */
{
  pipesigs++;
}

