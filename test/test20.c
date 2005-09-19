/* test20: fcntl()		Author: Jan-Mark Wams (jms@cs.vu.nl) */

/* Some things have to be checked for ``exec()'' call's. Therefor
** there is a check routine called ``do_check()'' that will be
** called if the first argument (``argv[0]'') equals ``DO CHECK.''
** Note that there is no way the shell (``/bin/sh'') will set
** ``argv[0]'' to this funny value. (Unless we rename ``test20''
** to ``DO CHECK'' ;-)
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

int errct = 0;
int subtest = 1;
int superuser;
char MaxName[NAME_MAX + 1];	/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char ToLongName[NAME_MAX + 2];	/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

_PROTOTYPE(void main, (int argc, char *argv[]));
_PROTOTYPE(void test20a, (void));
_PROTOTYPE(void test20b, (void));
_PROTOTYPE(void test20c, (void));
_PROTOTYPE(void test20d, (void));
_PROTOTYPE(int do_check, (void));
_PROTOTYPE(void makelongnames, (void));
_PROTOTYPE(void e, (int number));
_PROTOTYPE(void quit, (void));

char executable[1024];

void main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);

  /* If we have to check things, call do_check(). */
  if (strcmp(argv[0], "DO CHECK") == 0) exit(do_check());

  /* Get the path of the executable. */
  strcpy(executable, "../");
  strcat(executable, argv[0]);

  printf("Test 20 ");
  fflush(stdout);
  System("rm -rf DIR_20; mkdir DIR_20");
  Chdir("DIR_20");
  makelongnames();
  superuser = (geteuid() == 0);

  for (i = 0; i < ITERATIONS; i++) {
	test20a();
	test20b();
	test20c();
	test20d();
  }
  quit();
}

void test20a()
{				/* Test normal operation. */
  subtest = 1;
  System("rm -rf ../DIR_20/*");
}

void test20b()
{
  subtest = 2;
  System("rm -rf ../DIR_20/*");
}

void test20c()
{
  subtest = 3;
  System("rm -rf ../DIR_20/*");
}

/* Open fds 3, 4, 5 and 6. Set FD_CLOEXEC on 5 and 6. Exclusively lock the
** first 10 bytes of fd no. 3. Shared lock fd no. 7. Lock fd no. 8 after
** the fork. Do a ``exec()'' call with a funny argv[0] and check the return
** value.
*/
void test20d()
{				/* Test locks with ``fork()'' and ``exec().'' */
  int fd3, fd4, fd5, fd6, fd7, fd8;
  int stat_loc;
  int do_check_retval;
  char *argv[2];
  struct flock fl;

  subtest = 4;

  argv[0] = "DO CHECK";
  argv[1] = (char *) NULL;

  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 10;

  /* Make a dummy files and open them. */
  System("echo 'Great Balls Of Fire!' > file3");
  System("echo 'Great Balls Of Fire!' > file4");
  System("echo 'Great Balls Of Fire!' > file7");
  System("echo 'Great Balls Of Fire!' > file8");
  System("echo 'Great Balls Of Fire!' > file");
  if ((fd3 = open("file3", O_RDWR)) != 3) e(1);
  if ((fd4 = open("file4", O_RDWR)) != 4) e(2);
  if ((fd5 = open("file", O_RDWR)) != 5) e(3);
  if ((fd6 = open("file", O_RDWR)) != 6) e(4);
  if ((fd7 = open("file7", O_RDWR)) != 7) e(5);
  if ((fd8 = open("file8", O_RDWR)) != 8) e(6);

  /* Set FD_CLOEXEC flags on fd5 and fd6. */
  if (fcntl(fd5, F_SETFD, FD_CLOEXEC) == -1) e(7);
  if (fcntl(fd6, F_SETFD, FD_CLOEXEC) == -1) e(8);

  /* Lock the first ten bytes from fd3 (for writing). */
  fl.l_type = F_WRLCK;
  if (fcntl(fd3, F_SETLK, &fl) == -1) e(9);

  /* Lock (for reading) fd7. */
  fl.l_type = F_RDLCK;
  if (fcntl(fd7, F_SETLK, &fl) == -1) e(10);

  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);

	/* Lock fd8. */
	fl.l_type = F_WRLCK;
	if (fcntl(fd8, F_SETLK, &fl) == -1) e(11);

	/* Check the lock on fd3 and fd7. */
	fl.l_type = F_WRLCK;
	if (fcntl(fd3, F_GETLK, &fl) == -1) e(12);
	if (fl.l_type != F_WRLCK) e(13);
	if (fl.l_pid != getppid()) e(14);
	fl.l_type = F_WRLCK;
	if (fcntl(fd7, F_GETLK, &fl) == -1) e(15);
	if (fl.l_type != F_RDLCK) e(16);
	if (fl.l_pid != getppid()) e(17);

	/* Check FD_CLOEXEC flags. */
	if ((fcntl(fd3, F_GETFD) & FD_CLOEXEC) != 0) e(18);
	if ((fcntl(fd4, F_GETFD) & FD_CLOEXEC) != 0) e(19);
	if ((fcntl(fd5, F_GETFD) & FD_CLOEXEC) != FD_CLOEXEC) e(20);
	if ((fcntl(fd6, F_GETFD) & FD_CLOEXEC) != FD_CLOEXEC) e(21);
	if ((fcntl(fd7, F_GETFD) & FD_CLOEXEC) != 0) e(22);
	if ((fcntl(fd8, F_GETFD) & FD_CLOEXEC) != 0) e(23);

	execlp(executable + 3, "DO CHECK", (char *) NULL);
	execlp(executable, "DO CHECK", (char *) NULL);
	printf("Can't exec %s or %s\n", executable + 3, executable);
	exit(0);

      default:
	wait(&stat_loc);
	if (WIFSIGNALED(stat_loc)) e(24);	/* Alarm? */
	if (WIFEXITED(stat_loc) == 0) {
		errct=10000;
		quit();
	}
  }

  /* Check the return value of do_check(). */
  do_check_retval = WEXITSTATUS(stat_loc);
  if ((do_check_retval & 0x11) == 0x11) e(25);
  if ((do_check_retval & 0x12) == 0x12) e(26);
  if ((do_check_retval & 0x14) == 0x14) e(27);
  if ((do_check_retval & 0x18) == 0x18) e(28);
  if ((do_check_retval & 0x21) == 0x21) e(29);
  if ((do_check_retval & 0x22) == 0x22) e(30);
  if ((do_check_retval & 0x24) == 0x24) e(31);
  if ((do_check_retval & 0x28) == 0x28) e(32);
  if ((do_check_retval & 0x41) == 0x41) e(33);
  if ((do_check_retval & 0x42) == 0x42) e(34);
  if ((do_check_retval & 0x44) == 0x44) e(35);
  if ((do_check_retval & 0x48) == 0x48) e(36);
  if ((do_check_retval & 0x81) == 0x81) e(37);
  if ((do_check_retval & 0x82) == 0x82) e(38);
  if ((do_check_retval & 0x84) == 0x84) e(39);
  if ((do_check_retval & 0x88) == 0x88) e(40);

  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);

	/* Lock fd8. */
	fl.l_type = F_WRLCK;
	if (fcntl(fd8, F_SETLK, &fl) == -1) e(41);

	execvp(executable + 3, argv);
	execvp(executable, argv);
	printf("Can't exec %s or %s\n", executable + 3, executable);
	exit(0);

      default:
	wait(&stat_loc);
	if (WIFSIGNALED(stat_loc)) e(48);	/* Alarm? */
  }

  /* Check the return value of do_check(). */
  do_check_retval = WEXITSTATUS(stat_loc);
  if ((do_check_retval & 0x11) == 0x11) e(49);
  if ((do_check_retval & 0x12) == 0x12) e(50);
  if ((do_check_retval & 0x14) == 0x14) e(51);
  if ((do_check_retval & 0x18) == 0x18) e(52);
  if ((do_check_retval & 0x21) == 0x21) e(53);
  if ((do_check_retval & 0x22) == 0x22) e(54);
  if ((do_check_retval & 0x24) == 0x24) e(55);
  if ((do_check_retval & 0x28) == 0x28) e(56);
  if ((do_check_retval & 0x41) == 0x41) e(57);
  if ((do_check_retval & 0x42) == 0x42) e(58);
  if ((do_check_retval & 0x44) == 0x44) e(59);
  if ((do_check_retval & 0x48) == 0x48) e(60);
  if ((do_check_retval & 0x81) == 0x81) e(61);
  if ((do_check_retval & 0x82) == 0x82) e(62);
  if ((do_check_retval & 0x84) == 0x84) e(63);
  if ((do_check_retval & 0x88) == 0x88) e(64);

  fl.l_type = F_UNLCK;
  if (fcntl(fd3, F_SETLK, &fl) == -1) e(65);
  if (fcntl(fd7, F_SETLK, &fl) == -1) e(66);

  if (close(fd3) != 0) e(67);
  if (close(fd4) != 0) e(68);
  if (close(fd5) != 0) e(69);
  if (close(fd6) != 0) e(70);
  if (close(fd7) != 0) e(71);
  if (close(fd8) != 0) e(72);

  System("rm -f ../DIR_20/*\n");
}

/* This routine checks that fds 0 through 4, 7 and 8 are open and the rest
** is closed. It also checks if we can lock the first 10 bytes on fd no. 3
** and 4. It should not be possible to lock fd no. 3, but it should be
** possible to lock fd no. 4. See ``test20d()'' for usage of this routine.
*/
int do_check()
{
  int i;
  int retval = 0;
  struct flock fl;

  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 10;

  /* All std.. are open. */
  if (fcntl(0, F_GETFD) == -1) retval |= 0x11;
  if (fcntl(1, F_GETFD) == -1) retval |= 0x11;
  if (fcntl(2, F_GETFD) == -1) retval |= 0x11;

  /* Fd no. 3, 4, 7 and 8 are open. */
  if (fcntl(3, F_GETFD) == -1) retval |= 0x12;
  if (fcntl(4, F_GETFD) == -1) retval |= 0x12;
  if (fcntl(7, F_GETFD) == -1) retval |= 0x12;

  /* Fd no. 5, 6 and 9 trough OPEN_MAX are closed. */
  if (fcntl(5, F_GETFD) != -1) retval |= 0x14;
  if (fcntl(6, F_GETFD) != -1) retval |= 0x14;
  for (i = 9; i < OPEN_MAX; i++)
	if (fcntl(i, F_GETFD) != -1) retval |= 0x18;

#if 0
  /* Fd no. 3 is WRLCKed. */
  fl.l_type = F_WRLCK;
  if (fcntl(3, F_SETLK, &fl) != -1) retval |= 0x21;
  if (errno != EACCES && errno != EAGAIN) retval |= 0x22;
  fl.l_type = F_RDLCK;
  if (fcntl(3, F_SETLK, &fl) != -1) retval |= 0x24;
  if (errno != EACCES && errno != EAGAIN) retval |= 0x22;
  fl.l_type = F_RDLCK;
  if (fcntl(3, F_GETLK, &fl) == -1) retval |= 0x28;
  if (fl.l_type != F_WRLCK) retval |= 0x28;
  if (fl.l_pid != getpid()) retval |= 0x28;
  fl.l_type = F_WRLCK;
  if (fcntl(3, F_GETLK, &fl) == -1) retval |= 0x28;
  if (fl.l_type != F_WRLCK) retval |= 0x28;
  if (fl.l_pid != getpid()) retval |= 0x28;
#endif

  /* Fd no. 4 is not locked. */
  fl.l_type = F_WRLCK;
  if (fcntl(4, F_SETLK, &fl) == -1) retval |= 0x41;
  if (fcntl(4, F_GETLK, &fl) == -1) retval |= 0x42;
#if 0 /* XXX - see test7.c */
  if (fl.l_type != F_WRLCK) retval |= 0x42;
  if (fl.l_pid != getpid()) retval |= 0x42;
#endif /* 0 */

  /* Fd no. 8 is locked after the fork, it is ours. */
  fl.l_type = F_WRLCK;
  if (fcntl(8, F_SETLK, &fl) == -1) retval |= 0x44;
  if (fcntl(8, F_GETLK, &fl) == -1) retval |= 0x48;
#if 0 /* XXX - see test7.c */
  if (fl.l_type != F_WRLCK) retval |= 0x48;
  if (fl.l_pid != getpid()) retval |= 0x48;
#endif /* 0 */

#if 0
  /* Fd no. 7 is RDLCKed. */
  fl.l_type = F_WRLCK;
  if (fcntl(7, F_SETLK, &fl) != -1) retval |= 0x81;
  if (errno != EACCES && errno != EAGAIN) retval |= 0x82;
  fl.l_type = F_RDLCK;
  if (fcntl(7, F_SETLK, &fl) == -1) retval |= 0x84;
  fl.l_type = F_RDLCK;
  if (fcntl(7, F_GETLK, &fl) == -1) retval |= 0x88;
  if (fl.l_type != F_UNLCK) retval |= 0x88;
  fl.l_type = F_WRLCK;
  if (fcntl(7, F_GETLK, &fl) == -1) retval |= 0x88;
  if (fl.l_type != F_RDLCK) retval |= 0x88;
  if (fl.l_pid != getppid()) retval |= 0x88;
#endif

  return retval;
}

void makelongnames()
{
  register int i;

  memset(MaxName, 'a', NAME_MAX);
  MaxName[NAME_MAX] = '\0';
  for (i = 0; i < PATH_MAX - 1; i++) {	/* idem path */
	MaxPath[i++] = '.';
	MaxPath[i] = '/';
  }
  MaxPath[PATH_MAX - 1] = '\0';

  strcpy(ToLongName, MaxName);	/* copy them Max to ToLong */
  strcpy(ToLongPath, MaxPath);

  ToLongName[NAME_MAX] = 'a';
  ToLongName[NAME_MAX + 1] = '\0';	/* extend ToLongName by one too many */
  ToLongPath[PATH_MAX - 1] = '/';
  ToLongPath[PATH_MAX] = '\0';	/* inc ToLongPath by one */
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
  System("rm -rf DIR_20");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else if (errct < 10000) {
	printf("%d errors\n", errct);
	exit(1);
  } else {
	printf("errors\n");
	exit(2);
  }
}
