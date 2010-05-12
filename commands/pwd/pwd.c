/* pwd - print working directory	Author: Norbert Schlenker */

/*
 * pwd - print working directory
 *   Syntax:	pwd
 *   Flags:	None.
 *   Author:	Norbert Schlenker
 *   Copyright:	None.  Released to the public domain.
 *   Reference:	IEEE P1003.2 Section 4.50 (draft 10)
 *   Bugs:	No internationalization support; all messages are in English.
 */

/* Force visible Posix names */
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE 1
#endif

/* External interfaces */
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

/* Magic numbers suggested or required by Posix specification */
#define SUCCESS	0		/* exit code in case of success */
#define FAILURE 1		/*                   or failure */

_PROTOTYPE(int main, (void));

static char dir[PATH_MAX + 1];
static char *errmsg = "pwd: cannot search some directory on the path\n";

int main()
{
  char *p;
  size_t n;

  p = getcwd(dir, PATH_MAX);
  if (p == NULL) {
	write(STDERR_FILENO, errmsg, strlen(errmsg));
	exit(FAILURE);
  }
  n = strlen(p);
  p[n] = '\n';
  if (write(STDOUT_FILENO, p, n + 1) != n + 1) 	exit(FAILURE);
  return(SUCCESS);
}
