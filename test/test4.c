/* test 4 */

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

pid_t pid0, pid1, pid2, pid3;
int s, i, fd, nextb;
char *tempfile = "test4.temp";
char buf[1024];

int max_error = 2;
#include "common.h"



int main(void);
void subr(void);
void nofork(void);
void quit(void);

int main()
{
  int k;

  start(4);

  creat(tempfile, 0777);
  for (k = 0; k < 20; k++) {
	subr();
  }
  unlink(tempfile);
  quit();
  return(-1);			/* impossible */
}

void subr()
{
  if ( (pid0 = fork()) != 0) {
	/* Parent 0 */
	if (pid0 < 0) nofork();
	if ( (pid1 = fork()) != 0) {
		/* Parent 1 */
		if (pid1 < 0) nofork();
		if ( (pid2 = fork()) != 0) {
			/* Parent 2 */
			if (pid2 < 0) nofork();
			if ( (pid3 = fork()) != 0) {
				/* Parent 3 */
				if (pid3 < 0) nofork();
				for (i = 0; i < 10000; i++);
				kill(pid2, 9);
				kill(pid1, 9);
				kill(pid0, 9);
				wait(&s);
				wait(&s);
				wait(&s);
				wait(&s);
			} else {
				fd = open(tempfile, O_RDONLY);
				lseek(fd, 20480L * nextb, 0);
				for (i = 0; i < 10; i++) read(fd, buf, 1024);
				nextb++;
				close(fd);
				exit(0);
			}
		} else {
			while (1) getpid();
		}
	} else {
		while (1) getpid();
	}
  } else {
	while (1) getpid();
  }
}

void nofork()
{
  int e = errno;
  printf("Fork failed: %s (%d)\n",strerror(e),e);
  exit(1);
}

