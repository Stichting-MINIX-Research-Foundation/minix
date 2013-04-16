/* Test 14. unlinking an open file. */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define TRIALS 100
int max_error = 4;
#include "common.h"


char name[20] = {"TMP14."};
int subtest = 1;


int main(void);
void quit(void);

int main()
{
  int fd0, i, pid;

  start(14);

  pid = getpid();
  sprintf(&name[6], "%x", pid);

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

