/* test 2 */

#include <sys/types.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

#define ITERATIONS 5
#define MAX_ERROR 4

int is, array[4], parsigs, parcum, sigct, cumsig, subtest;
int iteration, kk = 0;
char buf[2048];

#include "common.c"

int main(int argc, char *argv []);
void test2a(void);
void test2b(void);
void test2c(void);
void test2d(void);
void test2e(void);
void test2f(void);
void test2g(void);
void sigpip(int s);

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  start(2);

  for (i = 0; i < ITERATIONS; i++) {
	iteration = i;
	if (m & 0001) test2a();
	if (m & 0002) test2b();
	if (m & 0004) test2c();
	if (m & 0010) test2d();
	if (m & 0020) test2e();
	if (m & 0040) test2f();
	if (m & 0100) test2g();
  }
  subtest = 100;
  if (cumsig != ITERATIONS) e(101);
  quit();
  return(-1);			/* impossible */
}

void test2a()
{
/* Test pipes */

  int fd[2];
  int n, i, j, q = 0;

  subtest = 1;
  if (pipe(fd) < 0) {
	printf("pipe error.  errno= %d\n", errno);
	errct++;
	quit();
  }
  i = fork();
  if (i < 0) {
	printf("fork failed\n");
	errct++;
	quit();
  }
  if (i != 0) {
	/* Parent code */
	close(fd[0]);
	for (i = 0; i < 2048; i++) buf[i] = i & 0377;
	for (q = 0; q < 8; q++) {
		if (write(fd[1], buf, 2048) < 0) {
			printf("write pipe err.  errno=%d\n", errno);
			errct++;
			quit();
		}
	}
	close(fd[1]);
	wait(&q);
	if (q != 256 * 58) {
		printf("wrong exit code %d\n", q);
		errct++;
		quit();
	}
  } else {
	/* Child code */
	close(fd[1]);
	for (q = 0; q < 32; q++) {
		n = read(fd[0], buf, 512);
		if (n != 512) {
			printf("read yielded %d bytes, not 512\n", n);
			errct++;
			quit();
		}
		for (j = 0; j < n; j++)
			if ((buf[j] & 0377) != (kk & 0377)) {
				printf("wrong data: %d %d %d \n ", 
						j, buf[j] & 0377, kk & 0377);
			} else {
				kk++;
			}
	}
	exit(58);
  }
}

void test2b()
{
  int fd[2], n;
  char buf[4];

  subtest = 2;
  sigct = 0;
  signal(SIGPIPE, sigpip);
  pipe(fd);
  if (fork()) {
	/* Parent */
	close(fd[0]);
	while (sigct == 0) {
		write(fd[1], buf, 1);
	}
	wait(&n);
  } else {
	/* Child */
	close(fd[0]);
	close(fd[1]);
	exit(0);
  }
}

void test2c()
{
  int n;

  subtest = 3;
  signal(SIGINT, SIG_DFL);
  is = 0;
  if ((array[is++] = fork()) > 0) {
	if ((array[is++] = fork()) > 0) {
		if ((array[is++] = fork()) > 0) {
			if ((array[is++] = fork()) > 0) {
				signal(SIGINT, SIG_IGN);
				kill(array[0], SIGINT);
				kill(array[1], SIGINT);
				kill(array[2], SIGINT);
				kill(array[3], SIGINT);
				wait(&n);
				wait(&n);
				wait(&n);
				wait(&n);
			} else {
				pause();
			}
		} else {
			pause();
		}
	} else {
		pause();
	}
  } else {
	pause();
  }
}

void test2d()
{

  int pid, stat_loc, s;

  /* Test waitpid. */
  subtest = 4;

  /* Test waitpid(pid, arg2, 0) */
  pid = fork();
  if (pid < 0) e(1);
  if (pid > 0) {
	/* Parent. */
	s = waitpid(pid, &stat_loc, 0);
	if (s != pid) e(2);
	if (WIFEXITED(stat_loc) == 0) e(3);
	if (WIFSIGNALED(stat_loc) != 0) e(4);
	if (WEXITSTATUS(stat_loc) != 22) e(5);
  } else {
	/* Child */
	exit(22);
  }

  /* Test waitpid(-1, arg2, 0) */
  pid = fork();
  if (pid < 0) e(6);
  if (pid > 0) {
	/* Parent. */
	s = waitpid(-1, &stat_loc, 0);
	if (s != pid) e(7);
	if (WIFEXITED(stat_loc) == 0) e(8);
	if (WIFSIGNALED(stat_loc) != 0) e(9);
	if (WEXITSTATUS(stat_loc) != 33) e(10);
  } else {
	/* Child */
	exit(33);
  }

  /* Test waitpid(0, arg2, 0) */
  pid = fork();
  if (pid < 0) e(11);
  if (pid > 0) {
	/* Parent. */
	s = waitpid(0, &stat_loc, 0);
	if (s != pid) e(12);
	if (WIFEXITED(stat_loc) == 0) e(13);
	if (WIFSIGNALED(stat_loc) != 0) e(14);
	if (WEXITSTATUS(stat_loc) != 44) e(15);
  } else {
	/* Child */
	exit(44);
  }

  /* Test waitpid(0, arg2, WNOHANG) */
  signal(SIGTERM, SIG_DFL);
  pid = fork();
  if (pid < 0) e(16);
  if (pid > 0) {
	/* Parent. */
	s = waitpid(0, &stat_loc, WNOHANG);
	if (s != 0) e(17);
	if (kill(pid, SIGTERM) != 0) e(18);
	if (waitpid(pid, &stat_loc, 0) != pid) e(19);
	if (WIFEXITED(stat_loc) != 0) e(20);
	if (WIFSIGNALED(stat_loc) == 0) e(21);
	if (WTERMSIG(stat_loc) != SIGTERM) e(22);
  } else {
	/* Child */
	pause();
  }

  /* Test some error conditions. */
  errno = 9999;
  if (waitpid(0, &stat_loc, 0) != -1) e(23);
  if (errno != ECHILD) e(24);
  errno = 9999;
  if (waitpid(0, &stat_loc, WNOHANG) != -1) e(25);
  if (errno != ECHILD) e(26);
}

void test2e()
{

  int pid1, pid2, stat_loc, s;

  /* Test waitpid with two children. */
  subtest = 5;
  if (iteration > 1) return;		/* slow test, don't do it too much */
  if ( (pid1 = fork())) {
	/* Parent. */
	if ( (pid2 = fork()) ) {
		/* Parent. Collect second child first. */
		s = waitpid(pid2, &stat_loc, 0);
		if (s != pid2) e(1);
		if (WIFEXITED(stat_loc) == 0) e(2);
		if (WIFSIGNALED(stat_loc) != 0) e(3);
		if (WEXITSTATUS(stat_loc) != 222) e(4);

		/* Now collect first child. */
		s = waitpid(pid1, &stat_loc, 0);
		if (s != pid1) e(5);
		if (WIFEXITED(stat_loc) == 0) e(6);
		if (WIFSIGNALED(stat_loc) != 0) e(7);
		if (WEXITSTATUS(stat_loc) != 111) e(8);
	} else {
		/* Child 2. */
		sleep(2);		/* child 2 delays before exiting. */
		exit(222);
	}
  } else {
	/* Child 1. */
	exit(111);			/* child 1 exits immediately */
  }

}

void test2f()
{
/* test getpid, getppid, getuid, etc. */

  pid_t pid, pid1, ppid, cpid, stat_loc, err;

  subtest = 6;
  errno = -2000;
  err = 0;
  pid = getpid();
  if ( (pid1 = fork())) {
	/* Parent.  Do nothing. */
	if (wait(&stat_loc) != pid1) e(1);
	if (WEXITSTATUS(stat_loc) != (pid1 & 0377)) e(2);
  } else {
	/* Child.  Get ppid. */
	cpid = getpid();
	ppid = getppid();
	if (ppid != pid) err = 3;
	if (cpid == ppid) err = 4;
	exit(cpid & 0377);
  }
  if (err != 0) e(err);
}

void test2g()
{
/* test time(), times() */

  time_t t1, t2;
  clock_t t3, t4;
  struct tms tmsbuf;

  subtest = 7;
  errno = -7000;

  /* First time(). */
  t1 = -1;
  t2 = -2;
  t1 = time(&t2);
  if (t1 < 650000000L) e(1);	/* 650000000 is Sept. 1990 */
  if (t1 != t2) e(2);
  t1 = -1;
  t1 = time( (time_t *) NULL);
  if (t1 < 650000000L) e(3);
  t3 = times(&tmsbuf);
  sleep(1);
  t2 = time( (time_t *) NULL);
  if (t2 < 0L) e(4);
  if (t2 - t1 < 1) e(5);

  /* Now times(). */
  t4 = times(&tmsbuf);
  if ( t4 == (clock_t) -1) e(6);
  if (t4 - t3 < CLOCKS_PER_SEC) e(7);
  if (tmsbuf.tms_utime < 0) e(8);
  if (tmsbuf.tms_stime < 0) e(9);
  if (tmsbuf.tms_cutime < 0) e(10);
  if (tmsbuf.tms_cstime < 0) e(11);
}

void sigpip(s)
int s;				/* for ANSI */
{
  sigct++;
  cumsig++;
}

