/* test 16 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>

int max_error = 4;
#include "common.h"


int subtest, passes;
int V1filesystem = 0;


int main(int argc, char *argv []);
void test16init(void);
void test16a(void);
void test16b(void);
void test16c(void);
void test16d(void);
void test16e(void);
void test16f(void);
void test16g(void);
void test16h(void);
void get_times(char *name, time_t *a, time_t *c, time_t *m);

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m;

  start(16);

  m = (argc == 2 ? atoi(argv[1]) : 0xFFFF);


  for (i = 0; i < 4; i++) {
	test16init();
	if (m & 0001) test16a();
	if (m & 0002) test16b();
	if (m & 0004) test16c();
	if (m & 0010) test16d();
	if (m & 0020) test16e();
	if (m & 0040) test16f();
	if (m & 0100) test16g();
	if (m & 0200) test16h();
	passes++;
  }
  quit();
  return(-1);			/* impossible */
}

void test16init()
{
/* Test atime, ctime, and mtime. */

  int fd;
  char buf[1024];
  struct stat s;

  subtest = 0;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  if ( (fd = creat("T16.a", 0666)) < 0) e(1);
  if (write(fd, buf, 1024) != 1024) e(2);
  if (close(fd) < 0) e(3);
  sleep(1);			/* wait 1 sec before continuing */
  if ( (fd = open("T16.a", O_RDONLY)) < 0) e(4);
  if (read(fd, buf, 3) != 3) e(5);
  if (close(fd) != 0) e(6);
  if (stat("T16.a", &s) != 0) e(7);
  if (s.st_atime == 0) {
	/* Almost certainly means we are running a V1 file system. */
	printf(" (atime = 0. Probably V1 file system.  V2+ tests skipped.) ");
	V1filesystem = 1;
  }
}

  /* Many system calls affect atime, ctime, and mtime.  Test them.  They
   * fall into several groups.  The members of each group can be tested
   * together.  Start with creat(), mkdir(), and mkfifo, all of which
   * set all 3 times on the created object, and ctime and mtime of the dir.
   */

void test16a()
{
/* Test creat(). */

  int fd;
  time_t a, c, m, pa, pc, pm;

  subtest = 1;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  if ( (fd = creat("T16.b", 0666)) < 0) e(8);
  if (close(fd) != 0) e(9);
  get_times("T16.b", &a, &c, &m);
  get_times(".", &pa, &pc, &pm);
  if (a != c) e(10);
  if (a != m) e(11);
  if (a != pc) e(12);
  if (a != pm) e(13);
  if (unlink("T16.b") < 0) e(14);
}

void test16b()
{
  /* Test the times for mkfifo. */
  int fd;
  time_t a, c, m, pa, pc, pm;

  subtest = 2;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  if  ( (fd = mkfifo("T16.c", 0666)) != 0) e(15);
  if (access("T16.c", R_OK | W_OK) != 0) e(16);
  get_times("T16.c", &a, &c, &m);
  get_times(".", &pa, &pc, &pm);
  if (a != c) e(17);
  if (a != m) e(18);
  if (a != pc) e(19);
  if (a != pm) e(20);
  if (unlink("T16.c") < 0) e(21);
}

void test16c()
{
  /* Test the times for mkdir. */
  time_t a, c, m, pa, pc, pm, xa, xc, xm;

  subtest = 3;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  if (mkdir("T16.d", 0666) < 0) e(22);
  get_times("T16.d", &a, &c, &m);
  get_times(".", &pa, &pc, &pm);
  if (a != c) e(23);
  if (a != m) e(24);
  if (a != pc) e(25);
  if (a != pm) e(26);
  sleep(1);
  if (rmdir("T16.d") < 0) e(27);
  get_times(".", &xa, &xc, &xm);
  if (c == xc) e(28);
  if (m == xm) e(29);
  if (xc != xm) e(30);
}

void test16d()
{
  /* Test open(file, O_TRUNC). */
  int fd;
  time_t a, c, m, pa, pc, pm, xa, xc, xm, ya, yc, ym;
  char buf[1024];

  subtest = 4;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  if ( (fd = open("T16.e", O_WRONLY|O_CREAT, 0666)) < 0) e(31);
  if (write(fd, buf, 1024) != 1024) e(32);
  if (close(fd) != 0) e(33);
  get_times("T16.e", &a, &c, &m);
  get_times(".", &pa, &pc, &pm);
  sleep(1);
  if ( (fd = open("T16.e", O_WRONLY|O_TRUNC)) < 0) e(34);
  get_times("T16.e", &xa, &xc, &xm);
  get_times(".", &ya, &yc, &ym);
  if (c != m) e(35);
  if (pc != pm) e(36);
  if (c == xc) e(37);
  if (m == xm) e(38);
  if (yc != pc) e(39);
  if (ym != pm) e(40);
  if (close(fd) != 0) e(41);
  /* Try once more, now without changing the file size. */
  sleep(1);
  if ( (fd = open("T16.e", O_WRONLY|O_TRUNC)) < 0) e(89);
  get_times("T16.e", &a, &c, &m);
  if (c != m) e(90);
  if (c == xc) e(91);
  if (m == xm) e(92);
  if (close(fd) != 0) e(93);
}

void test16e()
{
  /* Test the times for link/unlink. */
  time_t a, c, m, pa, pc, pm, xa, xc, xm, ya, yc, ym;

  subtest = 5;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  get_times("T16.e", &a, &c, &m);
  get_times(".", &ya, &yc, &ym);
  sleep(1);
  if (link("T16.e", "T16.f") != 0) e(42);	/* second link */
  get_times("T16.e", &xa, &xc, &xm);
  get_times(".", &pa, &pc, &pm);
  if (a != xa) e(43);
  if (m != xm) e(44);
#ifndef V1_FILESYSTEM
  if (c == xc) e(45);
#endif
  if (ya != pa) e(46);
  if (yc == pc) e(47);
  if (ym == pm) e(48);
  if (yc != ym) e(49);
  if (pc != pm) e(50);
  sleep(1);
  if (unlink("T16.f") != 0) e(46);
  get_times("T16.e", &a, &c, &m);
  get_times(".", &ya, &yc, &ym);
  if (a != xa) e(51);
  if (m != xm) e(52);
#ifndef V1_FILESYSTEM
  if (c == xc) e(53);
#endif
  if (pa != ya) e(54);
  if (pc == yc) e(55);
  if (pm == ym) e(56);
  if (yc != ym) e(57);
  if (unlink("T16.e") != 0) e(58);
}

void test16f()
{
  /* Test rename, read, write, chmod, utime. */
  int fd, fd1, fd2, fd3, fd4;
  time_t a, c, m, pa, pc, pm, xa, xc, xm, ya, yc, ym, za, zc, zm, ta, tc, tm;
  time_t wa, wc, wm;
  char buf[1024];

  subtest = 6;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  get_times(".", &pa, &pc, &pm);
  if ( (fd  = open("T16.g", O_RDWR|O_CREAT)) < 0) e(59);
  if ( (fd1 = open("T16.h", O_WRONLY|O_CREAT, 0666)) < 0) e(60);
  if ( (fd2 = open("T16.i", O_WRONLY|O_CREAT, 0666)) < 0) e(61);
  if ( (fd3 = open("T16.j", O_WRONLY|O_CREAT, 0666)) < 0) e(62);
  if ( (fd4 = open("T16.k", O_RDWR|O_CREAT, 0666)) < 0) e(63);
  if (write(fd, buf, 1024) != 1024) e(64);
  get_times("T16.g", &a, &c, &m);
  get_times("T16.h", &pa, &pc, &pm);
  get_times("T16.i", &xa, &xc, &xm);
  get_times("T16.j", &ya, &yc, &ym);
  get_times("T16.k", &za, &zc, &zm);
  get_times(".", &wa, &wc, &wm);
  sleep(1);
  lseek(fd, 0L, SEEK_SET);
  if (read(fd, buf, 35) != 35) e(65);
  get_times("T16.g", &ta, &tc, &tm);
  if (a == ta || c != tc || m != tm) e(66);
  if (write(fd1, buf, 35) != 35) e(67);
  get_times("T16.h", &ta, &tc, &tm);
  if (pa != ta || pc == tc || pm == tm) e(69);
  if (rename("T16.i", "T16.i1") != 0) e(70);
  get_times("T16.i1", &ta, &tc, &tm);
  if (xa != ta || xc != tc || xm != tm) e(71);
  get_times(".", &a, &c, &m);
  if (a != wa || c == wc || m == wm || wc != wm) e(72);
  if (chmod("T16.j", 0777) != 0) e(73);
  get_times("T16.j", &ta, &tc, &tm);
  if (ya != ta || yc == tc || ym != tm) e(74);
  if (utime("T16.k", (void *) 0) != 0) e(75);
  get_times("T16.k", &ta, &tc, &tm);
  if (za == ta || zc == tc) e(76);
  if (close(fd) != 0) e(77);
  if (close(fd1) != 0) e(78);
  if (close(fd2) != 0) e(79);
  if (close(fd3) != 0) e(80);
  if (close(fd4) != 0) e(81);
  if (unlink("T16.g") != 0) e(82); 
  if (unlink("T16.h") != 0) e(83); 
  if (unlink("T16.i1") != 0) e(84); 
  if (unlink("T16.j") != 0) e(85); 
  if (unlink("T16.k") != 0) e(86); 
}

void test16g()
{
  /* Test the times for truncate. */
  time_t a, c, m, ta, tc, tm;
  struct stat s;

  subtest = 7;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  if (system("echo 1 > T16.l") != 0) e(87);
  stat("T16.l", &s);
  get_times("T16.l", &a, &c, &m);
  sleep(1);
  truncate("T16.l", s.st_size);
  get_times("T16.l", &ta, &tc, &tm);
  if (a != ta || c != tc || m != tm) e(88);

}

void test16h()
{
  /* Test utimes, futimes, lutimes, futimens, utimensat. */
  int fd, fd1, fd2, fd3, fd4;
  time_t a, c, m, pa, pc, pm;
  time_t ta, tc, tm, wa, wc, wm, xa, xc, xm, ya, yc, ym, za, zc, zm;

  subtest = 8;
  if (passes > 0) return;	/* takes too long to repeat this test */
  if (V1filesystem) return;	/* no need to spend time testing lacking features */

  get_times(".", &pa, &pc, &pm);
  if ( (fd  = open("T16.m", O_RDWR|O_CREAT, 0666)) < 0) e(89);
  if ( (fd1 = open("T16.n", O_RDWR|O_CREAT, 0666)) < 0) e(90);
  if ( (fd2 = open("T16.o", O_RDWR|O_CREAT, 0666)) < 0) e(91);
  if ( (fd3 = open("T16.p", O_RDWR|O_CREAT, 0666)) < 0) e(92);
  if ( (fd4 = open("T16.q", O_RDWR|O_CREAT, 0666)) < 0) e(93);
  get_times("T16.m", &ta, &tc, &tm);
  get_times("T16.n", &wa, &wc, &wm);
  get_times("T16.o", &xa, &xc, &xm);
  get_times("T16.p", &ya, &yc, &ym);
  get_times("T16.q", &za, &zc, &zm);
  get_times(".", &pa, &pc, &pm);
  sleep(1);
  if (utimes("T16.m", (void *) 0) != 0) e(94);
  get_times("T16.m", &a, &c, &m);
  if (a == ta || c == tc) e(95);
  if (futimes(fd1, (void *) 0) != 0) e(96);
  get_times("T16.n", &a, &c, &m);
  if (a == wa || c == wc) e(97);
  if (lutimes("T16.o", (void *) 0) != 0) e(98);
  get_times("T16.o", &a, &c, &m);
  if (a == xa || c == xc) e(99);
  if (utimensat(AT_FDCWD, "T16.p", (void *) 0, 0) != 0) e(100);
  get_times("T16.p", &a, &c, &m);
  if (a == ya || c == yc) e(101);
  if (futimens(fd4, (void *) 0) != 0) e(102);
  get_times("T16.q", &a, &c, &m);
  if (a == za || c == zc) e(103);
  if (close(fd) != 0) e(104);
  if (close(fd1) != 0) e(105);
  if (close(fd2) != 0) e(106);
  if (close(fd3) != 0) e(107);
  if (close(fd4) != 0) e(108);
  if (unlink("T16.m") != 0) e(109);
  if (unlink("T16.n") != 0) e(110);
  if (unlink("T16.o") != 0) e(111);
  if (unlink("T16.p") != 0) e(112);
  if (unlink("T16.q") != 0) e(113);
}

void get_times(name, a, c, m)
char *name;
time_t *a, *c, *m;
{
  struct stat s;

  if (stat(name, &s) != 0) e(500);
  *a = s.st_atime;
  *c = s.st_ctime;
  *m = s.st_mtime;
}

