/* POSIX test program (7).			Author: Andy Tanenbaum */

/* The following POSIX calls are tested:
 *	pipe(), mkfifo(), fcntl()
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <setjmp.h>

#define ITERATIONS        4
#define MAX_ERROR 3
#define ITEMS  32
#define READ   10
#define WRITE  20
#define UNLOCK 30
#define U      70
#define L      80

char buf[ITEMS] = {0,1,2,3,4,5,6,7,8,9,8,7,6,5,4,3,2,1,0,1,2,3,4,5,6,7,8,9};

#include "common.c"

int subtes, xfd;
int whence = SEEK_SET, func_code = F_SETLK;
extern char **environ;

#define timed_test(func) (timed_test_func(#func, func));

int main(int argc, char *argv []);
void timed_test_func(const char *s, void (* func)(void));
void timed_test_timeout(int signum);
void test7a(void);
void test7b(void);
void test7c(void);
void test7d(void);
void test7e(void);
void test7f(void);
void test7g(void);
void test7h(void);
void test7i(void);
void test7j(void);
void cloexec_test(void);
int set(int how, int first, int last);
int locked(int b);
void sigfunc(int s);

int main(argc, argv)
int argc;
char *argv[];
{

  int i, m = 0xFFFF;

  if (argc == 2) m = atoi(argv[1]);
  if (m == 0) cloexec_test();	/* important; do not remove this! */

  start(7);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 00001) timed_test(test7a);
	if (m & 00002) timed_test(test7b);
	if (m & 00004) timed_test(test7c);
	if (m & 00010) timed_test(test7d);
	if (m & 00020) timed_test(test7e);
	if (m & 00040) timed_test(test7f);
	if (m & 00100) timed_test(test7g);
	if (m & 00200) timed_test(test7h);
	if (m & 00400) timed_test(test7i);
	if (m & 01000) timed_test(test7j);
  }
  quit();
  return(-1);			/* impossible */
}

static jmp_buf timed_test_context;

void timed_test_timeout(int signum)
{
  longjmp(timed_test_context, -1);
  e(700);
  quit();
  exit(-1);
}

void timed_test_func(const char *s, void (* func)(void))
{
  if (setjmp(timed_test_context) == 0)
  {
    /* the function gets 60 seconds to complete */
    if (signal(SIGALRM, timed_test_timeout) == SIG_ERR) { e(701); return; }
    alarm(60);
    func();
    alarm(0);
  }
  else
  {
    /* report timeout as error */
    printf("timeout in %s\n", s);
    e(702);
  }
}

void test7a()
{
/* Test pipe(). */

  int i, fd[2], ect;
  char buf2[ITEMS+1];

  /* Create a pipe, write on it, and read it back. */
  subtest = 1;
  if (pipe(fd) != 0) e(1);
  if (write(fd[1], buf, ITEMS) != ITEMS) e(2);
  buf2[0] = 0;
  if (read(fd[0], buf2, ITEMS+1) != ITEMS) e(3);
  ect = 0;
  for (i = 0; i < ITEMS; i++) if (buf[i] != buf2[i]) ect++;
  if (ect != 0) e(4);
  if (close(fd[0]) != 0) e(5);
  if (close(fd[1]) != 0) e(6);

  /* Error test.  Keep opening pipes until it fails.  Check error code. */
  errno = 0;
  while (1) {
	if (pipe(fd) < 0) break;
  }
  if (errno != EMFILE) e(7);

  /* Close all the pipes. */
  for (i = 3; i < OPEN_MAX; i++) close(i);
}

void test7b()
{
/* Test mkfifo(). */

  int fdr, fdw, status;
  char buf2[ITEMS+1];
  int efork;

  /* Create a fifo, write on it, and read it back. */
  subtest = 2;
  if (mkfifo("T7.b", 0777) != 0) e(1);
  switch (fork()) {
  case -1:
  	efork = errno;
  	printf("Fork failed: %s (%d)\n", strerror(efork), efork);
  	exit(1);
  case 0:
	/* Child reads from the fifo. */
	if ( (fdr = open("T7.b", O_RDONLY)) < 0) e(5);
	if (read(fdr, buf2, ITEMS+1) != ITEMS) e(6);
	if (strcmp(buf, buf2) != 0) e(7);
	if (close(fdr) != 0) e(8);
	exit(0);
  default:
	/* Parent writes on the fifo. */
	if ( (fdw = open("T7.b", O_WRONLY)) < 0) e(2);
	if (write(fdw, buf, ITEMS) != ITEMS) e(3);
	wait(&status);
	if (close(fdw) != 0) e(4);
  }

  /* Check some error conditions. */
  if (mkfifo("T7.b", 0777) != -1) e(9);
  errno = 0;
  if (mkfifo("a/b/c", 0777) != -1) e(10);
  if (errno != ENOENT) e(11);
  errno = 0;
  if (mkfifo("", 0777) != -1) e(12);
  if (errno != ENOENT) e(13);
  errno = 0;
  if (mkfifo("T7.b/x", 0777) != -1) e(14);
  if (errno != ENOTDIR) e(15);
  if (unlink("T7.b") != 0) e(16);

  /* Now check fifos and the O_NONBLOCK flag. */
  if (mkfifo("T7.b", 0600) != 0) e(17);
  errno = 0;
  if (open("T7.b", O_WRONLY | O_NONBLOCK) != -1) e(18);
  if (errno != ENXIO) e(19);
  if ( (fdr = open("T7.b", O_RDONLY | O_NONBLOCK)) < 0) e(20);
  if (fork()) {
	/* Parent reads from fdr. */
	wait(&status);		/* but first make sure writer has already run*/
	if ( ( (status>>8) & 0377) != 77) e(21);
	if (read(fdr, buf2, ITEMS+1) != ITEMS) e(22);
	if (strcmp(buf, buf2) != 0) e(23);
	if (close(fdr) != 0) e(24);
  } else {
	/* Child opens the fifo for writing and writes to it. */
	if ( (fdw = open("T7.b", O_WRONLY | O_NONBLOCK)) < 0) e(25);
	if (write(fdw, buf, ITEMS) != ITEMS) e(26);
	if (close(fdw) != 0) e(27);
	exit(77);
  }
  
  if (unlink("T7.b") != 0) e(28);
}

void test7c()
{
/* Test fcntl(). */

  int fd, m, s, newfd, newfd2;
  struct stat stat1, stat2, stat3;

  subtest = 3;
  errno = -100;
  if ( (fd = creat("T7.c", 0777)) < 0) e(1);

  /* Turn the per-file-descriptor flags on and off. */
  if (fcntl(fd, F_GETFD) != 0) e(2);	/* FD_CLOEXEC is initially off */
  if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) e(3);/* turn it on */
  if (fcntl(fd, F_GETFD) != FD_CLOEXEC) e(4);	/* should be on now */
  if (fcntl(fd, F_SETFD, 0) != 0) e(5);		/* turn it off */
  if (fcntl(fd, F_GETFD) != 0) e(6);		/* should be off now */

  /* Turn the open-file-description flags on and off. Start with O_APPEND. */
  m = O_WRONLY;
  if (fcntl(fd, F_GETFL) != m) e(7);	/* O_APPEND, O_NONBLOCK are off */
  if (fcntl(fd, F_SETFL, O_APPEND) != 0) e(8);	 /* turn on O_APPEND */
  if (fcntl(fd, F_GETFL) != (O_APPEND | m)) e(9);/* should be on now */
  if (fcntl(fd, F_SETFL, 0) != 0) e(10);	 /* turn it off */
  if (fcntl(fd, F_GETFL) != m) e(11);		 /* should be off now */
  
  /* Turn the open-file-description flags on and off. Now try O_NONBLOCK.  */
  if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0) e(12);      /* turn on O_NONBLOCK */
  if (fcntl(fd, F_GETFL) != (O_NONBLOCK | m)) e(13);   /* should be on now */
  if (fcntl(fd, F_SETFL, 0) != 0) e(14);	       /* turn it off */
  if (fcntl(fd, F_GETFL) != m) e(15);		       /* should be off now */
  
  /* Now both at once. */
  if (fcntl(fd, F_SETFL, O_APPEND|O_NONBLOCK) != 0) e(16);
  if (fcntl(fd, F_GETFL) != (O_NONBLOCK | O_APPEND | m)) e(17);
  if (fcntl(fd, F_SETFL, 0) != 0) e(18);
  if (fcntl(fd, F_GETFL) != m) e(19);

  /* Now test F_DUPFD. */
  if ( (newfd = fcntl(fd, F_DUPFD, 0)) != 4) e(20);	/* 0-4 open */
  if ( (newfd2 = fcntl(fd, F_DUPFD, 0)) != 5) e(21);	/* 0-5 open */
  if (close(newfd) != 0) e(22);				/* 0-3, 5 open */
  if ( (newfd = fcntl(fd, F_DUPFD, 0)) != 4) e(23);	/* 0-5 open */
  if (close(newfd) != 0) e(24);				/* 0-3, 5 open */
  if ( (newfd = fcntl(fd, F_DUPFD, 5)) != 6) e(25);	/* 0-3, 5, 6 open */
  if (close(newfd2) != 0) e(26);			/* 0-3, 6 open */
  
  /* O_APPEND should be inherited, but FD_CLOEXEC should be cleared.  Check. */
  if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) e(26);/* turn FD_CLOEXEC on */
  if (fcntl(fd, F_SETFL, O_APPEND) != 0) e(27);	 /* turn O_APPEND on */
  if ( (newfd2 = fcntl(fd, F_DUPFD, 10)) != 10) e(28);	/* 0-3, 6, 10 open */
  if (fcntl(newfd2, F_GETFD) != 0) e(29);	/* FD_CLOEXEC must be 0 */
  if (fcntl(newfd2, F_GETFL) != (O_APPEND | m)) e(30);	/* O_APPEND set */
  if (fcntl(fd, F_SETFD, 0) != 0) e(31);/* turn FD_CLOEXEC off */

  /* Check if newfd and newfd2 are the same inode. */
  if (fstat(fd, &stat1) != 0) e(32);
  if (fstat(fd, &stat2) != 0) e(33);
  if (fstat(fd, &stat3) != 0) e(34);
  if (stat1.st_dev != stat2.st_dev) e(35);
  if (stat1.st_dev != stat3.st_dev) e(36);
  if (stat1.st_ino != stat2.st_ino) e(37);
  if (stat1.st_ino != stat3.st_ino) e(38);

  /* Now check on the FD_CLOEXEC flag.  Set it for fd (3) and newfd2 (10) */
  if (fd != 3 || newfd2 != 10 || newfd != 6) e(39);
  if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) e(40);	/* close 3 on exec */
  if (fcntl(newfd2, F_SETFD, FD_CLOEXEC) != 0) e(41);	/* close 10 on exec */
  if (fcntl(newfd, F_SETFD, 0) != 0) e(42);		/* don't close 6 */
  if (fork()) {
	wait(&s);		/* parent just waits */
	if (WEXITSTATUS(s) != 0) e(43);
  } else {
	execle("../test7", "test7", "0", (char *) 0, environ);
	exit(1);		/* the impossible never happens, right? */
  }
  
  /* Finally, close all the files. */
  if (fcntl(fd, F_SETFD, 0) != 0) e(44);	/* FD_CLOEXEC off */
  if (fcntl(newfd2, F_SETFD, 0) != 0) e(45);	/* FD_CLOEXEC off */
  if (close(fd) != 0) e(46);
  if (close(newfd) != 0) e(47);
  if (close(newfd2) != 0) e(48);
}

void test7d()
{
/* Test file locking. */

  subtest = 4;
 
  if ( (xfd = creat("T7.d", 0777)) != 3) e(1);
  close(xfd);
  if ( (xfd = open("T7.d", O_RDWR)) < 0) e(2);
  if (write(xfd, buf, ITEMS) != ITEMS) e(3);
  if (set(WRITE, 0, 3) != 0) e(4);
  if (set(WRITE, 5, 9) != 0) e(5);
  if (set(UNLOCK, 0, 3) != 0) e(6);
  if (set(UNLOCK, 4, 9) != 0) e(7);

  if (set(READ, 1, 4) != 0) e(8);
  if (set(READ, 4, 7) != 0) e(9);
  if (set(UNLOCK, 4, 7) != 0) e(10);
  if (set(UNLOCK, 1, 4) != 0) e(11);

  if (set(WRITE, 0, 3) != 0) e(12);
  if (set(WRITE, 5, 7) != 0) e(13);
  if (set(WRITE, 9 ,10) != 0) e(14);
  if (set(UNLOCK, 0, 4) != 0) e(15);
  if (set(UNLOCK, 0, 7) != 0) e(16);
  if (set(UNLOCK, 0, 2000) != 0) e(17);

  if (set(WRITE, 0, 3) != 0) e(18);
  if (set(WRITE, 5, 7) != 0) e(19);
  if (set(WRITE, 9 ,10) != 0) e(20);
  if (set(UNLOCK, 0, 100) != 0) e(21);

  if (set(WRITE, 0, 9) != 0) e(22);
  if (set(UNLOCK, 8, 9) != 0) e(23);
  if (set(UNLOCK, 0, 2) != 0) e(24);
  if (set(UNLOCK, 5, 5) != 0) e(25);
  if (set(UNLOCK, 4, 6) != 0) e(26);
  if (set(UNLOCK, 3, 3) != 0) e(27);
  if (set(UNLOCK, 7, 7) != 0) e(28);

  if (set(WRITE, 0, 10) != 0) e(29);
  if (set(UNLOCK, 0, 1000) != 0) e(30);

  /* Up until now, all locks have been disjoint.  Now try conflicts. */
  if (set(WRITE, 0, 4) != 0) e(31);
  if (set(WRITE, 4, 7) != 0) e(32);	/* owner may lock same byte twice */
  if (set(WRITE, 5, 10) != 0) e(33);
  if (set(UNLOCK, 0, 11) != 0) e(34);

  /* File is now unlocked. Length 0 means whole file. */
  if (set(WRITE, 2, 1) != 0) e(35);	/* this locks whole file */
  if (set(WRITE, 9,10) != 0) e(36);	/* a process can relock its file */
  if (set(WRITE, 3, 3) != 0) e(37);
  if (set(UNLOCK, 0, -1) != 0) e(38);	/* file is now unlocked. */

  /* Test F_GETLK. */
  if (set(WRITE, 2, 3) != 0) e(39);
  if (locked(1) != U) e(40);
  if (locked(2) != L) e(41);
  if (locked(3) != L) e(42);
  if (locked(4) != U) e(43);
  if (set(UNLOCK, 2, 3) != 0) e(44);
  if (locked(2) != U) e(45);
  if (locked(3) != U) e(46);

  close(xfd);
}

void test7e()
{
/* Test to see if SETLKW blocks as it should. */

  int pid, s;

  subtest = 5;
 
  if ( (xfd = creat("T7.e", 0777)) != 3) e(1);
  if (close(xfd) != 0) e(2);
  if ( (xfd = open("T7.e", O_RDWR)) < 0) e(3);
  if (write(xfd, buf, ITEMS) != ITEMS) e(4);
  if (set(WRITE, 0, 3) != 0) e(5);

  if ( (pid = fork()) ) {
	/* Parent waits until child has started before signaling it. */
	while (access("T7.e1", 0) != 0) ;
	unlink("T7.e1");
	sleep(1);
	if (kill(pid, SIGKILL) < 0) e(6);
	if (wait(&s) != pid) e(7);
  } else {
	/* Child tries to lock and should block. */
	if (creat("T7.e1", 0777) < 0) e(8);
	func_code = F_SETLKW;
	if (set(WRITE, 0, 3) != 0) e(9);	/* should block */
	errno = -1000;
	e(10);			/* process should be killed by signal */
	exit(0);		/* should never happen */
  }
  close(xfd);
}

void test7f()
{
/* Test to see if SETLKW gives EINTR when interrupted. */

  int pid, s;

  subtest = 6;
 
  if ( (xfd = creat("T7.f", 0777)) != 3) e(1);
  if (close(xfd) != 0) e(2);
  if ( (xfd = open("T7.f", O_RDWR)) < 0) e(3);
  if (write(xfd, buf, ITEMS) != ITEMS) e(4);
  if (set(WRITE, 0, 3) != 0) e(5);

  if ( (pid = fork()) ) {
	/* Parent waits until child has started before signaling it. */
	while (access("T7.f1", 0) != 0) ;
	unlink("T7.f1");
	sleep(1);
	if (kill(pid, SIGTERM) < 0) e(6);
	if (wait(&s) != pid) e(7);
	if ( (s>>8) != 19) e(8);
  } else {
	/* Child tries to lock and should block.
	 * `signal(SIGTERM, sigfunc);' to set the signal handler is inadequate
	 * because on systems like BSD the sigaction flags for signal include
	 * `SA_RESTART' so syscalls are restarted after they have been
	 * interrupted by a signal.
	 */
	struct sigaction sa, osa;

	sa.sa_handler = sigfunc;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGTERM, &sa, &osa) < 0) e(999);
	if (creat("T7.f1", 0777) < 0) e(9);
	func_code = F_SETLKW;
	if (set(WRITE, 0, 3) != -1) e(10);	/* should block */
	if (errno != EINTR) e(11);	/* signal should release it */
	exit(19);
  }
  close(xfd);
}

void test7g()
{
/* Test to see if SETLKW unlocks when the needed lock becomes available. */

  int pid, s;

  subtest = 7;
 
  if ( (xfd = creat("T7.g", 0777)) != 3) e(1);
  if (close(xfd) != 0) e(2);
  if ( (xfd = open("T7.g", O_RDWR)) < 0) e(3);
  if (write(xfd, buf, ITEMS) != ITEMS) e(4);
  if (set(WRITE, 0, 3) != 0) e(5);	/* bytes 0 to 3 are now locked */

  if ( (pid = fork()) ) {
	/* Parent waits for child to start. */
	while (access("T7.g1", 0) != 0) ;
	unlink("T7.g1");
	sleep(1);
	if (set(UNLOCK, 0, 3) != 0) e(5);
	if (wait(&s) != pid) e(6);
	if ( (s >> 8) != 29) e(7);
  } else {
	/* Child tells parent it is alive, then tries to lock and is blocked.*/
	func_code = F_SETLKW;	
	if (creat("T7.g1", 0777) < 0) e(8);
	if (set(WRITE, 3, 3) != 0) e(9);	/* process must block now */
	if (set(UNLOCK, 3, 3) != 0) e(10);
	exit(29);
  }
  close(xfd);
}

void test7h()
{
/* Test to see what happens if two processed block on the same lock. */

  int pid, pid2, s, w;

  subtest = 8;
 
  if ( (xfd = creat("T7.h", 0777)) != 3) e(1);
  if (close(xfd) != 0) e(2);
  if ( (xfd = open("T7.h", O_RDWR)) < 0) e(3);
  if (write(xfd, buf, ITEMS) != ITEMS) e(4);
  if (set(WRITE, 0, 3) != 0) e(5);	/* bytes 0 to 3 are now locked */

  if ( (pid = fork()) ) {
	if ( (pid2 = fork()) ) {
		/* Parent waits for child to start. */
		while (access("T7.h1", 0) != 0) ;
		while (access("T7.h2", 0) != 0) ;
		unlink("T7.h1");
		unlink("T7.h2");
		sleep(1);
		if (set(UNLOCK, 0, 3) != 0) e(6);
		w = wait(&s);
		if (w != pid && w != pid2) e(7);
		s = s >> 8;
		if (s != 39 && s != 49) e(8);
		w = wait(&s);
		if (w != pid && w != pid2) e(9);
		s = s >> 8;
		if (s != 39 && s != 49) e(10);
	} else {
		func_code = F_SETLKW;	
		if (creat("T7.h1", 0777) < 0) e(11);
		if (set(WRITE, 0, 0) != 0) e(12);	/* block now */
		if (set(UNLOCK, 0, 0) != 0) e(13);
		exit(39);
	}
  } else {
	/* Child tells parent it is alive, then tries to lock and is blocked.*/
	func_code = F_SETLKW;	
	if (creat("T7.h2", 0777) < 0) e(14);
	if (set(WRITE, 0, 1) != 0) e(15);	/* process must block now */
	if (set(UNLOCK, 0, 1) != 0) e(16);
	exit(49);
  }
  close(xfd);
}

void test7i()
{
/* Check error conditions for fcntl(). */

  int tfd, i;

  subtest = 9;
 
  errno = 0;
  if ( (xfd = creat("T7.i", 0777)) != 3) e(1);
  if (close(xfd) != 0) e(2);
  if ( (xfd = open("T7.i", O_RDWR)) < 0) e(3);
  if (write(xfd, buf, ITEMS) != ITEMS) e(4);
  if (set(WRITE, 0, 3) != 0) e(5);	/* bytes 0 to 3 are now locked */
  if (set(WRITE, 0, 0) != 0) e(6);
  if (errno != 0) e(7);
  errno = 0;
  if (set(WRITE, 3, 3) != 0) e(8);
  if (errno != 0) e(9);
  tfd = xfd;			/* hold good value */
  xfd = -99;
  errno = 0;
  if (set(WRITE, 0, 0) != -1) e(10);
  if (errno != EBADF) e(11);

  errno = 0;  
  if ( (xfd = open("T7.i", O_WRONLY)) < 0) e(12);
  if (set(READ, 0, 0) != -1) e(13);
  if (errno != EBADF) e(14);
  if (close(xfd) != 0) e(15);

  errno = 0;  
  if ( (xfd = open("T7.i", O_RDONLY)) < 0) e(16);
  if (set(WRITE, 0, 0) != -1) e(17);
  if (errno != EBADF) e(18);
  if (close(xfd) != 0) e(19);
  xfd = tfd;			/* restore legal xfd value */

  /* Check for EINVAL. */
  errno = 0;
  if (fcntl(xfd, F_DUPFD, OPEN_MAX) != -1) e(20);
  if (errno != EINVAL) e(21);
  errno = 0;
  if (fcntl(xfd, F_DUPFD, -1) != -1) e(22);
  if (errno != EINVAL) e(23);

  xfd = 0;			/* stdin does not support locking */
  errno = 0;
  if (set(READ, 0, 0) != -1) e(24);
  if (errno != EINVAL) e(25);
  xfd = tfd;

  /* Check ENOLCK. */
  for (i = 0; i < ITEMS; i++) {
	if (set(WRITE, i, i) == 0) continue;
	if (errno != ENOLCK) {
		e(26);
		break;
	}
  }

  /* Check EMFILE. */
  for (i = xfd + 1; i < OPEN_MAX; i++) open("T7.i", 0);	/* use up all fds */
  errno = 0;
  if (fcntl(xfd, F_DUPFD, 0) != -1) e(27);	/* No fds left */
  if (errno != EMFILE) e(28);

  for (i = xfd; i < OPEN_MAX; i++) if (close(i) != 0) e(29);
}

void test7j()
{
/* Test file locking with two processes. */

  int s;

  subtest = 10;
 
  if ( (xfd = creat("T7.j", 0777)) != 3) e(1);
  close(xfd);
  if ( (xfd = open("T7.j", O_RDWR)) < 0) e(2);
  if (write(xfd, buf, ITEMS) != ITEMS) e(3);
  if (set(WRITE, 0, 4) != 0) e(4);	/* lock belongs to parent */
  if (set(READ, 10, 16) != 0) e(5);	/* lock belongs to parent */

  /* Up until now, all locks have been disjoint.  Now try conflicts. */
  if (fork()) {
	/* Parent just waits for child to finish. */
	wait(&s);
  } else {
	/* Child does the testing. */
	errno = -100;
	if (set(WRITE, 5, 7) < 0) e(6);	/* should work */
	if (set(WRITE, 4, 7) >= 0) e(7);	/* child may not lock byte 4 */
	if (errno != EACCES && errno != EAGAIN) e(8);
	if (set(WRITE, 5, 9) != 0) e(9);
	if (set(UNLOCK, 5, 9) != 0) e(10);
	if (set(READ, 9, 17) < 0) e(11);	/* shared READ lock is ok */
	exit(0);
  }
  close(xfd);
}

void cloexec_test()
{
/* To text whether the FD_CLOEXEC flag actually causes files to be
 * closed upon exec, we have to exec something.  The test is carried
 * out by forking, and then having the child exec test7 itself, but
 * with argument 0.  This is detected, and control comes here.
 * File descriptors 3 and 10 should be closed here, and 10 open.
 */

  if (close(3) == 0) e(1001);	/* close should fail; it was closed on exec */
  if (close(6) != 0) e(1002);	/* close should succeed */
  if (close(10) == 0) e(1003);	/* close should fail */
  fflush(stdout);
  exit(0);
}

int set(how, first, last)
int how, first, last;
{
  int r;
  struct flock flock;

  if (how == READ) flock.l_type = F_RDLCK;
  if (how == WRITE) flock.l_type = F_WRLCK;
  if (how == UNLOCK) flock.l_type = F_UNLCK;
  flock.l_whence = whence;
  flock.l_start = (long) first;
  flock.l_len = (long) last - (long) first + 1;
  r = fcntl(xfd, func_code, &flock);
  if (r != -1) 
	return(0);
  else
	return(-1);
}

int locked(b)
int b;
/* Test to see if byte b is locked.  Return L or U */
{
  struct flock flock;
  pid_t pid;
  int status;

  flock.l_type = F_WRLCK;
  flock.l_whence = whence;
  flock.l_start = (long) b;
  flock.l_len = 1;

  /* Process' own locks are invisible to F_GETLK, so fork a child to test. */
  pid = fork();
  if (pid == 0) {
	if (fcntl(xfd, F_GETLK, &flock) != 0) e(2000);
	exit(flock.l_type == F_UNLCK ? U : L);  
  }
  if (pid == -1) e(2001);
  if (fcntl(xfd, F_GETLK, &flock) != 0) e(2002);
  if (flock.l_type != F_UNLCK) e(2003);
  if (wait(&status) != pid) e(2004);
  if (!WIFEXITED(status)) e(2005);
  return(WEXITSTATUS(status));
}

void sigfunc(s)
int s;				/* for ANSI */
{
}

