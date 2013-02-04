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

#define PROGBUF_LONGS 3000
long prog[PROGBUF_LONGS];
int psize;

#define MAX_ERROR 2
#include "common.c"

int main(void);
void spawn(int n);
void mkfiles(void);
void cr_file(char *name, int size);
void rmfiles(void);
void quit(void);

int main()
{
  int i, n, pid, r;

  start(10);
  system("cp ../t10a .");
  pid = getpid();

  /* Create files t10b ... t10h */
  mkfiles();

  if (getpid() == pid)
	if (fork() == 0) {
		execl("t10a", "t10a", (char *) 0);
		exit(0);
	}
  if (getpid() == pid)
	if (fork() == 0) {
		execl("t10b", "t10b", (char *) 0);
		exit(0);
	}
  if (getpid() == pid)
	if (fork() == 0) {
		execl("t10c", "t10c", (char *) 0);
		exit(0);
	}
  if (getpid() == pid)
	if (fork() == 0) {
		execl("t10d", "t10d", (char *) 0);
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
  int pid;

  if ((pid = fork()) != 0) {
	wait(&n);		/* wait for some child (any one) */
  } else {
	/* a successful exec or a successful detection of a broken executable
	 * is ok
	 */
	if(execl(name[n], name[n], (char *) 0) < 0 && errno == ENOEXEC)
		exit(0);
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
  psize = read(fd, (char *) prog, PROGBUF_LONGS * 4);
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

  size += 3000;
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

