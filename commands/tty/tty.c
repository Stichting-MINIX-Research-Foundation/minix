/* tty.c - Return tty name		Author: Freeman P. Pascal IV */

/* Minor changes to make tty conform to POSIX1003.2 Draft10
   Thomas Brupbacher (tobr@mw.lpc.ethz.ch)			*/

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv);

int main(argc, argv)
int argc;
char *argv[];
{
  char *tty_name;

  tty_name = ttyname(STDIN_FILENO);
  if ((argc == 2) && (!strcmp(argv[1], "-s")))
	 /* Do nothing - shhh! we're in silent mode */ ;
  else
	puts((tty_name != NULL) ? tty_name : "not a tty");

  if (isatty(STDIN_FILENO) == 0)
	return(1);
  else
	return(0);
}
