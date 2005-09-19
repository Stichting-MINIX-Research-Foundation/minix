/* test29: dup() dup2()		Author: Jan-Mark Wams (jms@cs.vu.nl) */

/* The definition of ``dup2()'' is realy a big mess! For:
**
** (1) if fildes2 is less than zero or greater than {OPEN_MAX}
**     errno has to set to [EBADF]. But if fildes2 equals {OPEN_MAX}
**     errno has to be set to [EINVAL]. And ``fcntl(F_DUPFD...)'' always
**     returns [EINVAL] if fildes2 is out of range!
**
** (2) if the number of file descriptors would exceed {OPEN_MAX}, or no
**     file descriptors above fildes2 are available, errno has to be set
**     to [EMFILE]. But this can never occur!
*/

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

#define MAX_ERROR	4
#define ITERATIONS     10

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)

#define IS_CLOEXEC(fd)	((fcntl(fd, F_GETFD) & FD_CLOEXEC) == FD_CLOEXEC)
#define SET_CLOEXEC(fd)	fcntl(fd, F_SETFD, FD_CLOEXEC)

int errct = 0;
int subtest = 1;
int superuser;
char MaxName[NAME_MAX + 1];	/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char ToLongName[NAME_MAX + 2];	/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

_PROTOTYPE(void main, (int argc, char *argv[]));
_PROTOTYPE(void test29a, (void));
_PROTOTYPE(void test29b, (void));
_PROTOTYPE(void test29c, (void));
_PROTOTYPE(void e, (int number));
_PROTOTYPE(void quit, (void));

void main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  printf("Test 29 ");
  fflush(stdout);
  System("rm -rf DIR_29; mkdir DIR_29");
  Chdir("DIR_29");
  superuser = (geteuid() == 0);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test29a();
	if (m & 0002) test29b();
	if (m & 0004) test29c();
  }
  quit();
}

void test29a()
{
  int fd1, fd2, fd3, fd4, fd5;
  struct flock flock;

  subtest = 1;

  /* Basic checking. */
  if ((fd1 = dup(0)) != 3) e(1);
  if ((fd2 = dup(0)) != 4) e(2);
  if ((fd3 = dup(0)) != 5) e(3);
  if ((fd4 = dup(0)) != 6) e(4);
  if ((fd5 = dup(0)) != 7) e(5);
  if (close(fd2) != 0) e(6);
  if (close(fd4) != 0) e(7);
  if ((fd2 = dup(0)) != 4) e(8);
  if ((fd4 = dup(0)) != 6) e(9);
  if (close(fd1) != 0) e(10);
  if (close(fd3) != 0) e(11);
  if (close(fd5) != 0) e(12);
  if ((fd1 = dup(0)) != 3) e(13);
  if ((fd3 = dup(0)) != 5) e(14);
  if ((fd5 = dup(0)) != 7) e(15);
  if (close(fd1) != 0) e(16);
  if (close(fd2) != 0) e(17);
  if (close(fd3) != 0) e(18);
  if (close(fd4) != 0) e(19);
  if (close(fd5) != 0) e(20);

  /* FD_CLOEXEC should be cleared. */
  if ((fd1 = dup(0)) != 3) e(21);
  if (SET_CLOEXEC(fd1) == -1) e(22);
  if (!IS_CLOEXEC(fd1)) e(23);
  if ((fd2 = dup(fd1)) != 4) e(24);
  if ((fd3 = dup(fd2)) != 5) e(25);
  if (IS_CLOEXEC(fd2)) e(26);
  if (IS_CLOEXEC(fd3)) e(27);
  if (SET_CLOEXEC(fd2) == -1) e(28);
  if (!IS_CLOEXEC(fd2)) e(29);
  if (IS_CLOEXEC(fd3)) e(30);
  if (close(fd1) != 0) e(31);
  if (close(fd2) != 0) e(32);
  if (close(fd3) != 0) e(33);

  /* Locks should be shared, so we can lock again. */
  System("echo 'Hallo' > file");
  if ((fd1 = open("file", O_RDWR)) != 3) e(34);
  flock.l_whence = SEEK_SET;
  flock.l_start = 0;
  flock.l_len = 10;
  flock.l_type = F_WRLCK;
  if (fcntl(fd1, F_SETLK, &flock) == -1) e(35);
  if (fcntl(fd1, F_SETLK, &flock) == -1) e(36);
  if ((fd2 = dup(fd1)) != 4) e(37);
  if (fcntl(fd1, F_SETLK, &flock) == -1) e(38);
  if (fcntl(fd1, F_GETLK, &flock) == -1) e(39);
#if 0 /* XXX - see test7.c */
  if (flock.l_type != F_WRLCK) e(40);
  if (flock.l_pid != getpid()) e(41);
#endif /* 0 */
  flock.l_type = F_WRLCK;
  if (fcntl(fd2, F_GETLK, &flock) == -1) e(42);
#if 0 /* XXX - see test7.c */
  if (flock.l_type != F_WRLCK) e(43);
  if (flock.l_pid != getpid()) e(44);
#endif /* 0 */
  if (close(fd1) != 0) e(45);
  if (close(fd2) != 0) e(46);

  System("rm -rf ../DIR_29/*");
}

void test29b()
{
  int fd;
  char buf[32];

  subtest = 2;

  /* Test file called ``file''. */
  System("echo 'Hallo!' > file");

  /* Check dup2() call with the same fds. Should have no effect. */
  if ((fd = open("file", O_RDONLY)) != 3) e(1);
  if (read(fd, buf, 2) != 2) e(2);
  if (strncmp(buf, "Ha", 2) != 0) e(3);
  if (dup2(fd, fd) != fd) e(4);
  if (read(fd, buf, 2) != 2) e(5);
  if (strncmp(buf, "ll", 2) != 0) e(6);
  if (dup2(fd, fd) != fd) e(7);
  if (read(fd, buf, 2) != 2) e(8);
  if (strncmp(buf, "o!", 2) != 0) e(9);
  if (close(fd) != 0) e(10);

  /* If dup2() call fails, the fildes2 argument has to stay open. */
  if ((fd = open("file", O_RDONLY)) != 3) e(11);
  if (read(fd, buf, 2) != 2) e(12);
  if (strncmp(buf, "Ha", 2) != 0) e(13);
  if (dup2(OPEN_MAX + 3, fd) != -1) e(14);
  if (errno != EBADF) e(15);
  if (read(fd, buf, 2) != 2) e(16);
  if (strncmp(buf, "ll", 2) != 0) e(17);
  if (dup2(-4, fd) != -1) e(18);
  if (errno != EBADF) e(19);
  if (read(fd, buf, 2) != 2) e(20);
  if (strncmp(buf, "o!", 2) != 0) e(21);
  if (close(fd) != 0) e(22);

  System("rm -rf ../DIR_29/*");
}

void test29c()
{
  int i;

  subtest = 3;

  /* Check bad arguments to dup() and dup2(). */
  for (i = -OPEN_MAX; i < OPEN_MAX * 2; i++) {

	/* ``i'' is a valid and open fd. */
	if (i >= 0 && i < 3) continue;

	/* If ``i'' is a valid fd it is not open. */
	if (dup(i) != -1) e(1);
	if (errno != EBADF) e(2);

	/* ``i'' Is OPEN_MAX. */
	if (i == OPEN_MAX) {
		if (dup2(0, i) != -1) e(3);
		if (errno != EINVAL) e(4);
	}

	/* ``i'' Is out of range. */
	if (i < 0 || i > OPEN_MAX) {
		if (dup2(0, i) != -1) e(5);
		if (errno != EBADF) e(6);
	}
  }

  System("rm -rf ../DIR_29/*");
}

void e(n)
int n;
{
  int err_num = errno;		/* Save in case printf clobbers it. */

  printf("Subtest %d,  error %d  errno=%d: ", subtest, n, errno);
  errno = err_num;
  perror("");
  if (errct++ > MAX_ERROR) {
	printf("Too many errors; test aborted\n");
	chdir("..");
	system("rm -rf DIR*");
	exit(1);
  }
  errno = 0;
}

void quit()
{
  Chdir("..");
  System("rm -rf DIR_29");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
