/* test22: umask()		(p) Jan-Mark Wams. email: jms@cs.vu.nl */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>

#include "common.h"

#define ITERATIONS 2

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)


void test22a(void);
int mode(char *filename);
int umode(char *filename);
void quit(void);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  start(22);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test22a();
  }

  quit();

  return (-1);	/* Unreachable */
}

void test22a()
{
  int fd1, fd2;
  int i, oldmask;
  int stat_loc;			/* For the wait sys call. */

  subtest = 1;

  system("chmod 777 ../DIR_22/* ../DIR_22/*/* > /dev/null 2>&1");
  System("rm -rf ../DIR_22/*");

  oldmask = 0123;		/* Set oldmask and umask. */
  umask(oldmask);		/* Set oldmask and umask. */

  /* Check all the possible values of umask. */
  for (i = 0000; i <= 0777; i++) {
	if (oldmask != umask(i)) e(1);	/* set umask() */
	fd1 = open("open", O_CREAT, 0777);
	if (fd1 != 3) e(2);	/* test open(), */
	fd2 = creat("creat", 0777);
	if (fd2 != 4) e(3);	/* creat(), */
	if (mkdir("dir", 0777) != 0) e(4);	/* mkdir(), */
	if (mkfifo("fifo", 0777) != 0) e(5);	/* and mkfifo(). */

	if (umode("open") != i) e(6);	/* see if they have */
	if (umode("creat") != i) e(7);	/* the proper mode */
	if (umode("dir") != i) e(8);
	if (umode("fifo") != i) e(9);

	/* Clean up */
	if (close(fd1) != 0) e(10);
	if (close(fd2) != 0) e(11);	/* close fd's and */
	unlink("open");		/* clean the dir */
	unlink("creat");
	rmdir("dir");
	unlink("fifo");
	oldmask = i;		/* save current mask */
  }

  /* Check-reset mask */
  if (umask(0124) != 0777) e(12);

  /* Check if a umask of 0000 leaves the modes alone. */
  if (umask(0000) != 0124) e(13);
  for (i = 0000; i <= 0777; i++) {
	fd1 = open("open", O_CREAT, i);
	if (fd1 != 3) e(14);	/* test open(), */
	fd2 = creat("creat", i);
	if (fd2 != 4) e(15);	/* creat(), */
	if (mkdir("dir", i) != 0) e(16);	/* mkdir(), */
	if (mkfifo("fifo", i) != 0) e(17);	/* and mkfifo(). */

	if (mode("open") != i) e(18);	/* see if they have */
	if (mode("creat") != i) e(19);	/* the proper mode */
	if (mode("dir") != i) e(20);
	if (mode("fifo") != i) e(21);

	/* Clean up */
	if (close(fd1) != 0) e(22);
	if (close(fd2) != 0) e(23);
	if (unlink("open") != 0) e(24);
	unlink("creat");
	rmdir("dir");
	unlink("fifo");
  }

  /* Check if umask survives a fork() */
  if (umask(0124) != 0000) e(25);
  switch (fork()) {
      case -1:	fprintf(stderr, "Can't fork\n");	break;
      case 0:
	mkdir("bar", 0777);	/* child makes a dir */
	exit(0);
      default:
	if (wait(&stat_loc) == -1) e(26);
  }
  if (umode("bar") != 0124) e(27);
  rmdir("bar");

  /* Check if umask in child changes umask in parent. */
  switch (fork()) {
      case -1:	fprintf(stderr, "Can't fork\n");	break;
      case 0:
	switch (fork()) {
	    case -1:
		fprintf(stderr, "Can't fork\n");
		break;
	    case 0:
		if (umask(0432) != 0124) e(28);
		exit(0);
	    default:
		if (wait(&stat_loc) == -1) e(29);
	}
	if (umask(0423) != 0124) e(30);
	exit(0);
      default:
	if (wait(&stat_loc) == -1) e(31);
  }
  if (umask(0342) != 0124) e(32);

  /* See if extra bits are ignored */
  if (umask(0xFFFF) != 0342) e(33);
  if (umask(0xFE00) != 0777) e(34);
  if (umask(01777) != 0000) e(35);
  if (umask(0022) != 0777) e(36);
}

int mode(arg)
char *arg;
{				/* return the file mode. */
  struct stat st;
  Stat(arg, &st);
  return st.st_mode & 0777;
}

int umode(arg)
char *arg;
{				/* return the umask used for this file */
  return 0777 ^ mode(arg);
}

