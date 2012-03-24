/* test 11 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define ITERATIONS 10
#define MAX_ERROR 1

int errct, subtest;
char *envp[3] = {"spring", "summer", 0};
char *passwd_file = "/etc/passwd";

#include "common.c"

int main(int argc, char *argv[]);
void test11a(void);
void test11b(void);
void test11c(void);
void test11d(void);

int main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  if (argc == 2) m = atoi(argv[1]);

  start(11);

  system("cp ../t11a .");
  system("cp ../t11b .");

  if (geteuid() != 0) {
	printf("must be setuid root; test aborted\n");
	exit(1);
  }
  if (getuid() == 0) {
       printf("must be setuid root logged in as someone else; test aborted\n");
       exit(1);
  }

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test11a();
	if (m & 0002) test11b();
	if (m & 0004) test11c();
	if (m & 0010) test11d();
  }
  quit();
  return(-1);
}

void test11a()
{
/* Test exec */
  int n, fd;
  char aa[4];

  subtest = 1;

  if (fork()) {
	wait(&n);
	if (n != 25600) e(1);
	unlink("t1");
	unlink("t2");
  } else {
	if (chown("t11a", 10, 20) < 0) e(2);
	chmod("t11a", 0666);

	/* The following call should fail because the mode has no X
	 * bits on. If a bug lets it unexpectedly succeed, the child
	 * will print an error message since the arguments are wrong.
 	 */
	execl("t11a", "t11a", (char *) 0); /* should fail -- no X bits */

	/* Control should come here after the failed execl(). */
	chmod("t11a", 06555);
	if ((fd = creat("t1", 0600)) != 3) e(3);
	if (close(fd) < 0) e(4);
	if (open("t1", O_RDWR) != 3) e(5);
	if (chown("t1", 10, 99) < 0) e(6);
	if ((fd = creat("t2", 0060)) != 4) e(7);
	if (close(fd) < 0) e(8);
	if (open("t2", O_RDWR) != 4) e(9);
	if (chown("t2", 99, 20) < 0) e(10);
	if (setgid(6) < 0) e(11);
	if (setuid(5) < 0) e(12);
	if (getuid() != 5) e(13);
	if (geteuid() != 5) e(14);
	if (getgid() != 6) e(15);
	if (getegid() != 6) e(16);
	aa[0] = 3;
	aa[1] = 5;
	aa[2] = 7;
	aa[3] = 9;
	if (write(3, aa, 4) != 4) e(17);
	lseek(3, 2L, 0);
	execle("t11a", "t11a", "arg0", "arg1", "arg2", (char *) 0, envp);
	e(18);
	printf("Can't exec t11a\n");
	exit(3);
  }
}

void test11b()
{
  int n;
  char *argv[5];

  subtest = 2;
  if (fork()) {
	wait(&n);
	if (n != (75 << 8)) e(20);
  } else {
	/* Child tests execv. */
	argv[0] = "t11b";
	argv[1] = "abc";
	argv[2] = "defghi";
	argv[3] = "j";
	argv[4] = 0;
	execv("t11b", argv);
	e(19);
  }
}

void test11c()
{
/* Test getlogin().  This test  MUST run setuid root. */

  int n, etc_uid;
  uid_t ruid, euid;
  char *lnamep, *p;
#define MAXLINELEN 200
  char array[MAXLINELEN], save[L_cuserid];
  FILE *stream;

  subtest = 3;
  errno = -2000;		/* None of these calls set errno. */
  save[0] = '#';
  save[1] = '0';
  ruid = getuid();
  euid = geteuid();
  lnamep = getlogin();
  strcpy(save, lnamep);

  /* Because we are setuid, login != 'root' */
  if (euid != 0) e(1);
  if (ruid == 0) e(2);
  if ( (n = strlen(save)) == 0) e(5);

  /* Check login against passwd file. First lookup login in /etc/passwd. */
  if (n == 0) return;		/* if login not found, don't look it up */
  if ( (stream = fopen(passwd_file, "r")) == NULL) e(8);
  while (fgets(array, sizeof(array), stream) != NULL) {
	if (strncmp(array, save, n) == 0) {
		p = &array[0];		/* hunt for uid */
		while (*p != ':') p++;
		p++;
		while (*p != ':') p++;
		p++;			/* p now points to uid */
		etc_uid = atoi(p);
		if (etc_uid != ruid) e(9);
		break;			/* 1 entry per login please */
	}
  }
  fclose(stream);
}

void test11d()
{
  int fd;
  struct stat statbuf;

  subtest = 4;
  fd = creat("T11.1", 0750);
  if (fd < 0) e(1);
  if (chown("T11.1", 8, 1) != 0) e(2);
  if (chmod("T11.1", 0666) != 0) e(3);
  if (stat("T11.1", &statbuf) != 0) e(4);
  if ((statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != 0666) e(5);
  if (close(fd) != 0) e(6);
  if (unlink("T11.1") != 0) e(7);
}

