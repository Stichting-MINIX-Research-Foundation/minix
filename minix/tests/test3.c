/* test 3 - library routines rather than system calls */

#include <sys/types.h>
#include <sys/utsname.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define ITERATIONS 10
int max_error = 0;
#include "common.h"

#define SIZE 64

int subtest;
char el_weirdo[] = "\n\t\\\e@@!!##\e\e\n\n";


int main(int argc, char *argv []);
void test3a(void);
void test3c(void);
void test3d(void);
void test3e(void);

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  start(3);
  if (argc == 2) m = atoi(argv[1]);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test3a();
	if (m & 0004) test3c();
	if (m & 0010) test3d();
	if (m & 0020) test3e();
  }
  quit();
  return(-1);			/* impossible */

}

void test3a()
{
/* Signal set manipulation. */

  sigset_t s, s1;

  subtest = 1;
  errno = -1000;		/* None of these calls set errno. */
  if (sigemptyset(&s) != 0) e(1);
  if (sigemptyset(&s1) != 0) e(2);
  if (sigaddset(&s, SIGABRT) != 0) e(3);
  if (sigaddset(&s, SIGALRM) != 0) e(4);
  if (sigaddset(&s, SIGFPE ) != 0) e(5);
  if (sigaddset(&s, SIGHUP ) != 0) e(6);
  if (sigaddset(&s, SIGILL ) != 0) e(7);
  if (sigaddset(&s, SIGINT ) != 0) e(8);
  if (sigaddset(&s, SIGKILL) != 0) e(9);
  if (sigaddset(&s, SIGPIPE) != 0) e(10);
  if (sigaddset(&s, SIGQUIT) != 0) e(11);
  if (sigaddset(&s, SIGSEGV) != 0) e(12);
  if (sigaddset(&s, SIGTERM) != 0) e(13);
  if (sigaddset(&s, SIGUSR1) != 0) e(14);
  if (sigaddset(&s, SIGUSR2) != 0) e(15);
  
  if (sigismember(&s, SIGABRT) != 1) e(16);
  if (sigismember(&s, SIGALRM) != 1) e(17);
  if (sigismember(&s, SIGFPE ) != 1) e(18);
  if (sigismember(&s, SIGHUP ) != 1) e(19);
  if (sigismember(&s, SIGILL ) != 1) e(20);
  if (sigismember(&s, SIGINT ) != 1) e(21);
  if (sigismember(&s, SIGKILL) != 1) e(22);
  if (sigismember(&s, SIGPIPE) != 1) e(23);
  if (sigismember(&s, SIGQUIT) != 1) e(24);
  if (sigismember(&s, SIGSEGV) != 1) e(25);
  if (sigismember(&s, SIGTERM) != 1) e(26);
  if (sigismember(&s, SIGUSR1) != 1) e(27);
  if (sigismember(&s, SIGUSR2) != 1) e(28);
  
  if (sigdelset(&s, SIGABRT) != 0) e(29);
  if (sigdelset(&s, SIGALRM) != 0) e(30);
  if (sigdelset(&s, SIGFPE ) != 0) e(31);
  if (sigdelset(&s, SIGHUP ) != 0) e(32);
  if (sigdelset(&s, SIGILL ) != 0) e(33);
  if (sigdelset(&s, SIGINT ) != 0) e(34);
  if (sigdelset(&s, SIGKILL) != 0) e(35);
  if (sigdelset(&s, SIGPIPE) != 0) e(36);
  if (sigdelset(&s, SIGQUIT) != 0) e(37);
  if (sigdelset(&s, SIGSEGV) != 0) e(38);
  if (sigdelset(&s, SIGTERM) != 0) e(39);
  if (sigdelset(&s, SIGUSR1) != 0) e(40);
  if (sigdelset(&s, SIGUSR2) != 0) e(41);
  
  if (memcmp(&s, &s1, sizeof(s))) e(42);

  if (sigaddset(&s, SIGILL) != 0) e(43);
  if (!memcmp(&s, &s1, sizeof(s))) e(42);

  if (sigfillset(&s) != 0) e(45);
  if (sigismember(&s, SIGABRT) != 1) e(46);
  if (sigismember(&s, SIGALRM) != 1) e(47);
  if (sigismember(&s, SIGFPE ) != 1) e(48);
  if (sigismember(&s, SIGHUP ) != 1) e(49);
  if (sigismember(&s, SIGILL ) != 1) e(50);
  if (sigismember(&s, SIGINT ) != 1) e(51);
  if (sigismember(&s, SIGKILL) != 1) e(52);
  if (sigismember(&s, SIGPIPE) != 1) e(53);
  if (sigismember(&s, SIGQUIT) != 1) e(54);
  if (sigismember(&s, SIGSEGV) != 1) e(55);
  if (sigismember(&s, SIGTERM) != 1) e(56);
  if (sigismember(&s, SIGUSR1) != 1) e(57);
  if (sigismember(&s, SIGUSR2) != 1) e(58);

  /* Test error returns. */
  if (sigaddset(&s, -1) != -1) e(59);
  if (sigaddset(&s, -1) != -1) e(60);
  if (sigismember(&s, -1) != -1) e(61);
  if (sigaddset(&s, 10000) != -1) e(62);
  if (sigaddset(&s, 10000) != -1) e(63);
  if (sigismember(&s, 10000) != -1) e(64);

}

void test3c()
{
/* Test getenv.  Asume HOME, PATH, and LOGNAME exist (not strictly required).*/

  char *p, name[SIZE];

  subtest = 3;
  errno = -3000;		/* None of these calls set errno. */
  if ( (p = getenv("HOME")) == NULL) { fprintf(stderr, "Please have $HOME set properly.\n"); e(1); }
  if (*p != '/') { fprintf(stderr, "Please have $HOME point to an abolute path.\n"); e(2); }		/* path must be absolute */
  if ( (p = getenv("PATH")) == NULL) { fprintf(stderr, "Please have $PATH set properly.\n"); e(3); }
  if ( (p = getenv("LOGNAME")) == NULL) { fprintf(stderr, "Please have $LOGNAME set properly.\n"); e(5); }
  strcpy(name, p);		/* save it, since getlogin might wipe it out */
  p = getlogin();
  if (strcmp(p, name) != 0) { fprintf(stderr, "Please have $LOGNAME set to your real username. (su - instead of su?)\n"); e(6); }

  /* The following test could fail in a legal POSIX system.  However, if it
   * does, you deserve it to fail.
   */
  if (getenv(el_weirdo) != NULL) e(7);
}

void test3d()
{
/* Test ctermid, ttyname, and isatty. */

  int fd;
  char *p, name[L_ctermid];

  subtest = 4;
  errno = -4000;		/* None of these calls set errno. */

  /* Test ctermid first. */
  if ( (p = ctermid(name)) == NULL) e(1);
  if (strcmp(p, name) != 0) e(2);
  if (strncmp(p, "/dev/tty", 8) != 0) e(3);	/* MINIX convention */
  
  if ( (p = ttyname(0)) == NULL) e(4);
  if (strncmp(p, "/dev/tty", 8) != 0 && strcmp(p, "/dev/console") != 0 &&
      strncmp(p, "/dev/pts/", 9) != 0) e(5);
  if ( (p = ttyname(3)) != NULL) e(6);
  if (ttyname(5000) != NULL) e(7);
  if ( (fd = creat("T3a", 0777)) < 0) e(8);
  if (ttyname(fd) != NULL) e(9);

  if (isatty(0) != 1) e(10);
  if (isatty(3) != 0) e(11);
  if (isatty(fd) != 0) e(12);
  if (close(fd) != 0) e(13);
  if (ttyname(fd) != NULL) e(14);
}

void test3e()
{
/* Test ctermid, ttyname, and isatty. */

  subtest = 5;
  errno = -5000;		/* None of these calls set errno. */

  if (sysconf(_SC_ARG_MAX) < _POSIX_ARG_MAX) e(1);
  if (sysconf(_SC_CHILD_MAX) < _POSIX_CHILD_MAX) e(2);
  if (sysconf(_SC_NGROUPS_MAX) < 0) e(3);
  if (sysconf(_SC_OPEN_MAX) < _POSIX_OPEN_MAX) e(4);

  /* The rest are MINIX specific */
  if (sysconf(_SC_JOB_CONTROL) >= 0) e(5);	/* no job control! */
}

