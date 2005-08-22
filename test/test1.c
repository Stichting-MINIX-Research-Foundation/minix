/* test 1 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define SIGNUM 10
#define MAX_ERROR 4
#define ITERATIONS 10

_VOLATILE int glov, gct;
int errct;
int subtest;

_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(void test1a, (void));
_PROTOTYPE(void parent, (void));
_PROTOTYPE(void child, (int i));
_PROTOTYPE(void test1b, (void));
_PROTOTYPE(void parent1, (int childpid));
_PROTOTYPE(void func, (int s));
_PROTOTYPE(void child1, (void));
_PROTOTYPE(void e, (int n));
_PROTOTYPE(void quit, (void));

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  sync();

  if (argc == 2) m = atoi(argv[1]);

  printf("Test  1 ");
  fflush(stdout);		/* have to flush for child's benefit */

  system("rm -rf DIR_01; mkdir DIR_01");
  chdir("DIR_01");

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 00001) test1a();
	if (m & 00002) test1b();
  }

  quit();
  return(-1);			/* impossible */
}

void test1a()
{
  int i, n, pid;

  subtest = 1;
  n = 4;
  for (i = 0; i < n; i++) {
	if ((pid = fork())) {
		if (pid < 0) {
			printf("\nTest 1 fork failed\n");
			exit(1);
		}
		parent();
	} else
		child(i);
  }
}

void parent()
{

  int n;

  n = getpid();
  wait(&n);
}

void child(i)
int i;
{
  int n;

  n = getpid();
  exit(100+i);
}

void test1b()
{
  int i, k;

  subtest = 2;
  for (i = 0; i < 4; i++) {
	glov = 0;
	signal(SIGNUM, func);
	if ((k = fork())) {
		if (k < 0) {
			printf("Test 1 fork failed\n");
			exit(1);
		}
		parent1(k);
	} else
		child1();
  }
}

void parent1(childpid)
int childpid;
{

  int n;

  for (n = 0; n < 5000; n++);
  while (kill(childpid, SIGNUM) < 0)	/* null statement */
	;
  wait(&n);
}

void func(s)
int s;				/* for ANSI */
{
  glov++;
  gct++;
}

void child1()
{
  while (glov == 0);
  exit(gct);
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
