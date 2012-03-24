/* Tests for MINIX3 ptrace(2) - by D.C. van Moolenbroek */
#define _POSIX_SOURCE 1
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/ptrace.h>

#define ITERATIONS 3
#define MAX_ERROR 4
#include "common.c"

#define _WIFSTOPPED(s) (WIFSTOPPED(s) && !WIFSIGNALED(s) && !WIFEXITED(s))
#define _WIFSIGNALED(s) (!WIFSTOPPED(s) && WIFSIGNALED(s) && !WIFEXITED(s))
#define _WIFEXITED(s) (!WIFSTOPPED(s) && !WIFSIGNALED(s) && WIFEXITED(s))

#define timed_test(func) (timed_test_func(#func, func));

int main(int argc, char **argv);
void test(int m, int a);
void timed_test_func(const char *s, void (* func)(void));
void timed_test_timeout(int signum);
pid_t traced_fork(void (*c) (void));
pid_t traced_pfork(void (*c) (void));
void WRITE(int value);
int READ(void);
void traced_wait(void);
void detach_running(pid_t pid);
void dummy_handler(int sig);
void exit_handler(int sig);
void count_handler(int sig);
void catch_handler(int sig);
void test_wait_child(void);
void test_wait(void);
void test_exec_child(void);
void test_exec(void);
void test_step_child(void);
void test_step(void);
void test_sig_child(void);
void test_sig(void);
void test_exit_child(void);
void test_exit(void);
void test_term_child(void);
void test_term(void);
void test_catch_child(void);
void test_catch(void);
void test_kill_child(void);
void test_kill(void);
void test_attach_child(void);
void test_attach(void);
void test_detach_child(void);
void test_detach(void);
void test_death_child(void);
void test_death(void);
void test_zdeath_child(void);
void test_zdeath(void);
void test_syscall_child(void);
void test_syscall(void);
void test_tracefork_child(void);
void test_tracefork(void);
void sigexec(int setflag, int opt, int *traps, int *stop);
void test_trapexec(void);
void test_altexec(void);
void test_noexec(void);
void test_defexec(void);
void test_reattach_child(void);
void test_reattach(void);
void my_e(int n);

static char *executable;
static int child = 0, attach;
static pid_t ppid;
static int pfd[4];
static int sigs, caught;

int main(argc, argv)
int argc;
char **argv;
{
  int i, m = 0xFFFFFF, n = 0xF;
  char cp_cmd[NAME_MAX + 10];

  if (strcmp(argv[0], "DO CHECK") == 0) {
	exit(42);
  }

  start(42);

  executable = argv[0];

  snprintf(cp_cmd, sizeof(cp_cmd), "cp ../%s .", executable);
  system(cp_cmd);

  if (argc >= 2) m = atoi(argv[1]);
  if (argc >= 3) n = atoi(argv[2]);

  for (i = 0; i < ITERATIONS; i++) {
	if (n & 001) test(m, 0);
	if (n & 002) test(m, 1);
	if (n & 004) test(m, 2);
	if (n & 010) test(m, 3);
  }

  quit();
  return(-1);			/* impossible */
}

void test(m, a)
int m;
int a;
{
  attach = a;

  if (m & 00000001) timed_test(test_wait);
  if (m & 00000002) timed_test(test_exec);
  if (m & 00000004) timed_test(test_step);
  if (m & 00000010) timed_test(test_sig);
  if (m & 00000020) timed_test(test_exit);
  if (m & 00000040) timed_test(test_term);
  if (m & 00000100) timed_test(test_catch);
  if (m & 00000200) timed_test(test_kill);
  if (m & 00000400) timed_test(test_attach);
  if (m & 00001000) timed_test(test_detach);
  if (m & 00002000) timed_test(test_death);
  if (m & 00004000) timed_test(test_zdeath);
  if (m & 00010000) timed_test(test_syscall);
  if (m & 00020000) timed_test(test_tracefork);
  if (m & 00040000) timed_test(test_trapexec);
  if (m & 00100000) timed_test(test_altexec);
  if (m & 00200000) timed_test(test_noexec);
  if (m & 00400000) timed_test(test_defexec);
  if (m & 01000000) test_reattach(); /* not timed, catches SIGALRM */
}
  
static jmp_buf timed_test_context;

void timed_test_timeout(int signum)
{
  longjmp(timed_test_context, -1);
  my_e(700);
  quit();
  exit(-1);
}

void timed_test_func(const char *s, void (* func)(void))
{
  if (setjmp(timed_test_context) == 0)
  {
    /* the function gets 60 seconds to complete */
    if (signal(SIGALRM, timed_test_timeout) == SIG_ERR) { my_e(701); return; }
    alarm(60);
    func();
    alarm(0);
  }
  else
  {
    /* report timeout as error */
    printf("timeout in %s\n", s);
    my_e(702);
  }
}

pid_t traced_fork(c)
void(*c) (void);
{
  pid_t pid;
  int r, status;

  if (pipe(pfd) != 0) my_e(200);
  if (pipe(&pfd[2]) != 0) my_e(201);

  switch (attach) {
  case 0:			/* let child volunteer to be traced */
  	pid = fork();

  	if (pid < 0) my_e(202);

  	if (pid == 0) {
		child = 1;

		if (ptrace(T_OK, 0, 0, 0) != 0) my_e(203);

		WRITE(0);

		c();

		my_e(204);
  	}

  	if (READ() != 0) my_e(205);

  	break;

  case 1:			/* attach to child process */
	pid = fork();

	if (pid < 0) my_e(206);

	if (pid == 0) {
		child = 1;

		if (READ() != 0) my_e(207);

		c();

		my_e(208);
	}

	if (ptrace(T_ATTACH, pid, 0, 0) != 0) my_e(209);

	if (waitpid(pid, &status, 0) != pid) my_e(210);
	if (!_WIFSTOPPED(status)) my_e(211);
	if (WSTOPSIG(status) != SIGSTOP) my_e(212);

	if (ptrace(T_RESUME, pid, 0, 0) != 0) my_e(213);

	WRITE(0);

	break;

  case 2:			/* attach to non-child process */
	ppid = fork();

	if (ppid < 0) my_e(214);

	if (ppid == 0) {
		pid = fork();

		if (pid < 0) exit(215);

		if (pid == 0) {
			child = 1;

			if (READ() != 0) my_e(216);

			c();

			my_e(217);
		}

		child = 1;

		WRITE(pid);

		if (waitpid(pid, &status, 0) != pid) my_e(218);
		if (_WIFSTOPPED(status)) my_e(219);
		if (_WIFEXITED(status) && (r = WEXITSTATUS(status)) != 42) my_e(r);

		exit(0);
	}

	pid = READ();

	if (ptrace(T_ATTACH, pid, 0, 0) != 0) my_e(220);

	if (waitpid(pid, &status, 0) != pid) my_e(221);
	if (!_WIFSTOPPED(status)) my_e(222);
	if (WSTOPSIG(status) != SIGSTOP) my_e(223);

	if (ptrace(T_RESUME, pid, 0, 0) != 0) my_e(224);

	WRITE(0);

  	break;

  case 3:			/* attach by forking from child */
	ppid = fork();

	if (ppid < 0) my_e(225);

	if (ppid == 0) {
		child = 1;

		if (ptrace(T_OK, 0, 0, 0) != 0) my_e(226);

		WRITE(0);

		if (READ() != 0) my_e(227);

		pid = fork();

		if (pid < 0) my_e(228);

		if (pid == 0) {
			c();

			my_e(229);
		}

		WRITE(pid);

		if (waitpid(pid, &status, 0) != pid) my_e(230);
		if (_WIFSTOPPED(status)) my_e(231);
		if (_WIFEXITED(status) && (r = WEXITSTATUS(status)) != 42) my_e(r);

		exit(0);
	}

	if (READ() != 0) my_e(232);

	if (kill(ppid, SIGSTOP) != 0) my_e(233);

	if (waitpid(ppid, &status, 0) != ppid) my_e(234);
	if (!_WIFSTOPPED(status)) my_e(235);
	if (WSTOPSIG(status) != SIGSTOP) my_e(236);

	if (ptrace(T_SETOPT, ppid, 0, TO_TRACEFORK) != 0) my_e(237);

	if (ptrace(T_RESUME, ppid, 0, 0) != 0) my_e(238);

	WRITE(0);

	pid = READ();

	if (waitpid(pid, &status, 0) != pid) my_e(239);
	if (!_WIFSTOPPED(status)) my_e(240);
	if (WSTOPSIG(status) != SIGSTOP) my_e(241);

	if (ptrace(T_SETOPT, pid, 0, 0) != 0) my_e(242);
	if (ptrace(T_RESUME, pid, 0, 0) != 0) my_e(243);

	detach_running(ppid);

	break;
  }

  return pid;
}

pid_t traced_pfork(c)
void(*c) (void);
{
  pid_t pid;

  if (pipe(pfd) != 0) my_e(300);
  if (pipe(&pfd[2]) != 0) my_e(301);

  pid = fork();

  if (pid < 0) my_e(302);

  if (pid == 0) {
	child = 1;

	c();

	my_e(303);
  }

  return pid;
}

void WRITE(value)
int value;
{
  if (write(pfd[child*2+1], &value, sizeof(value)) != sizeof(value)) my_e(400);
}

int READ()
{
  int value;

  if (read(pfd[2-child*2], &value, sizeof(value)) != sizeof(value)) my_e(401);

  return value;
}

void traced_wait()
{
  int r, status;

  if (attach == 2) {
	if (waitpid(ppid, &status, 0) != ppid) my_e(500);
	if (!_WIFEXITED(status)) my_e(501);
	if ((r = WEXITSTATUS(status)) != 0) my_e(r);
  }
  else {
	/* Quick hack to clean up detached children */
  	waitpid(-1, NULL, WNOHANG);
  }

  close(pfd[0]);
  close(pfd[1]);
  close(pfd[2]);
  close(pfd[3]);
}

void detach_running(pid)
pid_t pid;
{
/* Detach from a process that is not already stopped. This is the way to do it.
 * We have to stop the child in order to detach from it, but as the child may
 * have other signals pending for the tracer, we cannot assume we get our own
 * signal back immediately. However, because we know that the kill is instant
 * and resuming with pending signals will only stop the process immediately
 * again, we can use T_RESUME for all the signals until we get our own signal,
 * and then detach. A complicating factor is that anywhere during this
 * procedure, the child may die (e.g. by getting a SIGKILL). In our tests, this
 * will not happen.
 */
  int status;

  if (kill(pid, SIGSTOP) != 0) my_e(600);

  if (waitpid(pid, &status, 0) != pid) my_e(601);

  while (_WIFSTOPPED(status)) {
	if (WSTOPSIG(status) == SIGSTOP) {
		if (ptrace(T_DETACH, pid, 0, 0) != 0) my_e(602);

		return;
	}

	if (ptrace(T_RESUME, pid, 0, WSTOPSIG(status)) != 0) my_e(603);

	if (waitpid(pid, &status, 0) != pid) my_e(604);
  }

  /* Apparently the process exited. */
  if (!_WIFEXITED(status) && !_WIFSIGNALED(status)) my_e(605);

  /* In our tests, that should not happen. */
  my_e(606);
}

void dummy_handler(sig)
int sig;
{
}

void exit_handler(sig)
int sig;
{
  exit(42);
}

void count_handler(sig)
int sig;
{
  sigs++;
}

void catch_handler(sig)
int sig;
{
  sigset_t set;
  int bit;

  switch (sig) {
  case SIGUSR1: bit = 1; break;
  case SIGUSR2: bit = 2; break;
  case SIGTERM: bit = 4; break;
  default: my_e(100);
  }

  sigfillset(&set);
  sigprocmask(SIG_SETMASK, &set, NULL);

  if (caught & bit) my_e(101);
  caught |= bit;
}

void test_wait_child()
{
  exit(42);
}

void test_wait()
{
  pid_t pid;
  int status;

  subtest = 1;

  pid = traced_fork(test_wait_child);

  if (waitpid(pid, &status, 0) != pid) my_e(1);
  if (!_WIFEXITED(status)) my_e(2);
  if (WEXITSTATUS(status) != 42) my_e(3);

  traced_wait();
}

void test_exec_child()
{
  if (READ() != 0) my_e(100);

  execl(executable, "DO CHECK", NULL);

  my_e(101);
}

void test_exec()
{
  pid_t pid;
  int r, status;

  /* This test covers the T_OK case. */
  if (attach != 0) return;

  subtest = 2;

  pid = traced_fork(test_exec_child);

  WRITE(0);

  /* An exec() should result in a trap signal. */
  if (waitpid(pid, &status, 0) != pid) my_e(1);
  if (!_WIFSTOPPED(status)) my_e(2);
  if (WSTOPSIG(status) != SIGTRAP) my_e(3);

  if (ptrace(T_RESUME, pid, 0, 0) != 0) my_e(4);

  if (waitpid(pid, &status, 0) != pid) my_e(5);
  if (!_WIFEXITED(status)) my_e(6);
  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  traced_wait();
}

void test_step_child()
{
  sigset_t set;

  signal(SIGUSR1, SIG_IGN);

  WRITE(0);

  if (READ() != 0) my_e(100);

  /* It must not be possible for the child to stop the single-step signal. */
  signal(SIGTRAP, SIG_IGN);
  sigfillset(&set);
  sigprocmask(SIG_SETMASK, &set, NULL);

  exit(42);
}

void test_step()
{
  pid_t pid;
  int r, status, count;

  subtest = 3;

  pid = traced_fork(test_step_child);

  if (READ() != 0) my_e(1);

  /* While the child is running, neither waitpid() nor ptrace() should work. */
  if (waitpid(pid, &status, WNOHANG) != 0) my_e(2);
  if (ptrace(T_RESUME, pid, 0, 0) != -1) my_e(3);
  if (errno != EBUSY) my_e(4);

  if (kill(pid, SIGUSR1) != 0) my_e(5);

  WRITE(0);

  /* A kill() signal (other than SIGKILL) should be delivered to the tracer. */
  if (waitpid(pid, &status, 0) != pid) my_e(6);
  if (!_WIFSTOPPED(status)) my_e(7);
  if (WSTOPSIG(status) != SIGUSR1) my_e(8);

  /* ptrace(T_STEP) should result in instruction-wise progress. */
  for (count = 0; ; count++) {
	if (ptrace(T_STEP, pid, 0, 0) != 0) my_e(9);

	if (waitpid(pid, &status, 0) != pid) my_e(10);
	if (_WIFEXITED(status)) break;
	if (!_WIFSTOPPED(status)) my_e(11);
	if (WSTOPSIG(status) != SIGTRAP) my_e(12);
  }

  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  if (count < 10) my_e(13); /* in practice: hundreds */

  traced_wait();
}

void test_sig_child()
{
  signal(SIGUSR1, exit_handler);

  if (READ() != 0) my_e(100);

  pause();

  my_e(101);
}

void test_sig()
{
  pid_t pid;
  int r, sig, status;

  subtest = 4;

  pid = traced_fork(test_sig_child);

  WRITE(0);

  /* allow the child to enter the pause */
  sleep(1);

  if (kill(pid, SIGUSR1) != 0) my_e(1);
  if (kill(pid, SIGUSR2) != 0) my_e(2);

  /* All signals should arrive at the tracer, although in "random" order. */
  if (waitpid(pid, &status, 0) != pid) my_e(3);
  if (!_WIFSTOPPED(status)) my_e(4);
  if (WSTOPSIG(status) != SIGUSR1 && WSTOPSIG(status) != SIGUSR2) my_e(5);

  /* The tracer should see kills arriving while the tracee is stopped. */
  if (kill(pid, WSTOPSIG(status)) != 0) my_e(6);

  if (waitpid(pid, &status, WNOHANG) != pid) my_e(7);
  if (!_WIFSTOPPED(status)) my_e(8);
  if (WSTOPSIG(status) != SIGUSR1 && WSTOPSIG(status) != SIGUSR2) my_e(9);
  sig = (WSTOPSIG(status) == SIGUSR1) ? SIGUSR2 : SIGUSR1;

  if (ptrace(T_RESUME, pid, 0, 0) != 0) my_e(10);

  if (waitpid(pid, &status, 0) != pid) my_e(11);
  if (!_WIFSTOPPED(status)) my_e(12);
  if (WSTOPSIG(status) != sig) my_e(13);

  if (waitpid(pid, &status, WNOHANG) != 0) my_e(14);

  if (ptrace(T_RESUME, pid, 0, 0) != 0) my_e(15);

  /* Ignored signals passed via ptrace() should be ignored. */
  if (kill(pid, SIGUSR1) != 0) my_e(16);

  if (waitpid(pid, &status, 0) != pid) my_e(17);
  if (!_WIFSTOPPED(status)) my_e(18);
  if (WSTOPSIG(status) != SIGUSR1) my_e(19);

  if (ptrace(T_RESUME, pid, 0, SIGCHLD) != 0) my_e(20);

  /* if the pause has been aborted (shouldn't happen!), let the child exit */
  sleep(1);

  if (waitpid(pid, &status, WNOHANG) != 0) my_e(21);

  /* Caught signals passed via ptrace() should invoke their signal handlers. */
  if (kill(pid, SIGUSR1) != 0) my_e(22);

  if (waitpid(pid, &status, 0) != pid) my_e(23);
  if (!_WIFSTOPPED(status)) my_e(24);
  if (WSTOPSIG(status) != SIGUSR1) my_e(25);

  if (ptrace(T_RESUME, pid, 0, SIGUSR1) != 0) my_e(26);

  if (waitpid(pid, &status, 0) != pid) my_e(27);
  if (!_WIFEXITED(status)) my_e(28);
  if ((r = WEXITSTATUS(status)) != 42) my_e(29);

  traced_wait();
}

void test_exit_child()
{
  WRITE(0);

  for(;;);
}

void test_exit()
{
  pid_t pid;
  int r, status;

  subtest = 5;

  pid = traced_fork(test_exit_child);

  if (READ() != 0) my_e(1);

  sleep(1);

  if (kill(pid, SIGSTOP) != 0) my_e(2);

  if (waitpid(pid, &status, 0) != pid) my_e(3);
  if (!_WIFSTOPPED(status)) my_e(4);
  if (WSTOPSIG(status) != SIGSTOP) my_e(5);

  /* There should be no more signals pending for the tracer now. */
  if (waitpid(pid, &status, WNOHANG) != 0) my_e(6);

  /* ptrace(T_EXIT) should terminate the process with the given exit value. */
  if (ptrace(T_EXIT, pid, 0, 42) != 0) my_e(7);

  if (waitpid(pid, &status, 0) != pid) my_e(8);
  if (!_WIFEXITED(status)) my_e(9);
  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  traced_wait();
}

void test_term_child()
{
  signal(SIGUSR1, SIG_DFL);
  signal(SIGUSR2, dummy_handler);

  WRITE(0);

  pause();

  my_e(100);
}

void test_term()
{
  pid_t pid;
  int status;

  subtest = 6;

  pid = traced_fork(test_term_child);

  if (READ() != 0) my_e(1);

  /* If the first of two signals terminates the traced child, the second signal
   * may or may not be delivered to the tracer - this is merely a policy issue.
   * However, nothing unexpected should happen.
   */
  if (kill(pid, SIGUSR1) != 0) my_e(2);
  if (kill(pid, SIGUSR2) != 0) my_e(3);

  if (waitpid(pid, &status, 0) != pid) my_e(4);
  if (!_WIFSTOPPED(status)) my_e(5);

  if (ptrace(T_RESUME, pid, 0, SIGUSR1) != 0) my_e(6);

  if (waitpid(pid, &status, 0) != pid) my_e(7);

  if (_WIFSTOPPED(status)) {
	if (ptrace(T_RESUME, pid, 0, SIGUSR1) != 0) my_e(8);

	if (waitpid(pid, &status, 0) != pid) my_e(9);
  }

  if (!_WIFSIGNALED(status)) my_e(10);
  if (WTERMSIG(status) != SIGUSR1) my_e(11);

  traced_wait();
}

void test_catch_child()
{
  struct sigaction sa;
  sigset_t set, oset;

  sa.sa_handler = catch_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NODEFER;

  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGUSR2, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  sigfillset(&set);
  sigprocmask(SIG_SETMASK, &set, &oset);

  caught = 0;

  WRITE(0);

  while (caught != 7) sigsuspend(&oset);

  exit(42);
}

void test_catch()
{
  pid_t pid;
  int r, sig, status;

  subtest = 7;

  pid = traced_fork(test_catch_child);

  if (READ() != 0) my_e(1);

  if (kill(pid, SIGUSR1) != 0) my_e(2);
  if (kill(pid, SIGUSR2) != 0) my_e(3);

  if (waitpid(pid, &status, 0) != pid) my_e(4);
  if (!_WIFSTOPPED(status)) my_e(5);
  if (WSTOPSIG(status) != SIGUSR1 && WSTOPSIG(status) != SIGUSR2) my_e(6);
  sig = (WSTOPSIG(status) == SIGUSR1) ? SIGUSR2 : SIGUSR1;

  if (ptrace(T_RESUME, pid, 0, WSTOPSIG(status)) != 0) my_e(7);

  if (kill(pid, SIGTERM) != 0) my_e(8);

  if (waitpid(pid, &status, 0) != pid) my_e(9);
  if (!_WIFSTOPPED(status)) my_e(10);
  if (WSTOPSIG(status) != sig && WSTOPSIG(status) != SIGTERM) my_e(11);
  if (WSTOPSIG(status) == sig) sig = SIGTERM;

  if (ptrace(T_RESUME, pid, 0, WSTOPSIG(status)) != 0) my_e(12);

  if (kill(pid, SIGBUS) != 0) my_e(13);

  if (waitpid(pid, &status, 0) != pid) my_e(14);
  if (!_WIFSTOPPED(status)) my_e(15);
  if (WSTOPSIG(status) != sig && WSTOPSIG(status) != SIGBUS) my_e(16);

  if (ptrace(T_RESUME, pid, 0, sig) != 0) my_e(17);

  if (WSTOPSIG(status) == sig) sig = SIGBUS;

  if (waitpid(pid, &status, 0) != pid) my_e(18);
  if (!_WIFSTOPPED(status)) my_e(19);
  if (WSTOPSIG(status) != sig) my_e(20);

  if (ptrace(T_RESUME, pid, 0, 0) != 0) my_e(21);

  if (waitpid(pid, &status, 0) != pid) my_e(22);
  if (!_WIFEXITED(status)) my_e(23);
  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  traced_wait();
}

void test_kill_child()
{
  sigset_t set;

  signal(SIGKILL, SIG_IGN);
  sigfillset(&set);
  sigprocmask(SIG_SETMASK, &set, NULL);

  WRITE(0);

  pause();

  my_e(100);
}

void test_kill()
{
  pid_t pid;
  int status;

  subtest = 8;

  pid = traced_fork(test_kill_child);

  if (READ() != 0) my_e(1);

  /* SIGKILL must be unstoppable in every way. */
  if (kill(pid, SIGKILL) != 0) my_e(2);

  if (waitpid(pid, &status, 0) != pid) my_e(3);
  if (!_WIFSIGNALED(status)) my_e(4);
  if (WTERMSIG(status) != SIGKILL) my_e(5);

  /* After termination, the child must no longer be visible to the tracer. */
  if (waitpid(pid, &status, WNOHANG) != -1) my_e(6);
  if (errno != ECHILD) my_e(7);

  traced_wait();
}

void test_attach_child()
{
  if (ptrace(T_OK, 0, 0, 0) != -1) my_e(100);
  if (errno != EBUSY) my_e(101);

  WRITE(0);

  if (READ() != 0) my_e(102);

  exit(42);
}

void test_attach()
{
  pid_t pid;

  subtest = 9;

  /* Attaching to kernel processes is not allowed. */
  if (ptrace(T_ATTACH, -1, 0, 0) != -1) my_e(1);
  if (errno != ESRCH) my_e(2);

  /* Attaching to self is not allowed. */
  if (ptrace(T_ATTACH, getpid(), 0, 0) != -1) my_e(3);
  if (errno != EPERM) my_e(4);

  /* Attaching to PM is not allowed. */
#if 0
  /* FIXME: disabled until we can reliably determine PM's pid */
  if (ptrace(T_ATTACH, 0, 0, 0) != -1) my_e(5);
  if (errno != EPERM) my_e(6);
#endif

  pid = traced_fork(test_attach_child);

  /* Attaching more than once is not allowed. */
  if (ptrace(T_ATTACH, pid, 0, 0) != -1) my_e(7);
  if (errno != EBUSY) my_e(8);

  if (READ() != 0) my_e(9);

  /* Detaching a running child should not succeed. */
  if (ptrace(T_DETACH, pid, 0, 0) == 0) my_e(10);
  if (errno != EBUSY) my_e(11);

  detach_running(pid);

  WRITE(0);

  traced_wait();
}

void test_detach_child()
{
  struct sigaction sa;
  sigset_t set, sset, oset;

  sa.sa_handler = catch_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NODEFER;

  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGUSR2, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  sigfillset(&set);
  sigprocmask(SIG_SETMASK, &set, &oset);

  sigfillset(&sset);
  sigdelset(&sset, SIGUSR1);

  caught = 0;

  WRITE(0);

  if (sigsuspend(&sset) != -1) my_e(102);
  if (errno != EINTR) my_e(103);

  if (caught != 1) my_e(104);

  if (READ() != 0) my_e(105);

  while (caught != 7) sigsuspend(&oset);

  exit(42);
}

void test_detach()
{
  pid_t pid;
  int r, status;

  /* Can't use traced_fork(), so simplify a bit */
  if (attach != 0) return;

  subtest = 10;

  pid = traced_pfork(test_detach_child);

  if (READ() != 0) my_e(1);

  /* The tracer should not see signals sent to the process before attaching. */
  if (kill(pid, SIGUSR2) != 0) my_e(2);

  if (ptrace(T_ATTACH, pid, 0, 0) != 0) my_e(3);

  if (waitpid(pid, &status, 0) != pid) my_e(4);
  if (!_WIFSTOPPED(status)) my_e(5);
  if (WSTOPSIG(status) != SIGSTOP) my_e(6);

  if (ptrace(T_RESUME, pid, 0, 0) != 0) my_e(7);

  if (kill(pid, SIGUSR1) != 0) my_e(8);

  if (waitpid(pid, &status, 0) != pid) my_e(9);
  if (!_WIFSTOPPED(status)) my_e(10);
  if (WSTOPSIG(status) != SIGUSR1) my_e(11);

  /* Signals pending at the tracer should be passed on after detaching. */
  if (kill(pid, SIGTERM) != 0) my_e(12);

  /* A signal may be passed with the detach request. */
  if (ptrace(T_DETACH, pid, 0, SIGUSR1) != 0) my_e(13);

  WRITE(0);

  if (waitpid(pid, &status, 0) != pid) my_e(14);
  if (!_WIFEXITED(status)) my_e(15);
  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  traced_wait();
}

void test_death_child() 
{
  pid_t pid;

  pid = fork();

  if (pid < 0) my_e(100);

  if (pid == 0) {
	ptrace(T_OK, 0, 0, 0);

	WRITE(getpid());

  	for (;;) pause();
  }

  if (READ() != 0) my_e(101);

  kill(getpid(), SIGKILL);

  my_e(102);
}

void test_death()
{
  pid_t pid, cpid;
  int status;

  subtest = 11;

  pid = traced_fork(test_death_child);

  cpid = READ();

  if (kill(cpid, 0) != 0) my_e(1);

  WRITE(0);

  if (waitpid(pid, &status, 0) != pid) my_e(2);
  if (!_WIFSIGNALED(status)) my_e(3);
  if (WTERMSIG(status) != SIGKILL) my_e(4);

  /* The children of killed tracers should be terminated. */
  while (kill(cpid, 0) == 0) sleep(1);
  if (errno != ESRCH) my_e(5);

  traced_wait();
}

void test_zdeath_child()
{
  if (READ() != 0) my_e(100);

  exit(42);
}

void test_zdeath()
{
  pid_t pid, tpid;
  int r, status;

  /* Can't use traced_fork(), so simplify a bit */
  if (attach != 0) return;

  subtest = 12;

  pid = traced_pfork(test_zdeath_child);

  tpid = fork();

  if (tpid < 0) my_e(1);

  if (tpid == 0) {
	if (ptrace(T_ATTACH, pid, 0, 0) != 0) exit(101);

	if (waitpid(pid, &status, 0) != pid) exit(102);
	if (!_WIFSTOPPED(status)) exit(103);
	if (WSTOPSIG(status) != SIGSTOP) exit(104);

	if (ptrace(T_RESUME, pid, 0, 0) != 0) exit(105);

	WRITE(0);

	/* Unwaited-for traced zombies should be passed to their parent. */
	sleep(2);

	exit(84);
  }

  sleep(1);

  /* However, that should only happen once the tracer has actually died. */
  if (waitpid(pid, &status, WNOHANG) != 0) my_e(2);

  if (waitpid(tpid, &status, 0) != tpid) my_e(3);
  if (!_WIFEXITED(status)) my_e(4);
  if ((r = WEXITSTATUS(status)) != 84) my_e(r);

  if (waitpid(pid, &status, 0) != pid) my_e(5);
  if (!_WIFEXITED(status)) my_e(6);
  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  traced_wait();
}

void test_syscall_child()
{
  signal(SIGUSR1, count_handler);
  signal(SIGUSR2, count_handler);

  sigs = 0;

  WRITE(0);

  if (READ() != 0) my_e(100);

  /* Three calls (may fail) */
  setuid(0);
  close(123);
  getpid();

  if (sigs != 2) my_e(101);

  exit(42);
}

void test_syscall()
{
  pid_t pid;
  int i, r, sig, status;

  subtest = 13;

  pid = traced_fork(test_syscall_child);

  if (READ() != 0) my_e(1);

  if (kill(pid, SIGSTOP) != 0) my_e(2);

  if (waitpid(pid, &status, 0) != pid) my_e(3);
  if (!_WIFSTOPPED(status)) my_e(4);
  if (WSTOPSIG(status) != SIGSTOP) my_e(5);

  WRITE(0);

  /* Upon resuming a first system call, no syscall leave event must be sent. */
  if (ptrace(T_SYSCALL, pid, 0, 0) != 0) my_e(6);

  if (waitpid(pid, &status, 0) != pid) my_e(7);

  for (i = 0; _WIFSTOPPED(status); i++) {
	if (WSTOPSIG(status) != SIGTRAP) my_e(8);

	/* Signals passed via T_SYSCALL should arrive, on enter and exit. */
	if (i == 3) sig = SIGUSR1;
	else if (i == 6) sig = SIGUSR2;
	else sig = 0;

	if (ptrace(T_SYSCALL, pid, 0, sig) != 0) my_e(9);

	if (waitpid(pid, &status, 0) != pid) my_e(10);
  }

  if (!_WIFEXITED(status)) my_e(11);
  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  /* The number of events seen is deterministic but libc-dependent. */
  if (i < 10 || i > 100) my_e(12);

  /* The last system call event must be for entering exit(). */
  if (!(i % 2)) my_e(13);

  traced_wait();
}

void test_tracefork_child()
{
  pid_t pid;

  signal(SIGHUP, SIG_IGN);

  pid = setsid();

  WRITE(pid);

  if (READ() != 0) my_e(100);

  if ((pid = fork()) < 0) my_e(101);

  exit(pid > 0 ? 42 : 84);
}

void test_tracefork()
{
  pid_t pgrp, ppid, cpid, wpid;
  int r, status, gotstop, ptraps, ctraps;

  subtest = 14;

  ppid = traced_fork(test_tracefork_child);

  if ((pgrp = READ()) <= 0) my_e(1);

  if (kill(ppid, SIGSTOP) != 0) my_e(2);

  if (waitpid(ppid, &status, 0) != ppid) my_e(3);
  if (!_WIFSTOPPED(status)) my_e(4);
  if (WSTOPSIG(status) != SIGSTOP) my_e(5);

  if (ptrace(T_SETOPT, ppid, 0, TO_TRACEFORK) != 0) my_e(6);

  WRITE(0);

  if (ptrace(T_SYSCALL, ppid, 0, 0) != 0) my_e(7);

  cpid = -1;
  gotstop = -1;

  /* Count how many traps we get for parent and child, until they both exit. */
  for (ptraps = ctraps = 0; ppid || cpid; ) {
	wpid = waitpid(-pgrp, &status, 0);

	if (wpid <= 0) my_e(8);
	if (cpid < 0 && wpid != ppid) {
		cpid = wpid;
		gotstop = 0;
	}
	if (wpid != ppid && wpid != cpid) my_e(9);

	if (_WIFEXITED(status)) {
		if (wpid == ppid) {
			if ((r = WEXITSTATUS(status)) != 42) my_e(r);
			ppid = 0;
		}
		else {
			if ((r = WEXITSTATUS(status)) != 84) my_e(r);
			cpid = 0;
		}
	}
	else {
		if (!_WIFSTOPPED(status)) my_e(10);

		switch (WSTOPSIG(status)) {
		case SIGCHLD:
		case SIGHUP:
			break;
		case SIGSTOP:
			if (wpid != cpid) my_e(11);
			if (gotstop) my_e(12);
			gotstop = 1;
			break;
		case SIGTRAP: 
			if (wpid == ppid) ptraps++;
			else ctraps++;
			break;
		default:
			my_e(13);
		}

		if (ptrace(T_SYSCALL, wpid, 0, 0) != 0) my_e(14);
	}
  }

  /* The parent should get an odd number of traps: the first one is a syscall
   * enter trap (typically for the fork()), the last one is the syscall enter
   * trap for its exit().
   */
  if (ptraps < 3) my_e(15);
  if (!(ptraps % 2)) my_e(16);

  /* The child should get an even number of traps: the first one is a syscall
   * leave trap from the fork(), the last one is the syscall enter trap for
   * its exit().
   */
  if (ctraps < 2) my_e(17);
  if (ctraps % 2) my_e(18);

  traced_wait();
}

void sigexec(setflag, opt, traps, stop)
int setflag;
int opt;
int *traps;
int *stop;
{
  pid_t pid;
  int r, status;

  pid = traced_fork(test_exec_child);

  if (kill(pid, SIGSTOP) != 0) my_e(1);

  if (waitpid(pid, &status, 0) != pid) my_e(2);
  if (!_WIFSTOPPED(status)) my_e(3);
  if (WSTOPSIG(status) != SIGSTOP) my_e(4);

  if (setflag && ptrace(T_SETOPT, pid, 0, opt) != 0) my_e(5);

  WRITE(0);

  if (ptrace(T_SYSCALL, pid, 0, 0) != 0) my_e(6);

  *traps = 0;
  *stop = -1;

  for (;;) {
  	if (waitpid(pid, &status, 0) != pid) my_e(7);

  	if (_WIFEXITED(status)) break;

  	if (!_WIFSTOPPED(status)) my_e(8);

  	switch (WSTOPSIG(status)) {
  	case SIGTRAP:
		(*traps)++;
		break;
  	case SIGSTOP:
  		if (*stop >= 0) my_e(9);
  		*stop = *traps;
  		break;
  	default:
		my_e(10);
  	}

  	if (ptrace(T_SYSCALL, pid, 0, 0) != 0) my_e(11);
  }

  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  traced_wait();
}

void test_trapexec()
{
  int traps, stop;

  subtest = 15;

  sigexec(1, 0, &traps, &stop);

  /* The exec does not cause a SIGSTOP. This gives us an even number of traps;
   * as above, but plus the exec()'s extra SIGTRAP. This trap is
   * indistinguishable from a syscall trap, especially when considering failed
   * exec() calls and immediately following signal handler invocations.
   */
  if (traps < 4) my_e(12);
  if (traps % 2) my_e(13);
  if (stop >= 0) my_e(14);
}

void test_altexec()
{
  int traps, stop;

  subtest = 16;

  sigexec(1, TO_ALTEXEC, &traps, &stop);

  /* The exec causes a SIGSTOP. This gives us an odd number of traps: a pair
   * for each system call, plus one for the final exit(). The stop must have
   * taken place after a syscall enter event, i.e. must be odd as well.
   */
  if (traps < 3) my_e(12);
  if (!(traps % 2)) my_e(13);
  if (stop < 0) my_e(14);
  if (!(stop % 2)) my_e(15);
}

void test_noexec()
{
  int traps, stop;

  subtest = 17;

  sigexec(1, TO_NOEXEC, &traps, &stop);

  /* The exec causes no signal at all. As above, but without the SIGSTOPs. */
  if (traps < 3) my_e(12);
  if (!(traps % 2)) my_e(13);
  if (stop >= 0) my_e(14);
}

void test_defexec()
{
  int traps, stop;

  /* We want to test the default of T_OK (0) and T_ATTACH (TO_NOEXEC). */
  if (attach != 0 && attach != 1) return;

  subtest = 18;

  /* Do not set any options this time. */
  sigexec(0, 0, &traps, &stop);

  /* See above. */
  if (attach == 0) {
	if (traps < 4) my_e(12);
	if (traps % 2) my_e(13);
	if (stop >= 0) my_e(14);
  }
  else {
	if (traps < 3) my_e(15);
	if (!(traps % 2)) my_e(16);
	if (stop >= 0) my_e(17);
  }
}

void test_reattach_child()
{
  struct timeval tv;

  if (READ() != 0) my_e(100);

  tv.tv_sec = 2;
  tv.tv_usec = 0;
  if (select(0, NULL, NULL, NULL, &tv) != 0) my_e(101);

  exit(42);
}

void test_reattach()
{
  pid_t pid;
  int r, status, count;

  subtest = 19;

  pid = traced_fork(test_reattach_child);

  if (kill(pid, SIGSTOP) != 0) my_e(1);

  if (waitpid(pid, &status, 0) != pid) my_e(2);
  if (!_WIFSTOPPED(status)) my_e(3);
  if (WSTOPSIG(status) != SIGSTOP) my_e(4);

  WRITE(0);

  signal(SIGALRM, dummy_handler);
  alarm(1);

  /* Start tracing system calls. We don't know how many there will be until
   * we reach the child's select(), so we have to interrupt ourselves.
   * The hard assumption here is that the child is able to enter the select()
   * within a second, despite being traced. If this is not the case, the test
   * may hang or fail, and the child may die from a SIGTRAP.
   */
  if (ptrace(T_SYSCALL, pid, 0, 0) != 0) my_e(5);

  for (count = 0; (r = waitpid(pid, &status, 0)) == pid; count++) {
	if (!_WIFSTOPPED(status)) my_e(6);
	if (WSTOPSIG(status) != SIGTRAP) my_e(7);

	if (ptrace(T_SYSCALL, pid, 0, 0) != 0) my_e(8);
  }

  if (r != -1 || errno != EINTR) my_e(9);

  /* We always start with syscall enter event; the last event we should have
   * seen before the alarm was entering the select() call.
   */
  if (!(count % 2)) my_e(10);

  /* Detach, and immediately attach again. */
  detach_running(pid);

  if (ptrace(T_ATTACH, pid, 0, 0) != 0) my_e(11);

  if (waitpid(pid, &status, 0) != pid) my_e(12);
  if (!_WIFSTOPPED(status)) my_e(13);
  if (WSTOPSIG(status) != SIGSTOP) my_e(14);

  if (ptrace(T_SYSCALL, pid, 0, 0) != 0) my_e(15);

  if (waitpid(pid, &status, 0) != pid) my_e(16);

  for (count = 0; _WIFSTOPPED(status); count++) {
	if (WSTOPSIG(status) != SIGTRAP) my_e(17);

	if (ptrace(T_SYSCALL, pid, 0, 0) != 0) my_e(18);

	if (waitpid(pid, &status, 0) != pid) my_e(19);
  }

  if (!_WIFEXITED(status)) my_e(20);
  if ((r = WEXITSTATUS(status)) != 42) my_e(r);

  /* We must not have seen the select()'s syscall leave event, and the last
   * event will be the syscall enter for the exit().
   */
  if (!(count % 2)) my_e(21);

  traced_wait();
}

void my_e(n)
int n;
{

  if (child) exit(n);

  printf("Attach type %d, ", attach);
  e(n);
}

