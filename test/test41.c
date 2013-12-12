/* Tests for getitimer(2)/setitimer(2) - by D.C. van Moolenbroek */
/* Warning: this test deals with (real and virtual) time, and, lacking a proper
 * point of reference, its correctness depends on circumstances like CPU speed
 * and system load. A succeeding test run says a lot - failure not so much. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <minix/config.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/syslimits.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#define ITERATIONS 3
int max_error = 4;
#include "common.h"



/* we have to keep in mind the millisecond values are rounded up */
#define UPPERUSEC(us) ((us)+(1000000/system_hz))
#define EQUSEC(l,r) \
  ((l) <= ((r) + (1000000/system_hz)) && (l) >= ((r) - (1000000/system_hz)))

#define FILLITIMER(it, vs, vu, is, iu) \
  (it).it_value.tv_sec = (vs); \
  (it).it_value.tv_usec = (vu); \
  (it).it_interval.tv_sec = (is); \
  (it).it_interval.tv_usec = (iu);

/* these two macros are not fully working for all possible values;
 * the tests only use values that the macros can deal with, though. */
#define EQITIMER(it, vs, vu, is, iu) \
  ((it).it_value.tv_sec == (vs) && EQUSEC((it).it_value.tv_usec,vu) && \
  (it).it_interval.tv_sec == (is) && (it).it_interval.tv_usec == (iu))

#define LEITIMER(it, vs, vu, is, iu) \
  ((it).it_value.tv_sec > 0 && ((it).it_value.tv_sec < (vs) || \
  ((it).it_value.tv_sec == (vs) && (it).it_value.tv_usec <= \
   UPPERUSEC(vu))) && \
  (it).it_interval.tv_sec == (is) && EQUSEC((it).it_interval.tv_usec,iu))

int main(int argc, char **argv);
void test(int m, int t);
void test_which(void);
void test_getset(void);
void test_neglarge(void);
void test_zero(void);
void test_timer(void);
void test_alarm(void);
void test_fork(void);
void test_exec(void);
int do_check(void);
void got_alarm(int sig);
void busy_wait(int secs);
#define my_e(n) do { printf("Timer %s, ", names[timer]); e(n); } while(0)

static char *executable;
static int signals;
static int timer;
static long system_hz;

static int sigs[] = { SIGALRM, SIGVTALRM, SIGPROF };
static const char *names[] = { "REAL", "VIRTUAL", "PROF" };

int main(argc, argv)
int argc;
char **argv;
{
  int i, m = 0xFFFF, n = 0xF;
  char cp_cmd[NAME_MAX+10];

  system_hz = sysconf(_SC_CLK_TCK);

  if (strcmp(argv[0], "DO CHECK") == 0) {
  	timer = atoi(argv[1]);

	exit(do_check());
  }

  executable = argv[0];

  start(41);

  snprintf(cp_cmd, sizeof(cp_cmd), "cp ../%s .", executable);
  system(cp_cmd);

  if (argc >= 2) m = atoi(argv[1]);
  if (argc >= 3) n = atoi(argv[2]);

  for (i = 0; i < ITERATIONS; i++) {
  	if (n & 1) test(m, ITIMER_REAL);
  	if (n & 2) test(m, ITIMER_VIRTUAL);
  	if (n & 4) test(m, ITIMER_PROF);
  }

  quit();
  return(-1);			/* impossible */
}

void test(m, t)
int m;
int t;
{
  timer = t;

  if (m & 0001) test_which();
  if (m & 0002) test_getset();
  if (m & 0004) test_neglarge();
  if (m & 0010) test_zero();
  if (m & 0020) test_timer();
  if (m & 0040) test_alarm();
  if (m & 0100) test_fork();
  if (m & 0200) test_exec();
}

/* test invalid and unsupported 'which' values */
void test_which()
{
  struct itimerval it;

  subtest = 0;

  errno = 0; if (!getitimer(-1, &it) || errno != EINVAL) my_e(1);
  errno = 0; if ( getitimer(timer, &it)                ) my_e(2);
  errno = 0; if (!getitimer( 3, &it) || errno != EINVAL) my_e(3);
}

/* test if we get back what we set */
void test_getset()
{
  struct itimerval it, oit;

  subtest = 1;

  /* no alarm should be set initially */
  if (getitimer(timer, &it)) my_e(1);
  if (!EQITIMER(it, 0, 0, 0, 0)) my_e(2);

  if (setitimer(timer, &it, &oit)) my_e(3);
  if (setitimer(timer, &oit, NULL)) my_e(4);
  if (!EQITIMER(oit, 0, 0, 0, 0)) my_e(5);

  FILLITIMER(it, 123, 0, 456, 0);
  if (setitimer(timer, &it, NULL)) my_e(6);

  FILLITIMER(it, 987, 0, 654, 0);
  if (setitimer(timer, &it, &oit)) my_e(7);
  if (!LEITIMER(oit, 123, 0, 456, 0)) my_e(8);

  if (getitimer(timer, &oit)) my_e(9);
  if (!LEITIMER(oit, 987, 0, 654, 0)) my_e(10);

  FILLITIMER(it, 0, 0, 0, 0);
  if (setitimer(timer, &it, &oit)) my_e(11);
  if (!LEITIMER(oit, 987, 0, 654, 0)) my_e(12);

  if (getitimer(timer, &oit)) my_e(13);
  if (!EQITIMER(oit, 0, 0, 0, 0)) my_e(14);
}

/* test negative/large values */
void test_neglarge()
{
  struct itimerval it;

  subtest = 2;

  FILLITIMER(it, 4, 0, 5, 0);
  if (setitimer(timer, &it, NULL)) my_e(1);

  FILLITIMER(it, 1000000000, 0, 0, 0);
  if (!setitimer(timer, &it, NULL) || errno != EINVAL) my_e(2);

  FILLITIMER(it, 0, 1000000, 0, 0);
  if (!setitimer(timer, &it, NULL) || errno != EINVAL) my_e(3);

  FILLITIMER(it, 0, 0, 0, 1000000);
  if (!setitimer(timer, &it, NULL) || errno != EINVAL) my_e(4);

  FILLITIMER(it, -1, 0, 0, 0);
  if (!setitimer(timer, &it, NULL) || errno != EINVAL) my_e(5);

  FILLITIMER(it, 0, -1, 0, 0);
  if (!setitimer(timer, &it, NULL) || errno != EINVAL) my_e(6);

  FILLITIMER(it, 0, 0, -1, 0);
  if (!setitimer(timer, &it, NULL) || errno != EINVAL) my_e(7);

  FILLITIMER(it, 0, 0, 0, -1);
  if (!setitimer(timer, &it, NULL) || errno != EINVAL) my_e(8);

  if (getitimer(timer, &it)) my_e(9);
  if (!LEITIMER(it, 4, 0, 5, 0)) my_e(10);
}

/* setitimer with a zero timer has to set the interval to zero as well */
void test_zero()
{
  struct itimerval it;

  subtest = 3;

  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = 0;
  it.it_interval.tv_sec = 1;
  it.it_interval.tv_usec = 1;

  if (setitimer(timer, &it, NULL)) my_e(1);
  if (getitimer(timer, &it)) my_e(2);
  if (!EQITIMER(it, 0, 0, 0, 0)) my_e(3);
}

/* test actual timer functioning */
void test_timer()
{
  struct itimerval it;

  subtest = 4;

  if (signal(sigs[timer], got_alarm) == SIG_ERR) my_e(1);

  FILLITIMER(it, 0, 100000, 0, 100000);

  if (setitimer(timer, &it, NULL)) my_e(2);

  signals = 0;
  busy_wait(1);

  FILLITIMER(it, 0, 0, 0, 0);
  if (setitimer(timer, &it, NULL)) my_e(3);

  /* we don't know how many signals we'll actually get in practice,
   * so these checks more or less cover the extremes of the acceptable */
  if (signals < 2) my_e(4);
  if (signals > system_hz * 2) my_e(5);

  /* only for REAL timer can we check against the clock */
  if (timer == ITIMER_REAL) {
	FILLITIMER(it, 1, 0, 0, 0);
	if (setitimer(timer, &it, NULL)) my_e(6);

	signals = 0;
	busy_wait(1);

  	FILLITIMER(it, 0, 0, 0, 0);
  	if (setitimer(timer, &it, NULL)) my_e(7);

	if (signals != 1) my_e(8);
  }

  signals = 0;
  busy_wait(1);

  if (signals != 0) my_e(9);
}

/* test itimer/alarm interaction */
void test_alarm(void) {
  struct itimerval it;

  /* only applicable for ITIMER_REAL */
  if (timer != ITIMER_REAL) return;

  subtest = 5;

  if (signal(SIGALRM, got_alarm) == SIG_ERR) my_e(1);

  FILLITIMER(it, 3, 0, 1, 0);
  if (setitimer(timer, &it, NULL)) my_e(2);

  if (alarm(2) != 3) my_e(3);

  if (getitimer(timer, &it)) my_e(4);
  if (!LEITIMER(it, 2, 0, 0, 0)) my_e(5);

  signals = 0;
  busy_wait(5);

  if (signals != 1) my_e(6);

  if (getitimer(timer, &it)) my_e(7);
  if (!EQITIMER(it, 0, 0, 0, 0)) my_e(8);
}

/* test that the timer is reset on forking */
void test_fork(void) {
  struct itimerval it, oit;
  pid_t pid;
  int status;

  subtest = 6;

  FILLITIMER(it, 12, 34, 56, 78);

  if (setitimer(timer, &it, NULL)) my_e(1);

  pid = fork();
  if (pid < 0) my_e(2);

  if (pid == 0) {
    if (getitimer(timer, &it)) exit(5);
    if (!EQITIMER(it, 0, 0, 0, 0)) exit(6);

    exit(0);
  }

  if (wait(&status) != pid) my_e(3);
  if (!WIFEXITED(status)) my_e(4);
  if (WEXITSTATUS(status) != 0) my_e(WEXITSTATUS(status));

  FILLITIMER(it, 0, 0, 0, 0);
  if (setitimer(timer, &it, &oit)) my_e(7);
  if (!LEITIMER(oit, 12, 34, 56, 78)) my_e(8);
}

/* test if timer is carried over to exec()'ed process */
void test_exec(void) {
  struct itimerval it;
  pid_t pid;
  int status;
  char buf[2];

  subtest = 7;

  pid = fork();
  if (pid < 0) my_e(1);

  if (pid == 0) {
    FILLITIMER(it, 3, 0, 1, 0);
    if (setitimer(timer, &it, NULL)) exit(2);

    sprintf(buf, "%d", timer);
    execl(executable, "DO CHECK", buf, NULL);

    exit(3);
  }

  if (wait(&status) != pid) my_e(4);
  if (WIFSIGNALED(status)) {
    /* process should have died from corresponding signal */
    if (WTERMSIG(status) != sigs[timer]) my_e(5);
  }
  else {
    if (WIFEXITED(status)) my_e(WEXITSTATUS(status));
    else my_e(6);
  }
}

/* procedure of the exec()'ed process */
int do_check()
{
  struct itimerval it;

  if (getitimer(timer, &it)) return(81);
  if (!LEITIMER(it, 3, 0, 1, 0)) return(82);

  busy_wait(60);

  return(83);
}

void busy_wait(secs)
int secs;
{
  time_t now, exp;
  int i;

  exp = time(&now) + secs + 1;

  while (now < exp) {
  	for (i = 0; i < 100000; i++);

	time(&now);
  }
}

void got_alarm(sig)
int sig;
{
  if (sig != sigs[timer]) my_e(1001);

  signals++;
}

