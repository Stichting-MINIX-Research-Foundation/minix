/* sleep - suspend a process for x sec		Author: Andy Tanenbaum */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <minix/minlib.h>

int main(int argc, char **argv);

int main(argc, argv)
int argc;
char *argv[];
{
  register int seconds;
  register char c;

  seconds = 0;

  if (argc != 2) {
	std_err("Usage: sleep time\n");
	exit(1);
  }
  while ((c = *(argv[1])++)) {
	if (c < '0' || c > '9') {
		std_err("sleep: bad arg\n");
		exit(1);
	}
	seconds = 10 * seconds + (c - '0');
  }

  /* Now sleep. */
  sleep(seconds);
  return(0);
}
