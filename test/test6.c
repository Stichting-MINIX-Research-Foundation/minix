/* test 6 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_ERROR 4

int errct;
int subtest = 1;
int zilch[5000];
char curdir[PATH_MAX];

_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(void test6a, (void));
_PROTOTYPE(void test6b, (void));
_PROTOTYPE(void test6c, (void));
_PROTOTYPE(void e, (int n));
_PROTOTYPE(void quit, (void));

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  sync();
  if (geteuid() == 0 || getuid() == 0) {
	printf("Test  6 cannot run as root; test aborted\n");
	exit(1);
  }

  if (argc == 2) m = atoi(argv[1]);

  printf("Test  6 ");
  fflush(stdout);

  getcwd(curdir, PATH_MAX);
  system("rm -rf DIR_06; mkdir DIR_06");
  chdir("DIR_06");

  for (i = 0; i < 70; i++) {
	if (m & 00001) test6a();
	if (m & 00002) test6b();
	if (m & 00004) test6c();
  }

  quit();
  return(-1);			/* impossible */
}

void test6a()
{
/* Test sbrk() and brk(). */

  char *addr, *addr2, *addr3;
  int i, del, click, click2;

  subtest = 1;
  addr = sbrk(0);
  addr = sbrk(0);		/* force break to a click boundary */
  for (i = 0; i < 10; i++) sbrk(7 * i);
  for (i = 0; i < 10; i++) sbrk(-7 * i);
  if (sbrk(0) != addr) e(1);
  sbrk(30);
  if (brk(addr) != 0) e(2);
  if (sbrk(0) != addr) e(3);

  del = 0;
  do {
	del++;
	brk(addr + del);
	addr2 = sbrk(0);
  } while (addr2 == addr);
  click = addr2 - addr;
  sbrk(-1);
  if (sbrk(0) != addr) e(4);
  brk(addr);
  if (sbrk(0) != addr) e(5);

  del = 0;
  do {
	del++;
	brk(addr - del);
	addr3 = sbrk(0);
  } while (addr3 == addr);
  click2 = addr - addr3;
  sbrk(1);
  if (sbrk(0) != addr) e(6);
  brk(addr);
  if (sbrk(0) != addr) e(8);
  if (click != click2) e(9);

  brk(addr + 2 * click);
  if (sbrk(0) != addr + 2 * click) e(10);
  sbrk(3 * click);
  if (sbrk(0) != addr + 5 * click) e(11);
  sbrk(-5 * click);
  if (sbrk(0) != addr) e(12);
}

void test6b()
{
  int i, err;

  subtest = 2;
  signal(SIGQUIT, SIG_IGN);
  err = 0;
  for (i = 0; i < 5000; i++)
	if (zilch[i] != 0) err++;
  if (err > 0) e(1);
  kill(getpid(), SIGQUIT);
}

void test6c()
{
/* Test mknod, chdir, chmod, chown, access.  */

  int i, j;
  struct stat s;

  subtest = 3;
  if (getuid() != 0) return;
  for (j = 0; j < 2; j++) {
	umask(0);

	if (chdir("/") < 0) e(1);
	if (mknod("dir", 040700, 0) < 0) e(2);
	if (link("/", "/dir/..") < 0) e(3);
	if (mknod("T3a", 0777, 0) < 0) e(4);
	if (mknod("/dir/T3b", 0777, 0) < 0) e(5);
	if (mknod("dir/T3c", 0777, 0) < 0) e(6);
	if ((i = open("/dir/T3b", 0)) < 0) e(7);
	if (close(i) < 0) e(8);
	if ((i = open("dir/T3c", O_RDONLY)) < 0) e(9);
	if (close(i) < 0) e(10);
	if (chdir("dir") < 0) e(11);
	if ((i = open("T3b", 0)) < 0) e(12);
	if (close(i) < 0) e(13);
	if ((i = open("../T3a", O_RDONLY)) < 0) e(14);
	if (close(i) < 0) e(15);
	if ((i = open("../dir/../dir/../dir/../dir/../dir/T3c", O_RDONLY)) < 0)
		e(16);
	if (close(i) < 0) e(17);

	if (chmod("../dir/../dir/../dir/../dir/../T3a", 0123) < 0) e(18);
	if (stat("../dir/../dir/../dir/../T3a", &s) < 0) e(19);
	if ((s.st_mode & 077777) != 0123) e(20);
	if (chmod("../dir/../dir/../T3a", 0456) < 0) e(21);
	if (stat("../T3a", &s) < 0) e(22);
	if ((s.st_mode & 077777) != 0456) e(23);
	if (chown("../dir/../dir/../T3a", 20, 30) < 0) e(24);
	if (stat("../T3a", &s) < 0) e(25);
	if (s.st_uid != 20) e(26);
	if (s.st_gid != 30) e(27);

	if ((i = open("/T3c", O_RDONLY)) >= 0) e(28);
	if ((i = open("/T3a", O_RDONLY)) < 0) e(29);
	if (close(i) < 0) e(30);

	if (access("/T3a", 4) < 0) e(31);
	if (access("/dir/T3b", 4) < 0) e(32);
	if (access("/dir/T3d", 4) >= 0) e(33);

	if (unlink("T3b") < 0) e(34);
	if (unlink("T3c") < 0) e(35);
	if (unlink("..") < 0) e(36);
	if (chdir("/") < 0) e(37);
	if (unlink("dir") < 0) e(38);
	if (unlink("/T3a") < 0) e(39);
  }

}

void e(n)
int n;
{

  int err_num = errno;		/* save errno in case printf clobbers it */

  printf("Subtest %d,  error %d  errno=%d  ", subtest, n, errno);
  errno = err_num;		/* restore errno, just in case */
  perror("");
  if (errct++ > MAX_ERROR) {
	printf("Too many errors; test aborted\n");
	chdir("..");
	system("rm -rf DIR*");
	exit(1);
  }
}

void quit()
{

  chdir("..");
  system("rm -rf DIR*");

  chdir(curdir);
  system("rm -rf DIR*");
  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
