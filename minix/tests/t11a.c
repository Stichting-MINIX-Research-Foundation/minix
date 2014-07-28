/* t11a */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_ERROR 4

int errct, subtest=1;

int main(int argc, char *argv [], char *envp []);
int diff(char *s1, char *s2);
void e(int n);

int main(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
/* See if arguments passed ok. */

  char aa[4];

  if (diff(argv[0], "t11a")) e(21);
  if (diff(argv[1], "arg0")) e(22);
  if (diff(argv[2], "arg1")) e(23);
  if (diff(argv[3], "arg2")) e(24);
  if (diff(envp[0], "spring")) e(25);
  if (diff(envp[1], "summer")) e(26);
  if (argc != 4) e(27);

  /* Now see if the files are ok. */
  if (read(3, aa, 4) != 2) e(28);
  if (aa[0] != 7 || aa[1] != 9) e(29);

  if (getuid() == 10) e(30);
  if (geteuid() != 10) e(31);
  if (getgid() == 20) e(32);
  if (getegid() != 20) e(33);

  if (open("t1", 0) < 0) e(34);
  if (open("t2", 0) < 0) e(35);
  exit(100);
}

int diff(s1, s2)
char *s1, *s2;
{
  while (1) {
	if (*s1 == 0 && *s2 == 0) return(0);
	if (*s1 != *s2) return (1);
	s1++;
	s2++;
  }
}

void e(n)
int n;
{
  printf("Subtest %d,  error %d  errno=%d  ", subtest, n, errno);
  perror("");
  if (errct++ > MAX_ERROR) {
	printf("Too many errors; test aborted\n");
	chdir("..");
	system("rm -rf DIR*");
	exit(1);
  }
}
