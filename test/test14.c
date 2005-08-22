/* Test 14. unlinking an open file. */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define TRIALS 100
#define MAX_ERROR 4

char name[20] = {"TMP14."};
int errct;
int subtest = 1;

_PROTOTYPE(int main, (void));
_PROTOTYPE(void e, (int n));
_PROTOTYPE(void quit, (void));

int main()
{
  int fd0, i, pid;

  printf("Test 14 ");
  fflush(stdout);

  system("rm -rf DIR_14; mkdir DIR_14");
  chdir("DIR_14");

  pid = getpid();
  name[6] = (pid & 037) + 33;
  name[7] = ((pid * pid) & 037) + 33;
  name[8] = 0;

  for (i = 0; i < TRIALS; i++) {
	if ( (fd0 = creat(name, 0777)) < 0) e(1);
	if (write(fd0, name, 20) != 20) e(2);
	if (unlink(name) != 0) e(3);
	if (close(fd0) != 0) e(4);
  }

  fd0 = creat(name, 0777);
  write(fd0, name, 20);
  unlink(name);
  quit();
  return(-1);			/* impossible */
}

void e(n)
int n;
{
  int err_num = errno;		/* save errno in case printf clobbers it */

  printf("Subtest %d,  error %d  errno=%d  ", subtest, n, errno);
  errno = err_num;		/* restore errno, just in case */
  perror("");
  if (errct++ > MAX_ERROR) {
	printf("Too many errors; test aborted\n");
	chdir("..");
	system("rm -rf DIR*");
	exit(1);
  }
}

void quit()
{

  chdir("..");
  system("rm -rf DIR*");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
