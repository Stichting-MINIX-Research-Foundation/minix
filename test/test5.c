/* test 5 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define ITERATIONS 2
int max_error = 3;
#include "common.h"


int subtest;
int zero[1024];
int sigmap[5] = {9, 10, 11};


int main(int argc, char *argv[]);
void test5a(void);
void parent(int childpid);
void child(int parpid);
void func1(int s);
void func8(int s);
void func10(int s);
void func11(int s);
void test5b(void);
void test5c(void);
void test5d(void);
void test5e(void);
void test5f(void);
void test5g(void);
void funcalrm(int s);
void test5h(void);
void test5i(void);
void ex(void);

volatile int childsigs, parsigs, alarms;

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0x7777;

  start(5);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test5a();
	if (m & 0002) test5b();
	if (m & 0004) test5c();
	if (m & 0010) test5d();
	if (m & 0020) test5e();
	if (m & 0040) test5f();
	if (m & 0100) test5g();
	if (m & 0200) test5h();
	if (m & 0400) test5i();
  }
  quit();
  return(-1);			/* impossible */
}

void test5a()
{
  int parpid, childpid, flag, *zp;

  subtest = 0;
  flag = 0;
  for (zp = &zero[0]; zp < &zero[1024]; zp++)
	if (*zp != 0) flag = 1;
  if (flag) e(0);		/* check if bss is cleared to 0 */
  if (signal(1, func1) ==  SIG_ERR) e(1);
  if (signal(10, func10) == SIG_ERR) e(2);
  parpid = getpid();
  if ((childpid = fork()) != 0) {
	if (childpid < 0) ex();
	parent(childpid);
  } else {
	child(parpid);
  }
  if (signal(1, SIG_DFL) == SIG_ERR) e(4);
  if (signal(10, SIG_DFL) == SIG_ERR) e(5);
}

void parent(childpid)
int childpid;
{
  int i, pid;

  for (i = 0; i < 3; i++) {
	if (kill(childpid, 1) < 0) e(6);
	while (parsigs == 0);
	parsigs--;
  }
  if ( (pid = wait(&i)) < 0) e(7);
  if (i != 256 * 6) e(8);
}

void child(parpid)
int parpid;
{

  int i;

  for (i = 0; i < 3; i++) {
	while (childsigs == 0);
	childsigs--;
	if (kill(parpid, 10) < 0) e(9);
  }
  exit(6);
}

void func1(s)
int s;				/* for ANSI */
{
  if (signal(1, func1) == SIG_ERR) e(10);
  childsigs++;
}

void func8(s)
int s;
{
}

void func10(s)
int s;				/* for ANSI */
{
  if (signal(10, func10) == SIG_ERR) e(11);
  parsigs++;
}

void func11(s)
int s;				/* for ANSI */
{
  e(38);
}

void test5b()
{
  int cpid, n, pid;

  subtest = 1;
  if ((pid = fork()) != 0) {
	if (pid < 0) ex();
	if ((pid = fork()) != 0) {
		if (pid < 0) ex();
		if ((cpid = fork()) != 0) {
			if (cpid < 0) ex();
			if (kill(cpid, 9) < 0) e(12);
			if (wait(&n) < 0) e(13);
			if (wait(&n) < 0) e(14);
			if (wait(&n) < 0) e(15);
		} else {
			pause();
			while (1);
		}
	} else {
		exit(0);
	}
  } else {
	exit(0);
  }
}

void test5c()
{
  int n, i, pid, wpid;

  /* Test exit status codes for processes killed by signals. */
  subtest = 3;
  for (i = 0; i < 2; i++) {
	if ((pid = fork()) != 0) {
		if (pid < 0) ex();
		sleep(2);	/* wait for child to pause */
		if (kill(pid, sigmap[i]) < 0) {
			e(20);
			exit(1);
		}
		if ((wpid = wait(&n)) < 0) e(21);
		if ((n & 077) != sigmap[i]) e(22);
		if (pid != wpid) e(23);
	} else {
		pause();
		exit(0);
	}
  }
}

void test5d()
{
/* Test alarm */

  int i;

  subtest = 4;
  alarms = 0;
  for (i = 0; i < 8; i++) {
	signal(SIGALRM, funcalrm);
	alarm(1);
	pause();
	if (alarms != i + 1) e(24);
  }
}

void test5e()
{
/* When a signal knocks a processes out of WAIT or PAUSE, it is supposed to
 * get EINTR as error status.  Check that.
 */
  int n;

  subtest = 5;
  if (signal(8, func8) == SIG_ERR) e(25);
  if ((n = fork()) != 0) {
	/* Parent must delay to give child a chance to pause. */
	if (n < 0) ex();
	sleep(1);
	if (kill(n, 8) < 0) e(26);
	if (wait(&n) < 0) e(27);
	if (signal(8, SIG_DFL) == SIG_ERR) e(28);
  } else {
	(void) pause();
	if (errno != EINTR && -errno != EINTR) e(29);
	exit(0);
  }
}

void test5f()
{
  int i, j, k, n;

  subtest = 6;
  if (getuid() != 0) return;
  n = fork();
  if (n < 0) ex();
  if (n) {
	wait(&i);
	i = (i >> 8) & 0377;
	if (i != (n & 0377)) e(30);
  } else {
	i = getgid();
	j = getegid();
	k = (i + j + 7) & 0377;
	if (setgid(k) < 0) e(31);
	if (getgid() != k) e(32);
	if (getegid() != k) e(33);
	i = getuid();
	j = geteuid();
	k = (i + j + 1) & 0377;
	if (setuid(k) < 0) e(34);
	if (getuid() != k) e(35);
	if (geteuid() != k) e(36);
	i = getpid() & 0377;
	if (wait(&j) != -1) e(37);
	exit(i);
  }
}

void test5g()
{
  int n;

  subtest = 7;
  signal(11, func11);
  signal(11, SIG_IGN);
  n = getpid();
  if (kill(n, 11) != 0) e(1);
  signal(11, SIG_DFL);
}

void funcalrm(s)
int s;				/* for ANSI */
{
  alarms++;
}

void test5h()
{
/* When a signal knocks a processes out of PIPE, it is supposed to
 * get EINTR as error status.  Check that.
 */
  int n, fd[2];

  subtest = 8;
  unlink("XXX.test5");
  if (signal(8, func8) == SIG_ERR) e(1);
  pipe(fd);
  if ((n = fork()) != 0) {
	/* Parent must delay to give child a chance to pause. */
	if (n < 0) ex();
	while (access("XXX.test5", 0) != 0) /* just wait */ ;
	sleep(1);
 	unlink("XXX.test5");
	if (kill(n, 8) < 0) e(2);
	if (wait(&n) < 0) e(3);
	if (signal(8, SIG_DFL) == SIG_ERR) e(4);
	if (close(fd[0]) != 0) e(5);
	if (close(fd[1]) != 0) e(6);
  } else {
	if (creat("XXX.test5", 0777) < 0) e(7);
	(void) read(fd[0], (char *) &n, 1);
	if (errno != EINTR) e(8);
	exit(0);
  }
}

void test5i()
{
  int fd[2], pid, buf[10], n;

  subtest = 9;
  pipe(fd);
  unlink("XXXxxxXXX");

  if ((pid = fork()) != 0) {
	/* Parent */
	/* Wait until child has started and has created the XXXxxxXXX file. */
	while (access("XXXxxxXXX", 0) != 0) /* loop */ ;
	sleep(1);
	if (kill(pid, SIGKILL) != 0) e(1);
	if (wait(&n) < 0) e(2);
	if (close(fd[0]) != 0) e(3);
	if (close(fd[1]) != 0) e(4);
  } else {
	if (creat("XXXxxxXXX", 0777) < 0) e(5);
	read(fd[0], (char *) buf, 1);
	e(5);		/* should be killed by signal and not get here */
  }
  unlink("XXXxxxXXX");
}

void ex()
{
  int e = errno;
  printf("Fork failed: %s (%d)\n", strerror(e), e);
  exit(1);
}

