/* test 10 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

char *name[] = {"t10a", "t10b", "t10c", "t10d", "t10e", "t10f", "t10g", 
						      "t10h", "t10i", "t10j"};
int errct;
long prog[300];
int psize;

_PROTOTYPE(int main, (void));
_PROTOTYPE(void spawn, (int n));
_PROTOTYPE(void mkfiles, (void));
_PROTOTYPE(void cr_file, (char *name, int size));
_PROTOTYPE(void rmfiles, (void));
_PROTOTYPE(void quit, (void));

int main()
{
  int i, n, pid, r;

  printf("Test 10 ");
  fflush(stdout);		/* have to flush for child's benefit */

  system("rm -rf DIR_10; mkdir DIR_10; cp t10a DIR_10");
  chdir("DIR_10");

  pid = getpid();

  /* Create files t10b ... t10h */
  mkfiles();

  if (getpid() == pid)
	if (fork() == 0) {
		execl("t10a", (char *) 0);
		exit(0);
	}
  if (getpid() == pid)
	if (fork() == 0) {
		execl("t10b", (char *) 0);
		exit(0);
	}
  if (getpid() == pid)
	if (fork() == 0) {
		execl("t10c", (char *) 0);
		exit(0);
	}
  if (getpid() == pid)
	if (fork() == 0) {
		execl("t10d", (char *) 0);
		exit(0);
	}

  srand(100);
  for (i = 0; i < 60; i++) {
	r = rand() & 07;
	spawn(r);
  }

  for (i = 0; i < 4; i++) wait(&n);
  rmfiles();
  quit();
  return(-1);			/* impossible */
}

void spawn(n)
int n;
{
  int pid, k;

  if (pid = fork()) {
	wait(&n);		/* wait for some child (any one) */
  } else {
	k = execl(name[n], (char *) 0);
	errct++;
	printf("Child execl didn't take. file=%s errno=%d\n", name[n], errno);
	rmfiles();
	exit(1);
	printf("Worse yet, EXIT didn't exit\n");
  }
}

void mkfiles()
{
  int fd;
  fd = open("t10a", 0);
  if (fd < 0) {
	printf("Can't open t10a\n");
	exit(1);
  }
  psize = read(fd, (char *) prog, 300 * 4);
  cr_file("t10b", 1600);
  cr_file("t10c", 1400);
  cr_file("t10d", 2300);
  cr_file("t10e", 3100);
  cr_file("t10f", 2400);
  cr_file("t10g", 1700);
  cr_file("t10h", 1500);
  cr_file("t10i", 4000);
  cr_file("t10j", 2250);
  close(fd);
}

void cr_file(name, size)
char *name;
int size;

{
  int fd;

#if (CHIP == SPARC)
  size += 4000;
#endif
  prog[6] = (long) size;
  fd = creat(name, 0755);
  write(fd, (char *) prog, psize);
  close(fd);
}

void rmfiles()
{
  unlink("t10b");
  unlink("t10c");
  unlink("t10d");
  unlink("t10e");
  unlink("t10f");
  unlink("t10g");
  unlink("t10h");
  unlink("t10i");
  unlink("t10j");
}

void quit()
{

  chdir("..");
  system("rm -rf DIR*");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
