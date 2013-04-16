/* test 37 - signals */

#include <sys/types.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define ITERATIONS 2
#define SIGS 14
int max_error = 4;
#include "common.h"



int iteration, cumsig, sig1, sig2;

int sigarray[SIGS] = {SIGHUP, SIGILL, SIGTRAP, SIGABRT, SIGIOT, 
	      SIGFPE, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM,
	      SIGTERM};

/* Prototypes produced automatically by mkptypes. */
int main(int argc, char *argv []);
void test37a(void);
void func1(int sig);
void func2(int sig);
void test37b(void);
void catch1(int signo);
void catch2(int signo);
void test37c(void);
void catch3(int signo);
void test37d(void);
void catch4(int signo);
void test37e(void);
void catch5(int signo);
void test37f(void);
void sigint_handler(int signo);
void sigpipe_handler(int signo);
void test37g(void);
void sighup8(int signo);
void sigpip8(int signo);
void sigter8(int signo);
void test37h(void);
void sighup9(int signo);
void sigter9(int signo);
void test37i(void);
void sighup10(int signo);
void sigalrm_handler10(int signo);
void test37j(void);
void test37k(void);
void test37l(void);
void func_m1(void);
void func_m2(void);
void test37m(void);
void test37p(void);
void test37q(void);
void longjerr(void);
void catch14(int signo, int code, struct sigcontext * scp);
void test37n(void);
void catch15(int signo);
void test37o(void);
void clearsigstate(void);
void wait_for(int pid);

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0377777;

  sync();

  start(37);

  if (argc == 2) m = atoi(argv[1]);

  for (i = 0; i < ITERATIONS; i++) {
	iteration = i;
	if (m & 0000001) test37a();
	if (m & 0000002) test37b();
	if (m & 0000004) test37c();
	if (m & 0000010) test37d();
	if (m & 0000020) test37e();
	if (m & 0000040) test37f();
	if (m & 0000100) test37g();
	if (m & 0000200) test37h();
	if (m & 0000400) test37i();
	if (m & 0001000) test37j();
	if (m & 0002000) test37k();
	if (m & 0004000) test37l();
	if (m & 0010000) test37m();
	if (m & 0020000) test37n();
	if (m & 0040000) test37o();
	if (m & 0100000) test37p();
	if (m & 0200000) test37q();
  }

  quit();

  return(-1);	/* Unreachable */
}

void test37a()
{
/* Test signal set management. */

  sigset_t s;

  subtest = 1;
  clearsigstate();

  /* Create an empty set and see if any bits are on. */
  if (sigemptyset(&s) != 0) e(1);
  if (sigismember(&s, SIGHUP) != 0) e(2);
  if (sigismember(&s, SIGINT) != 0) e(3);
  if (sigismember(&s, SIGQUIT) != 0) e(4);
  if (sigismember(&s, SIGILL) != 0) e(5);
  if (sigismember(&s, SIGTRAP) != 0) e(6);
  if (sigismember(&s, SIGABRT) != 0) e(7);
  if (sigismember(&s, SIGIOT) != 0) e(8);
  if (sigismember(&s, SIGFPE) != 0) e(10);
  if (sigismember(&s, SIGKILL) != 0) e(11);
  if (sigismember(&s, SIGUSR1) != 0) e(12);
  if (sigismember(&s, SIGSEGV) != 0) e(13);
  if (sigismember(&s, SIGUSR2) != 0) e(14);
  if (sigismember(&s, SIGPIPE) != 0) e(15);
  if (sigismember(&s, SIGALRM) != 0) e(16);
  if (sigismember(&s, SIGTERM) != 0) e(17);

  /* Create a full set and see if any bits are off. */
  if (sigfillset(&s) != 0) e(19);
  if (sigemptyset(&s) != 0) e(20);
  if (sigfillset(&s) != 0) e(21);
  if (sigismember(&s, SIGHUP) != 1) e(22);
  if (sigismember(&s, SIGINT) != 1) e(23);
  if (sigismember(&s, SIGQUIT) != 1) e(24);
  if (sigismember(&s, SIGILL) != 1) e(25);
  if (sigismember(&s, SIGTRAP) != 1) e(26);
  if (sigismember(&s, SIGABRT) != 1) e(27);
  if (sigismember(&s, SIGIOT) != 1) e(28);
  if (sigismember(&s, SIGFPE) != 1) e(30);
  if (sigismember(&s, SIGKILL) != 1) e(31);
  if (sigismember(&s, SIGUSR1) != 1) e(32);
  if (sigismember(&s, SIGSEGV) != 1) e(33);
  if (sigismember(&s, SIGUSR2) != 1) e(34);
  if (sigismember(&s, SIGPIPE) != 1) e(35);
  if (sigismember(&s, SIGALRM) != 1) e(36);
  if (sigismember(&s, SIGTERM) != 1) e(37);

  /* Create an empty set, then turn on bits individually. */
  if (sigemptyset(&s) != 0) e(39);
  if (sigaddset(&s, SIGHUP) != 0) e(40);
  if (sigaddset(&s, SIGINT) != 0) e(41);
  if (sigaddset(&s, SIGQUIT) != 0) e(42);
  if (sigaddset(&s, SIGILL) != 0) e(43);
  if (sigaddset(&s, SIGTRAP) != 0) e(44);

  /* See if the bits just turned on are indeed on. */
  if (sigismember(&s, SIGHUP) != 1) e(45);
  if (sigismember(&s, SIGINT) != 1) e(46);
  if (sigismember(&s, SIGQUIT) != 1) e(47);
  if (sigismember(&s, SIGILL) != 1) e(48);
  if (sigismember(&s, SIGTRAP) != 1) e(49);

  /* The others should be turned off. */
  if (sigismember(&s, SIGABRT) != 0) e(50);
  if (sigismember(&s, SIGIOT) != 0) e(51);
  if (sigismember(&s, SIGFPE) != 0) e(53);
  if (sigismember(&s, SIGKILL) != 0) e(54);
  if (sigismember(&s, SIGUSR1) != 0) e(55);
  if (sigismember(&s, SIGSEGV) != 0) e(56);
  if (sigismember(&s, SIGUSR2) != 0) e(57);
  if (sigismember(&s, SIGPIPE) != 0) e(58);
  if (sigismember(&s, SIGALRM) != 0) e(59);
  if (sigismember(&s, SIGTERM) != 0) e(60);

  /* Now turn them off and see if all are off. */
  if (sigdelset(&s, SIGHUP) != 0) e(62);
  if (sigdelset(&s, SIGINT) != 0) e(63);
  if (sigdelset(&s, SIGQUIT) != 0) e(64);
  if (sigdelset(&s, SIGILL) != 0) e(65);
  if (sigdelset(&s, SIGTRAP) != 0) e(66);

  if (sigismember(&s, SIGHUP) != 0) e(67);
  if (sigismember(&s, SIGINT) != 0) e(68);
  if (sigismember(&s, SIGQUIT) != 0) e(69);
  if (sigismember(&s, SIGILL) != 0) e(70);
  if (sigismember(&s, SIGTRAP) != 0) e(71);
  if (sigismember(&s, SIGABRT) != 0) e(72);
  if (sigismember(&s, SIGIOT) != 0) e(73);
  if (sigismember(&s, SIGFPE) != 0) e(75);
  if (sigismember(&s, SIGKILL) != 0) e(76);
  if (sigismember(&s, SIGUSR1) != 0) e(77);
  if (sigismember(&s, SIGSEGV) != 0) e(78);
  if (sigismember(&s, SIGUSR2) != 0) e(79);
  if (sigismember(&s, SIGPIPE) != 0) e(80);
  if (sigismember(&s, SIGALRM) != 0) e(81);
  if (sigismember(&s, SIGTERM) != 0) e(82);
}

void func1(sig)
int sig;
{
  sig1++;
}

void func2(sig)
int sig;
{
  sig2++;
}

void test37b()
{
/* Test sigprocmask and sigpending. */
  int i;
  pid_t p;
  sigset_t s, s1, s_empty, s_full, s_ill, s_ill_pip, s_nokill, s_nokill_stop;
  struct sigaction sa, osa;

  subtest = 2;
  clearsigstate();

  /* Construct s_ill = {SIGILL} and s_ill_pip {SIGILL | SIGPIP}, etc. */
  if (sigemptyset(&s_empty) != 0) e(1);
  if (sigemptyset(&s_ill) != 0) e(2);
  if (sigemptyset(&s_ill_pip) != 0) e(3);
  if (sigaddset(&s_ill, SIGILL) != 0) e(4);
  if (sigaddset(&s_ill_pip, SIGILL) != 0) e(5);
  if (sigaddset(&s_ill_pip, SIGPIPE) != 0) e(6);
  if (sigfillset(&s_full) != 0) e(7);
  s_nokill = s_full;
  if (sigdelset(&s_nokill, SIGKILL) != 0) e(8);
  s_nokill_stop = s_nokill;
  if (sigdelset(&s_nokill_stop, SIGSTOP) != 0) e(8);
  if (SIGSTOP >= _NSIG) e(666);
  if (SIGSTOP < _NSIG && sigdelset(&s_nokill, SIGSTOP) != 0) e(888);

  /* Now get most of the signals into default state.  Don't change SIGINT
  * or SIGQUIT, so this program can be killed.  SIGKILL is also special.
  */
  sa.sa_handler = SIG_DFL;
  sa.sa_mask = s_empty;
  sa.sa_flags = 0;
  for (i = 0; i < SIGS; i++) sigaction(i, &sa, &osa);

  /* The second argument may be zero.  See if it wipes out the system. */
  for (i = 0; i < SIGS; i++) sigaction(i, (struct sigaction *) NULL, &osa);

  /* Install a signal handler. */
  sa.sa_handler = func1;
  sa.sa_mask = s_ill;
  sa.sa_flags = SA_NODEFER | SA_NOCLDSTOP;
  osa.sa_handler = SIG_IGN;
  osa.sa_mask = s_empty;
  osa.sa_flags = 0;
  if (sigaction(SIGHUP, &sa, &osa) != 0) e(9);
  if (osa.sa_handler != SIG_DFL) e(10);
  if (osa.sa_mask != 0) e(11);
  if (osa.sa_flags != s_empty) e(12);

  /* Replace action and see if old value is read back correctly. */
  sa.sa_handler = func2;
  sa.sa_mask = s_ill_pip;
  sa.sa_flags = SA_RESETHAND | SA_NODEFER;
  osa.sa_handler = SIG_IGN;
  osa.sa_mask = s_empty;
  osa.sa_flags = 0;
  if (sigaction(SIGHUP, &sa, &osa) != 0) e(13);
  if (osa.sa_handler != func1) e(14);
  if (osa.sa_mask != s_ill) e(15);
  if (osa.sa_flags != SA_NODEFER
      && osa.sa_flags != (SA_NODEFER | SA_NOCLDSTOP)) e(16);

  /* Replace action once more and check what is read back. */
  sa.sa_handler = SIG_DFL;
  sa.sa_mask = s_empty;
  osa.sa_handler = SIG_IGN;
  osa.sa_mask = s_empty;
  osa.sa_flags = 0;
  if (sigaction(SIGHUP, &sa, &osa) != 0) e(17);
  if (osa.sa_handler != func2) e(18);
  if (osa.sa_mask != s_ill_pip) e(19);
  if (osa.sa_flags != (SA_RESETHAND | SA_NODEFER)) e(20);

  /* Test sigprocmask(SIG_SETMASK, ...). */
  if (sigprocmask(SIG_SETMASK, &s_full, &s1) != 0) e(18);    /* block all */
  if (sigemptyset(&s1) != 0) e(19);
  errno = 0;
  if (sigprocmask(SIG_SETMASK, &s_empty, &s1) != 0) e(20);   /* block none */
  if (s1 != s_nokill_stop) e(21);
  if (sigprocmask(SIG_SETMASK, &s_ill, &s1) != 0) e(22);     /* block SIGILL */
  errno = 0;
  if (s1 != s_empty) e(23);
  if (sigprocmask(SIG_SETMASK, &s_ill_pip, &s1) != 0) e(24); /* SIGILL+PIP */
  if (s1 != s_ill) e(25);
  if (sigprocmask(SIG_SETMASK, &s_full, &s1) != 0) e(26);    /* block all */
  if (s1 != s_ill_pip) e(27);

  /* Test sigprocmask(SIG_UNBLOCK, ...) */
  if (sigprocmask(SIG_UNBLOCK, &s_ill, &s1) != 0) e(28);
  if (s1 != s_nokill_stop) e(29);
  if (sigprocmask(SIG_UNBLOCK, &s_ill_pip, &s1) != 0) e(30);
  s = s_nokill_stop;
  if (sigdelset(&s, SIGILL) != 0) e(31);
  if (s != s1) e(32);
  if (sigprocmask(SIG_UNBLOCK, &s_empty, &s1) != 0) e(33);
  s = s_nokill_stop;
  if (sigdelset(&s, SIGILL) != 0) e(34);
  if (sigdelset(&s, SIGPIPE) != 0) e(35);
  if (s != s1) e(36);
  s1 = s_nokill_stop;
  if (sigprocmask(SIG_SETMASK, &s_empty, &s1) != 0) e(37);
  if (s != s1) e(38);

  /* Test sigprocmask(SIG_BLOCK, ...) */
  if (sigprocmask(SIG_BLOCK, &s_ill, &s1) != 0) e(39);
  if (s1 != s_empty) e(40);
  if (sigprocmask(SIG_BLOCK, &s_ill_pip, &s1) != 0) e(41);
  if (s1 != s_ill) e(42);
  if (sigprocmask(SIG_SETMASK, &s_full, &s1) != 0) e(43);
  if (s1 != s_ill_pip) e(44);

  /* Check error condition. */
  errno = 0;
  if (sigprocmask(20000, &s_full, &s1) != -1) e(45);
  if (errno != EINVAL) e(46);
  if (sigprocmask(SIG_SETMASK, &s_full, &s1) != 0) e(47);
  if (s1 != s_nokill_stop) e(48);

  /* If second arg is 0, nothing is set. */
  if (sigprocmask(SIG_SETMASK, (sigset_t *) NULL, &s1) != 0) e(49);
  if (s1 != s_nokill_stop) e(50);
  if (sigprocmask(SIG_SETMASK, &s_ill_pip, &s1) != 0) e(51);
  if (s1 != s_nokill_stop) e(52);
  if (sigprocmask(SIG_SETMASK, (sigset_t *) NULL, &s1) != 0) e(53);
  if (s1 != s_ill_pip) e(54);
  if (sigprocmask(SIG_BLOCK, (sigset_t *) NULL, &s1) != 0) e(55);
  if (s1 != s_ill_pip) e(56);
  if (sigprocmask(SIG_UNBLOCK, (sigset_t *) NULL, &s1) != 0) e(57);
  if (s1 != s_ill_pip) e(58);

  /* Trying to block SIGKILL is not allowed, but is not an error, either. */
  s = s_empty;
  if (sigaddset(&s, SIGKILL) != 0) e(59);
  if (sigprocmask(SIG_BLOCK, &s, &s1) != 0) e(60);
  if (s1 != s_ill_pip) e(61);
  if (sigprocmask(SIG_SETMASK, &s_full, &s1) != 0) e(62);
  if (s1 != s_ill_pip) e(63);

  /* Test sigpending. At this moment, all signals are blocked. */
  sa.sa_handler = func2;
  sa.sa_mask = s_empty;
  if (sigaction(SIGHUP, &sa, &osa) != 0) e(64);
  p = getpid();
  kill(p, SIGHUP);		/* send SIGHUP to self */
  if (sigpending(&s) != 0) e(65);
  if (sigemptyset(&s1) != 0) e(66);
  if (sigaddset(&s1, SIGHUP) != 0) e(67);
  if (s != s1) e(68);
  sa.sa_handler = SIG_IGN;
  if (sigaction(SIGHUP, &sa, &osa) != 0) e(69);
  if (sigpending(&s) != 0) e(70);
  if (s != s_empty) e(71);
}

/*---------------------------------------------------------------------------*/
int x;
sigset_t glo_vol_set;

void catch1(signo)
int signo;
{
  x = 42;
}

void catch2(signo)
int signo;
{
  if (sigprocmask(SIG_BLOCK, (sigset_t *)NULL, (sigset_t *) &glo_vol_set) != 0)
	e(1);
}

/* Verify that signal(2), which is now built on top of sigaction(2), still
* works.
*/
void test37c()
{
  pid_t pid;
  sigset_t sigset_var;

  subtest = 3;
  clearsigstate();
  x = 0;

  /* Verify an installed signal handler persists across a fork(2). */
  if (signal(SIGTERM, catch1) == SIG_ERR) e(1);
  switch (pid = fork()) {
      case 0:			/* child */
	errct = 0;
	while (x == 0);
	if (x != 42) e(2);
	exit(errct == 0 ? 0 : 1);
      case -1:	e(3);	break;
      default:			/* parent */
	sleep(1);
	if (kill(pid, SIGTERM) != 0) e(4);
	wait_for(pid);
	break;
  }

  /* Verify that the return value is the previous handler. */
  signal(SIGINT, SIG_IGN);
  if (signal(SIGINT, catch2) != SIG_IGN) e(5);
  if (signal(SIGINT, catch1) != catch2) e(6);
  if (signal(SIGINT, SIG_DFL) != catch1) e(7);
  if (signal(SIGINT, catch1) != SIG_DFL) e(8);
  if (signal(SIGINT, SIG_DFL) != catch1) e(9);
  if (signal(SIGINT, SIG_DFL) != SIG_DFL) e(10);
  if (signal(SIGINT, catch1) != SIG_DFL) e(11);

  /* Verify that SIG_ERR is correctly generated. */
  if (signal(_NSIG, catch1) != SIG_ERR) e(12);
  if (signal(0, catch1) != SIG_ERR) e(13);
  if (signal(-1, SIG_DFL) != SIG_ERR) e(14);

  /* Verify that caught signals are automatically reset to the default,
   * and that further instances of the same signal are not blocked here
   * or in the signal handler.
   */
  if (signal(SIGTERM, catch1) == SIG_ERR) e(15);
  switch ((pid = fork())) {
      case 0:			/* child */
	errct = 0;
	while (x == 0);
	if (x != 42) e(16);
	if (sigismember((sigset_t *) &glo_vol_set, SIGTERM)) e(17);
	if (sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &sigset_var) != 0) e(18);
	if (sigismember(&sigset_var, SIGTERM)) e(19);

#if 0
/* Use this if you have compiled signal() to have the broken SYSV behaviour. */
	if (signal(SIGTERM, catch1) != SIG_DFL) e(20);
#else
	if (signal(SIGTERM, catch1) != catch1) e(20);
#endif
	exit(errct == 0 ? 0 : 1);
      default:			/* parent */
	sleep(1);
	if (kill(pid, SIGTERM) != 0) e(21);
	wait_for(pid);
	break;
      case -1:	e(22);	break;
  }
}

/*---------------------------------------------------------------------------*/
/* Test that the signal handler can be invoked recursively with the
* state being properly saved and restored.
*/

static int y;
static int z;

void catch3(signo)
int signo;
{
  if (z == 1) {			/* catching a nested signal */
	y = 2;
	return;
  }
  z = 1;
  if (kill(getpid(), SIGHUP) != 0) e(1);
  while (y != 2);
  y = 1;
}

void test37d()
{
  struct sigaction act;

  subtest = 4;
  clearsigstate();
  y = 0;
  z = 0;

  act.sa_handler = catch3;
  act.sa_mask = 0;
  act.sa_flags = SA_NODEFER;	/* Otherwise, nested occurence of
				 * SIGINT is blocked. */
  if (sigaction(SIGHUP, &act, (struct sigaction *) NULL) != 0) e(2);
  if (kill(getpid(), SIGHUP) != 0) e(3);
  if (y != 1) e(4);
}

/*---------------------------------------------------------------------------*/

/* Test that the signal mask in effect for the duration of a signal handler
* is as specified in POSIX Section 3, lines 718 -724.  Test that the
* previous signal mask is restored when the signal handler returns.
*/

void catch4(signo)
int signo;
{
  sigset_t oset;
  sigset_t set;

  if (sigemptyset(&set) == -1) e(5001);
  if (sigaddset(&set, SIGTERM) == -1) e(5002);
  if (sigaddset(&set, SIGHUP) == -1) e(5003);
  if (sigaddset(&set, SIGINT) == -1) e(5004);
  if (sigaddset(&set, SIGPIPE) == -1) e(5005);
  if (sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &oset) != 0) e(5006);
  if (oset != set) e(5007);
}

void test37e()
{
  struct sigaction act, oact;
  sigset_t set, oset;

  subtest = 5;
  clearsigstate();

  act.sa_handler = catch4;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGTERM);
  sigaddset(&act.sa_mask, SIGHUP);
  act.sa_flags = 0;
  if (sigaction(SIGINT, &act, &oact) == -1) e(2);

  if (sigemptyset(&set) == -1) e(3);
  if (sigaddset(&set, SIGPIPE) == -1) e(4);
  if (sigprocmask(SIG_SETMASK, &set, &oset) == -1) e(5);
  if (kill(getpid(), SIGINT) == -1) e(6);
  if (sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &oset) == -1) e(7);
  if (sigemptyset(&set) == -1) e(8);
  if (sigaddset(&set, SIGPIPE) == -1) e(9);
  if (set != oset) e(10);
}

/*---------------------------------------------------------------------------*/

/* Test the basic functionality of sigsuspend(2). */

void catch5(signo)
int signo;
{
  x = 1;
}

void test37f()
{
  sigset_t set;
  int r;
  struct sigaction act;
  pid_t pid;

  subtest = 6;
  clearsigstate();

  switch (pid = fork()) {
      case 0:			/* child */
	errct = 0;
	sleep(1);
	if (kill(getppid(), SIGINT) == -1) e(1);
	exit(errct == 0 ? 0 : 1);
      case -1:	e(2);	break;
      default:			/* parent */
	if (sigemptyset(&act.sa_mask) == -1) e(3);
	act.sa_flags = 0;
	act.sa_handler = catch5;
	if (sigaction(SIGINT, &act, (struct sigaction *) NULL) == -1) e(4);

	if (sigemptyset(&set) == -1) e(5);
	r = sigsuspend(&set);

	if (r != -1 || errno != EINTR || x != 1) e(6);
	wait_for(pid);
	break;
  }
}

/*----------------------------------------------------------------------*/

/* Test that sigsuspend() does block the signals specified in its
* argument, and after sigsuspend returns, the previous signal
* mask is restored.
*
* The child sends two signals to the parent SIGINT and then SIGPIPE,
* separated by a long delay.  The parent executes sigsuspend() with
* SIGINT blocked.  It is expected that the parent's SIGPIPE handler
* will be invoked, then sigsuspend will return restoring the
* original signal mask, and then the SIGPIPE handler will be
* invoked.
*/

void sigint_handler(signo)
int signo;
{
  x = 1;
  z++;
}

void sigpipe_handler(signo)
int signo;
{
  x = 2;
  z++;
}

void test37g()
{
  sigset_t set;
  int r;
  struct sigaction act;
  pid_t pid;

  subtest = 7;
  clearsigstate();
  x = 0;
  z = 0;

  switch (pid = fork()) {
      case 0:			/* child */
	errct = 0;
	sleep(1);
	if (kill(getppid(), SIGINT) == -1) e(1);
	sleep(1);
	if (kill(getppid(), SIGPIPE) == -1) e(2);
	exit(errct == 0 ? 0 : 1);
      case -1:	e(3);	break;
      default:			/* parent */
	if (sigemptyset(&act.sa_mask) == -1) e(3);
	act.sa_flags = 0;
	act.sa_handler = sigint_handler;
	if (sigaction(SIGINT, &act, (struct sigaction *) NULL) == -1) e(4);

	act.sa_handler = sigpipe_handler;
	if (sigaction(SIGPIPE, &act, (struct sigaction *) NULL) == -1) e(5);

	if (sigemptyset(&set) == -1) e(6);
	if (sigaddset(&set, SIGINT) == -1) e(7);
	r = sigsuspend(&set);
	if (r != -1) e(8);
	if (errno != EINTR) e(9);
	if (z != 2) e(10);
	if (x != 1) e(11);
	wait_for(pid);
	break;
  }
}

/*--------------------------------------------------------------------------*/

/* Test that sigsuspend() does block the signals specified in its
* argument, and after sigsuspend returns, the previous signal
* mask is restored.
*
* The child sends three signals to the parent: SIGHUP, then SIGPIPE,
* and then SIGTERM, separated by a long delay.  The parent executes
* sigsuspend() with SIGHUP and SIGPIPE blocked.  It is expected that
* the parent's SIGTERM handler will be invoked first, then sigsuspend()
* will return restoring the original signal mask, and then the other
* two handlers will be invoked.
*/

void sighup8(signo)
int signo;
{
  x = 1;
  z++;
}

void sigpip8(signo)
int signo;
{
  x = 1;
  z++;
}

void sigter8(signo)
int signo;
{
  x = 2;
  z++;
}

void test37h()
{
  sigset_t set;
  int r;
  struct sigaction act;
  pid_t pid;

  subtest = 8;
  clearsigstate();
  x = 0;
  z = 0;

  switch (pid = fork()) {
      case 0:			/* child */
	errct = 0;
	sleep(1);
	if (kill(getppid(), SIGHUP) == -1) e(1);
	sleep(1);
	if (kill(getppid(), SIGPIPE) == -1) e(2);
	sleep(1);
	if (kill(getppid(), SIGTERM) == -1) e(3);
	exit(errct == 0 ? 0 : 1);
      case -1:	e(5);	break;
      default:			/* parent */
	if (sigemptyset(&act.sa_mask) == -1) e(6);
	act.sa_flags = 0;
	act.sa_handler = sighup8;
	if (sigaction(SIGHUP, &act, (struct sigaction *) NULL) == -1) e(7);

	act.sa_handler = sigpip8;
	if (sigaction(SIGPIPE, &act, (struct sigaction *) NULL) == -1) e(8);

	act.sa_handler = sigter8;
	if (sigaction(SIGTERM, &act, (struct sigaction *) NULL) == -1) e(9);

	if (sigemptyset(&set) == -1) e(10);
	if (sigaddset(&set, SIGHUP) == -1) e(11);
	if (sigaddset(&set, SIGPIPE) == -1) e(12);
	r = sigsuspend(&set);
	if (r != -1) e(13);
	if (errno != EINTR) e(14);
	if (z != 3) e(15);
	if (x != 1) e(16);
	wait_for(pid);
	break;
  }
}

/*--------------------------------------------------------------------------*/

/* Block SIGHUP and SIGTERM with sigprocmask(), send ourself SIGHUP
* and SIGTERM, unblock these signals with sigprocmask, and verify
* that these signals are delivered.
*/

void sighup9(signo)
int signo;
{
  y++;
}

void sigter9(signo)
int signo;
{
  z++;
}

void test37i()
{
  sigset_t set;
  struct sigaction act;

  subtest = 9;
  clearsigstate();
  y = 0;
  z = 0;

  if (sigemptyset(&act.sa_mask) == -1) e(1);
  act.sa_flags = 0;

  act.sa_handler = sighup9;
  if (sigaction(SIGHUP, &act, (struct sigaction *) NULL) == -1) e(2);

  act.sa_handler = sigter9;
  if (sigaction(SIGTERM, &act, (struct sigaction *) NULL) == -1) e(3);

  if (sigemptyset(&set) == -1) e(4);
  if (sigaddset(&set, SIGTERM) == -1) e(5);
  if (sigaddset(&set, SIGHUP) == -1) e(6);
  if (sigprocmask(SIG_SETMASK, &set, (sigset_t *)NULL) == -1) e(7);

  if (kill(getpid(), SIGHUP) == -1) e(8);
  if (kill(getpid(), SIGTERM) == -1) e(9);
  if (y != 0) e(10);
  if (z != 0) e(11);

  if (sigemptyset(&set) == -1) e(12);
  if (sigprocmask(SIG_SETMASK, &set, (sigset_t *)NULL) == -1) e(12);
  if (y != 1) e(13);
  if (z != 1) e(14);
}

/*---------------------------------------------------------------------------*/

/* Block SIGINT and then send this signal to ourself.
*
* Install signal handlers for SIGALRM and SIGINT.
*
* Set an alarm for 6 seconds, then sleep for 7.
*
* The SIGALRM should interrupt the sleep, but the SIGINT
* should remain pending.
*/

void sighup10(signo)
int signo;
{
  y++;
}

void sigalrm_handler10(signo)
int signo;
{
  z++;
}

void test37j()
{
  sigset_t set, set2;
  struct sigaction act;

  subtest = 10;
  clearsigstate();
  y = 0;
  z = 0;

  if (sigemptyset(&act.sa_mask) == -1) e(1);
  act.sa_flags = 0;

  act.sa_handler = sighup10;
  if (sigaction(SIGHUP, &act, (struct sigaction *) NULL) == -1) e(2);

  act.sa_handler = sigalrm_handler10;
  if (sigaction(SIGALRM, &act, (struct sigaction *) NULL) == -1) e(3);

  if (sigemptyset(&set) == -1) e(4);
  if (sigaddset(&set, SIGHUP) == -1) e(5);
  if (sigprocmask(SIG_SETMASK, &set, (sigset_t *)NULL) == -1) e(6);

  if (kill(getpid(), SIGHUP) == -1) e(7);
  if (sigpending(&set) == -1) e(8);
  if (sigemptyset(&set2) == -1) e(9);
  if (sigaddset(&set2, SIGHUP) == -1) e(10);
  if (set2 != set) e(11);
  alarm(6);
  sleep(7);
  if (sigpending(&set) == -1) e(12);
  if (set != set2) e(13);
  if (y != 0) e(14);
  if (z != 1) e(15);
}

/*--------------------------------------------------------------------------*/

void test37k()
{
  subtest = 11;
}
void test37l()
{
  subtest = 12;
}

/*---------------------------------------------------------------------------*/

/* Basic test for setjmp/longjmp.  This includes testing that the
* signal mask is properly restored.
*/

#define TEST_SETJMP(_name, _subtest, _type, _setjmp, _longjmp, _save)	\
void _name(void)							\
{									\
  _type jb;								\
  sigset_t ss, ssexp;							\
									\
  subtest = _subtest;							\
  clearsigstate();							\
									\
  ss = 0x32;								\
  if (sigprocmask(SIG_SETMASK, &ss, (sigset_t *)NULL) == -1) e(1);	\
  if (_setjmp) {							\
	if (sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &ss) == -1) e(2);	\
	ssexp = _save ? 0x32 : 0x3abc;					\
	if ((ss ^ ssexp) & ~(1 << SIGKILL)) e(388);			\
	return;								\
  }									\
  ss = 0x3abc;								\
  if (sigprocmask(SIG_SETMASK, &ss, (sigset_t *)NULL) == -1) e(4);	\
  _longjmp;								\
}

TEST_SETJMP(test37m, 13, jmp_buf,    setjmp(jb),       longjmp(jb, 1),    1)
TEST_SETJMP(test37p, 16, sigjmp_buf, sigsetjmp(jb, 0), siglongjmp(jb, 1), 0)
TEST_SETJMP(test37q, 17, sigjmp_buf, sigsetjmp(jb, 1), siglongjmp(jb, 1), 1)

void longjerr()
{
  e(5);
}

/*--------------------------------------------------------------------------*/

/* Test for setjmp/longjmp.
*
* Catch a signal.  While in signal handler do setjmp/longjmp.
*/

void catch14(signo, code, scp)
int signo;
int code;
struct sigcontext *scp;
{
  jmp_buf jb;

  if (setjmp(jb)) {
	x++;
	sigreturn(scp);
	e(1);
  }
  y++;
  longjmp(jb, 1);
  e(2);
}

void test37n()
{
  struct sigaction act;
  typedef void(*sighandler_t) (int sig);

  subtest = 14;
  clearsigstate();
  x = 0;
  y = 0;

  act.sa_flags = 0;
  act.sa_mask = 0;
  act.sa_handler = (sighandler_t) catch14;	/* fudge */
  if (sigaction(SIGSEGV, &act, (struct sigaction *) NULL) == -1) e(3);
  if (kill(getpid(), SIGSEGV) == -1) e(4);

  if (x != 1) e(5);
  if (y != 1) e(6);
}

/*---------------------------------------------------------------------------*/

/* Test for setjmp/longjmp.
 *
 * Catch a signal.  Longjmp out of signal handler.
 */
jmp_buf glo_jb;

void catch15(signo)
int signo;
{
  z++;
  longjmp(glo_jb, 7);
  e(1);

}

void test37o()
{
  struct sigaction act;
  int k;

  subtest = 15;
  clearsigstate();
  z = 0;

  act.sa_flags = 0;
  act.sa_mask = 0;
  act.sa_handler = catch15;
  if (sigaction(SIGALRM, &act, (struct sigaction *) NULL) == -1) e(2);

  if ((k = setjmp(glo_jb))) {
	if (z != 1) e(399);
	if (k != 7) e(4);
	return;
  }
  if (kill(getpid(), SIGALRM) == -1) e(5);
}

void clearsigstate()
{
  int i;
  sigset_t sigset_var;

  /* Clear the signal state. */
  for (i = 1; i < _NSIG; i++) signal(i, SIG_IGN);
  for (i = 1; i < _NSIG; i++) signal(i, SIG_DFL);
  sigfillset(&sigset_var);
  sigprocmask(SIG_UNBLOCK, &sigset_var, (sigset_t *)NULL);
}

void wait_for(pid)
pid_t pid;
{
/* Expect exactly one child, and that it exits with 0. */

  int r;
  int status;

  errno = 0;
  while (1) {
	errno = 0;
	r = wait(&status);
	if (r == pid) {
		errno = 0;
		if (status != 0) e(90);
		return;
	}
	if (r < 0) {
		e(91);
		return;
	}
	e(92);
  }
}

