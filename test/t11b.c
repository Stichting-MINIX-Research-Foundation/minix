/* t11b */

#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_ERROR 4

int errct, subtest=1;

int main(int argc, char *argv []);
int diff(char *s1, char *s2);
void e(int n);

int main(argc, argv)
int argc;
char *argv[];
{
/* See if arguments passed ok. */

  if (diff(argv[0], "t11b")) e(31);
  if (diff(argv[1], "abc")) e(32);
  if (diff(argv[2], "defghi")) e(33);
  if (diff(argv[3], "j")) e(34);
  if (argv[4] != 0) e(35);
  if (argc != 4) e(36);

  exit(75);
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
